/* Copyright 2011 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From test_cgen_range/versions.idl modified Wed Nov 21 15:18:23 2012. */

#ifndef PPAPI_C_TEST_CGEN_RANGE_VERSIONS_H_
#define PPAPI_C_TEST_CGEN_RANGE_VERSIONS_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/test_cgen_range/dev_channel_interface.h"

#define FOO_INTERFACE_0_0 "Foo;0.0"
#define FOO_INTERFACE_1_0 "Foo;1.0"
#define FOO_INTERFACE_2_0 "Foo;2.0"
#define FOO_INTERFACE FOO_INTERFACE_2_0

/**
 * @file
 * File Comment. */


/**
 * @addtogroup Interfaces
 * @{
 */
/* Bogus Interface Foo */
struct Foo_2_0 {
  /**
   * Comment for function x,y,z
   */
  int32_t (*Bar)(int32_t x, int32_t y, int32_t z);
};

typedef struct Foo_2_0 Foo;

struct Foo_0_0 {
  int32_t (*Bar)(int32_t x);
};

struct Foo_1_0 {
  int32_t (*Bar)(int32_t x, int32_t y);
};
/**
 * @}
 */

#endif  /* PPAPI_C_TEST_CGEN_RANGE_VERSIONS_H_ */

