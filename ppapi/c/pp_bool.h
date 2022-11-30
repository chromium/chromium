/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_bool.idl modified Thu Nov  1 13:48:33 2012. */

#ifndef PPAPI_C_PP_BOOL_H_
#define PPAPI_C_PP_BOOL_H_

#include "ppapi/c/pp_macros.h"

/**
 * @file
 * This file defines the <code>PP_Bool</code> enumeration for use in PPAPI C
 * headers.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * The <code>PP_Bool</code> enum is a boolean value for use in PPAPI C headers.
 * The standard bool type is not available to pre-C99 compilers, and is not
 * guaranteed to be compatible between C and C++, whereas the PPAPI C headers
 * can be included from C or C++ code.
 */
typedef enum {
  PP_FALSE = 0,
  PP_TRUE = 1
} PP_Bool;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_Bool, 4);
/**
 * @}
 */

#ifdef __cplusplus
/**
 * Converts a C++ "bool" type to a PP_Bool.
 *
 * @param[in] b A C++ "bool" type.
 *
 * @return A PP_Bool.
 */
inline PP_Bool PP_FromBool(bool b) {
  return b ? PP_TRUE : PP_FALSE;
}

/**
 * Converts a PP_Bool to a C++ "bool" type.
 *
 * @param[in] b A PP_Bool.
 *
 * @return A C++ "bool" type.
 */
inline bool PP_ToBool(PP_Bool b) {
  return (b != PP_FALSE);
}

#endif  /* __cplusplus */

#endif  /* PPAPI_C_PP_BOOL_H_ */

