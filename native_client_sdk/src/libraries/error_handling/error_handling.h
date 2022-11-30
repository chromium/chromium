/*
 * Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_ERROR_HANDLING_ERROR_HANDLING_H_
#define LIBRARIES_ERROR_HANDLING_ERROR_HANDLING_H_

#include "error_handling/string_stream.h"
#include "sdk_util/macros.h"

EXTERN_C_BEGIN

struct NaClExceptionContext;

typedef void (*EHRawHandler)(struct NaClExceptionContext* context);
typedef void (*EHJsonHandler)(const char* str);

typedef struct {
  uint32_t prog_ctr;
  uint32_t frame_ptr;
  uint32_t next_ptr;
} EHFrame;


/** Initialize error handling.
 *
 * Initializes the error handling to catch untrusted exceptions.  The init
 * functions will install an untrusted exception handler.
 *
 * The raw form will install the provided handler directly to the exception
 * system.
 *
 * The json form will install a default handler which will walk the stack
 * creating a valid JSON string which is passed to the provided handler.
 *
 * NOTE: Exception handling is enabled process wide.
 * NOTE: Exception handling is not guaranteed to be available so it should
 * not be considered an error if the request fails.
 */
void EHRequestExceptionsRaw(EHRawHandler callback);
void EHRequestExceptionsJson(EHJsonHandler callback);


/** Request an alternate signal handling stack for this thread.
 *
 * Specifies an alternate stack which will be used when this thread enters
 * the exception handler.  This is useful in cases when the threads original
 * stack may have overflowed or may be too small to handler the exception.
 *
 * Returns the allocated stack or MAP_FAILED.
 *
 * NOTE: Unlike the handler, this is a per thread call.
 * NOTE: If the allocation fails, the exception will still take place on the
 * thread's original stack.
 */
void *EHRequestExceptionStackOnThread(size_t stack_size);


/** Determine if NaCl will forward exceptions.
 *
 * Returns non-zero if a hander has been installed and exceptions will
 * be forwarded.
 *
 * NOTE: Exception handling is not guaranteed to be available so it should
 * not be considered an error if the request fails.
 */
int EHHanderInstalled();


/** Fill an exception frame from an exception context. */
int EHGetTopFrame(sstream_t* ss, struct NaClExceptionContext* context,
                  EHFrame* frame);


/** Unwind the stack by one frame.
 *
 * Returns zero once it failes to unwind.
 */
int EHUnwindFrame(EHFrame* frame);

EXTERN_C_END

#endif  // LIBRARIES_ERROR_HANDLING_ERROR_HANDLING_H_

