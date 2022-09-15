/*
 * Copyright 2011 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From test_cgen/interface.idl modified Wed Nov 21 14:22:50 2012. */

#ifndef PPAPI_C_TEST_CGEN_INTERFACE_H_
#define PPAPI_C_TEST_CGEN_INTERFACE_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/test_cgen/stdint.h"

#define IFACEFOO_INTERFACE_1_0 "ifaceFoo;1.0"
#define IFACEFOO_INTERFACE IFACEFOO_INTERFACE_1_0

#define IFACEBAR_INTERFACE_1_0 "ifaceBar;1.0"
#define IFACEBAR_INTERFACE IFACEBAR_INTERFACE_1_0

/**
 * @file
 * This file will test that the IDL snippet matches the comment.
 */


/**
 * @addtogroup Structs
 * @{
 */
/* struct ist { void* X; }; */
struct ist {
  void* X;
};
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/*
 * struct ifaceFoo_1_0 {
 * int8_t (*mem1)(int16_t x, int32_t y);
 * int32_t (*mem2)(const struct ist* a);
 * int32_t (*mem3)(struct ist* b);
 * int32_t (*mem4)(const void** ptr);
 * int32_t (*mem5)(void** ptr);
 * };
 * typedef struct ifaceFoo_1_0 ifaceFoo;
 */
struct ifaceFoo_1_0 {
  int8_t (*mem1)(int16_t x, int32_t y);
  int32_t (*mem2)(const struct ist* a);
  int32_t (*mem3)(struct ist* b);
  int32_t (*mem4)(const void** ptr);
  int32_t (*mem5)(void** ptr);
};

typedef struct ifaceFoo_1_0 ifaceFoo;

struct ifaceBar_1_0 {
  int8_t (*testIface)(const struct ifaceFoo_1_0* foo, int32_t y);
  struct ifaceFoo_1_0* (*createIface)(const char* name);
};

typedef struct ifaceBar_1_0 ifaceBar;

struct ifaceNoString_1_0 {
  void (*mem)(void);
};

typedef struct ifaceNoString_1_0 ifaceNoString;
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct struct2 {
  struct ifaceBar_1_0* bar;
};
/**
 * @}
 */

#endif  /* PPAPI_C_TEST_CGEN_INTERFACE_H_ */

