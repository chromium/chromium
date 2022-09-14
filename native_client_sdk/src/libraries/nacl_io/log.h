/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_LOG_H_
#define LIBRARIES_NACL_IO_LOG_H_

#include "sdk_util/macros.h"

#define LOG_PREFIX "nacl_io: "

#if defined(NDEBUG)

#define LOG_TRACE(format, ...)
#define LOG_ERROR(format, ...)
#define LOG_WARN(format, ...)

#else

#if NACL_IO_LOGGING

#define LOG_TRACE(format, ...) \
  nacl_io_log(LOG_PREFIX format "\n", ##__VA_ARGS__)

#else

#define LOG_TRACE(format, ...)

#endif

#define LOG_ERROR(format, ...)                         \
  nacl_io_log(LOG_PREFIX "%s:%d: error: " format "\n", \
              __FILE__,                                \
              __LINE__,                                \
              ##__VA_ARGS__)

#define LOG_WARN(format, ...)                            \
  nacl_io_log(LOG_PREFIX "%s:%d: warning: " format "\n", \
              __FILE__,                                  \
              __LINE__,                                  \
              ##__VA_ARGS__)

#endif

EXTERN_C_BEGIN

/*
 * Low level logging function for nacl_io log messages.
 *
 * This function sends its output directly to the IRT standard out
 * file descriptor, which by default will apear on the standard out
 * or chrome or sel_ldr.
 */
void nacl_io_log(const char* format, ...) PRINTF_LIKE(1, 2);

EXTERN_C_END

#endif  // LIBRARIES_NACL_IO_LOG_H_
