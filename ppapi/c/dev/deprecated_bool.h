/* Copyright 2010 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_DEPRECATED_BOOL_H_
#define PPAPI_C_DEV_DEPRECATED_BOOL_H_

/**
 * @file
 * Defines the API ...
 *
 * @addtogroup PP
 * @{
 */
// TODO(ppapi authors):  Remove ppp_class_deprecated.h and ppb_var_deprecated.h
// and remove this file.  This is only here to ease the transition from
// deprecated interfaces to the new ones.  Add a usable definition of bool for
// C code.
#if !defined(__cplusplus)
# if defined(_MSC_VER) || !defined(__STDC_VERSION__) || \
    (__STDC_VERSION__ < 199901L)
// The Visual Studio C compiler and older versions of GCC do not support C99
// and thus have no bool or stdbool.h.  Make a simple definition of bool,
// true, and false to make this deprecated interface compile in C.  Force it
// to 1 byte to have some chance of ABI compatibility between C and C++, in
// case we don't remove this.
typedef char bool;
#  define false 0
#  define true 1
# else
// In C99-compliant compilers, we can include stdbool.h to get a bool
// definition.
#  include <stdbool.h>
# endif
#endif

/**
 * @}
 * End addtogroup PP
 */

#endif  // PPAPI_C_DEV_DEPRECATED_BOOL_H_

