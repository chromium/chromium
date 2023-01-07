/* Copyright 2011 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_stdint.idl modified Mon Jul 18 17:53:53 2011. */

#ifndef PPAPI_C_PP_STDINT_H_
#define PPAPI_C_PP_STDINT_H_

#include "ppapi/c/pp_macros.h"

/**
 * @file
 * This file provides a definition of C99 sized types
 * for Microsoft compilers. These definitions only apply
 * for trusted modules.
 */



/**
 *
 * @addtogroup Typedefs
 * @{
 */
#if defined(_MSC_VER)

/** This value represents a guaranteed unsigned 8 bit integer. */
typedef unsigned char uint8_t;

/** This value represents a guaranteed signed 8 bit integer. */
typedef signed char int8_t;

/** This value represents a guaranteed unsigned 16 bit short. */
typedef unsigned short uint16_t;

/** This value represents a guaranteed signed 16 bit short. */
typedef short int16_t;

/** This value represents a guaranteed unsigned 32 bit integer. */
typedef unsigned int uint32_t;

/** This value represents a guaranteed signed 32 bit integer. */
typedef int int32_t;

/** This value represents a guaranteed signed 64 bit integer. */
typedef __int64 int64_t;

/** This value represents a guaranteed unsigned 64 bit integer. */
typedef unsigned __int64 uint64_t;

#else
#include <stdint.h>
#endif
/**
 * @}
 */

#endif  /* PPAPI_C_PP_STDINT_H_ */

