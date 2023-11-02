/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_SDK_UTIL_MACROS_H_
#define LIBRARIES_SDK_UTIL_MACROS_H_

/**
 * A macro to disallow the evil copy constructor and operator= functions
 * This should be used in the private: declarations for a class.
 */
#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
#endif

/** returns the size of a member of a struct. */
#define MEMBER_SIZE(struct_name, member) sizeof(((struct_name*)0)->member)

/**
 * Macros to prevent name mangling of definitions, allowing them to be
 * referenced from C.
 */
#ifdef __cplusplus
# define EXTERN_C_BEGIN  extern "C" {
# define EXTERN_C_END    }
#else
# define EXTERN_C_BEGIN
# define EXTERN_C_END
#endif  /* __cplusplus */

/**
 * Macro to error out when a printf-like function is passed incorrect arguments.
 *
 * Use like this:
 * void foo(const char* fmt, ...) PRINTF_LIKE(1, 2);
 *
 * The first argument is the location of the fmt string (1-based).
 * The second argument is the location of the first argument to validate (also
 *   1-based, but can be zero if the function uses a va_list, like vprintf.)
 */
#if defined(__GNUC__)
#define PRINTF_LIKE(a, b) __attribute__ ((format(printf, a, b)))
#else
#define PRINTF_LIKE(a, b)
#endif

#endif  // LIBRARIES_SDK_UTIL_MACROS_H_
