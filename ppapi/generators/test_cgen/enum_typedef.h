/*
 * Copyright 2011 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From test_cgen/enum_typedef.idl modified Wed Dec  5 13:08:05 2012. */

#ifndef PPAPI_C_TEST_CGEN_ENUM_TYPEDEF_H_
#define PPAPI_C_TEST_CGEN_ENUM_TYPEDEF_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/test_cgen/stdint.h"

/**
 * @file
 * This file will test that the IDL snippet matches the comment.
 */


/**
 * @addtogroup Enums
 * @{
 */
/* typedef enum { A = 1, B = 2, C = 3, D = A + B, E = ~D } et1; */
typedef enum {
  A = 1,
  B = 2,
  C = 3,
  D = A + B,
  E = ~D
} et1;
/**
 * @}
 */

/**
 * @addtogroup Typedefs
 * @{
 */
/* typedef int32_t i; */
typedef int32_t i;

/* typedef int32_t i2[3]; */
typedef int32_t i2[3];

/* typedef int32_t (*i_func)(void); */
typedef int32_t (*i_func)(void);

/* typedef int32_t (*i_func_i)(int32_t i); */
typedef int32_t (*i_func_i)(int32_t i);

/* typedef et1 et4[4]; */
typedef et1 et4[4];

/*
 * typedef int8_t (*PPB_Audio_Callback)(const void* sample_buffer,
 *                                   uint32_t buffer_size_in_bytes,
 *                                   const void* user_data);
 */
typedef int8_t (*PPB_Audio_Callback)(const void* sample_buffer,
                                     uint32_t buffer_size_in_bytes,
                                     const void* user_data);
/**
 * @}
 */

#endif  /* PPAPI_C_TEST_CGEN_ENUM_TYPEDEF_H_ */

