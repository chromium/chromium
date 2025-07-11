/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From test_cgen_range/dev_channel_interface.idl,
 *   modified Tue Dec  3 14:58:15 2013.
 */

#ifndef PPAPI_C_TEST_CGEN_RANGE_DEV_CHANNEL_INTERFACE_H_
#define PPAPI_C_TEST_CGEN_RANGE_DEV_CHANNEL_INTERFACE_H_

#include "ppapi/c/pp_macros.h"

#define TESTDEV_INTERFACE_1_0 "TestDev;1.0"
#define TESTDEV_INTERFACE_1_2 "TestDev;1.2"
#define TESTDEV_INTERFACE_1_3 "TestDev;1.3" /* dev */
#define TESTDEV_INTERFACE TESTDEV_INTERFACE_1_2

#define TESTDEVTOSTABLE_INTERFACE_1_0 "TestDevToStable;1.0"
#define TESTDEVTOSTABLE_INTERFACE_1_1 "TestDevToStable;1.1" /* dev */
#define TESTDEVTOSTABLE_INTERFACE_1_2 "TestDevToStable;1.2"
#define TESTDEVTOSTABLE_INTERFACE TESTDEVTOSTABLE_INTERFACE_1_2

/**
 * @file
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * TestDev
 */
struct TestDev_1_3 { /* dev */
  /**
   * TestDev1()
   */
  void (*TestDev1)(void);
  /**
   * TestDev2()
   */
  void (*TestDev2)(void);
  /**
   * TestDev3()
   */
  void (*TestDev3)(void);
  /**
   * TestDev4()
   */
  void (*TestDev4)(void);
};

struct TestDev_1_0 {
  void (*TestDev1)(void);
};

struct TestDev_1_2 {
  void (*TestDev1)(void);
  void (*TestDev3)(void);
};

typedef struct TestDev_1_2 TestDev;

/**
 * TestDevToStable
 */
struct TestDevToStable_1_2 {
  /**
   * Foo() comment.
   */
  void (*Foo)(int32_t x);
  /**
   * Bar() comment.
   */
  void (*Bar)(int32_t x);
  /**
   * Baz() comment.
   */
  void (*Baz)(int32_t x);
};

typedef struct TestDevToStable_1_2 TestDevToStable;

struct TestDevToStable_1_0 {
  void (*Foo)(int32_t x);
};

struct TestDevToStable_1_1 { /* dev */
  void (*Foo)(int32_t x);
  void (*Bar)(int32_t x);
  void (*Baz)(int32_t x);
};
/**
 * @}
 */

#endif  /* PPAPI_C_TEST_CGEN_RANGE_DEV_CHANNEL_INTERFACE_H_ */

