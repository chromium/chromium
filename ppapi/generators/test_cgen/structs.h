/*
 * Copyright 2011 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From test_cgen/structs.idl modified Wed Nov 21 11:02:50 2012. */

#ifndef PPAPI_C_TEST_CGEN_STRUCTS_H_
#define PPAPI_C_TEST_CGEN_STRUCTS_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/test_cgen/stdint.h"

/**
 * @file
 * This file will test that the IDL snippet matches the comment.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/* typedef uint8_t s_array[3]; */
typedef uint8_t s_array[3];
/**
 * @}
 */

/**
 * @addtogroup Enums
 * @{
 */
/* typedef enum { esv1 = 1, esv2 = 2 } senum; */
typedef enum {
  esv1 = 1,
  esv2 = 2
} senum;
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/* struct st1 { int32_t i; senum j; }; */
struct st1 {
  int32_t i;
  senum j;
};

/* struct st2 { s_array pixels[640][480]; }; */
struct st2 {
  s_array pixels[640][480];
};
/**
 * @}
 */

/**
 * @addtogroup Typedefs
 * @{
 */
/* typedef float (*func_t)(const s_array data); */
typedef float (*func_t)(const s_array data);

/* typedef func_t (*findfunc_t)(const char* x); */
typedef func_t (*findfunc_t)(const char* x);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/*
 * struct sfoo {
 *  s_array screen[480][640];
 *  findfunc_t myfunc;
 * };
 */
struct sfoo {
  s_array screen[480][640];
  findfunc_t myfunc;
};
/**
 * @}
 */

#endif  /* PPAPI_C_TEST_CGEN_STRUCTS_H_ */

