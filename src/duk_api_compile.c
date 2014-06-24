/*
 *  Compilation and evaluation
 */

#include "duk_internal.h"

typedef struct duk__compile_raw_args duk__compile_raw_args;
struct duk__compile_raw_args {
	duk_size_t src_length;  /* should be first on 64-bit platforms */
	const duk_uint8_t *src_buffer;
	duk_int_t flags;
};

/* Eval is just a wrapper now. */
duk_int_t duk_eval_raw(duk_context *ctx, const char *user_buffer, duk_size_t user_length, duk_int_t flags) {
	duk_int_t comp_flags;
	duk_int_t rc;

	/* [ ... source filename ] */

	comp_flags = flags;
	comp_flags |= DUK_COMPILE_EVAL;
	if (duk_is_strict_call(ctx)) {
		comp_flags |= DUK_COMPILE_STRICT;
	}
	rc = duk_compile_raw(ctx, user_buffer, user_length, comp_flags);  /* may be safe, or non-safe depending on flags */

	/* [ ... closure/error ] */

	if (rc != DUK_EXEC_SUCCESS) {
		rc = DUK_EXEC_ERROR;
		goto got_rc;
	}

	if (flags & DUK_COMPILE_SAFE) {
		rc = duk_pcall(ctx, 0);
	} else {
		duk_call(ctx, 0);
		rc = DUK_EXEC_SUCCESS;
	}

	/* [ ... result/error ] */

 got_rc:
	if (flags & DUK_COMPILE_NORESULT) {
		duk_pop(ctx);
	}

	return rc;
}

/* Helper which can be called both directly and with duk_safe_call(). */
static duk_ret_t duk__do_compile(duk_context *ctx) {
	duk_hthread *thr = (duk_hthread *) ctx;
	duk__compile_raw_args *comp_args;
	duk_int_t flags;
	duk_small_int_t comp_flags;
	duk_hcompiledfunction *h_templ;

	/* [ ... source filename flags ] */

	comp_args = (duk__compile_raw_args *) duk_require_pointer(ctx, -1);
	flags = comp_args->flags;
	duk_pop(ctx);

	/* [ ... source filename ] */

	if (!comp_args->src_buffer) {
		duk_hstring *h_sourcecode = duk_require_hstring(ctx, -2);
		comp_args->src_buffer = (const duk_uint8_t *) DUK_HSTRING_GET_DATA(h_sourcecode);
		comp_args->src_length = (duk_size_t) DUK_HSTRING_GET_BYTELEN(h_sourcecode);
	}
	DUK_ASSERT(comp_args->src_buffer != NULL);

	/* XXX: unnecessary translation of flags */
	comp_flags = 0;
	if (flags & DUK_COMPILE_EVAL) {
		comp_flags |= DUK_JS_COMPILE_FLAG_EVAL;
	}
	if (flags & DUK_COMPILE_FUNCTION) {
		comp_flags |= DUK_JS_COMPILE_FLAG_EVAL |
		              DUK_JS_COMPILE_FLAG_FUNCEXPR;
	}
	if (flags & DUK_COMPILE_STRICT) {
		comp_flags |= DUK_JS_COMPILE_FLAG_STRICT;
	}

	/* [ ... source filename ] */

	duk_js_compile(thr, comp_args->src_buffer, comp_args->src_length, comp_flags);

	/* [ ... source func_template ] */

	h_templ = (duk_hcompiledfunction *) duk_get_hobject(ctx, -1);
	DUK_ASSERT(h_templ != NULL);
	duk_js_push_closure(thr,
	                   h_templ,
	                   thr->builtins[DUK_BIDX_GLOBAL_ENV],
	                   thr->builtins[DUK_BIDX_GLOBAL_ENV]);
	duk_replace(ctx, -3);  /* -> [ ... closure func_template ] */
	duk_pop(ctx);          /* -> [ ... closure ] */

	/* [ ... closure ] */

	return 1;
}

duk_int_t duk_compile_raw(duk_context *ctx, const char *user_buffer, duk_size_t user_length, duk_int_t flags) {
	duk__compile_raw_args comp_args_alloc;
	duk__compile_raw_args *comp_args = &comp_args_alloc;

	comp_args->src_buffer = (const duk_uint8_t *) user_buffer;
	comp_args->src_length = user_length;
	comp_args->flags = flags;
	duk_push_pointer(ctx, (void *) comp_args);

	/* [ ... source filename &comp_args ] */

	if (flags & DUK_COMPILE_SAFE) {
		duk_int_t rc = duk_safe_call(ctx, duk__do_compile, 3 /*nargs*/, 1 /*nrets*/);

		/* [ ... closure ] */
		return rc;
	}

	(void) duk__do_compile(ctx);

	/* [ ... closure ] */
	return DUK_EXEC_SUCCESS;
}
