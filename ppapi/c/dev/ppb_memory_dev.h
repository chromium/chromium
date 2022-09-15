/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_memory_dev.idl modified Fri Nov 18 15:58:00 2011. */

#ifndef PPAPI_C_DEV_PPB_MEMORY_DEV_H_
#define PPAPI_C_DEV_PPB_MEMORY_DEV_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_MEMORY_DEV_INTERFACE_0_1 "PPB_Memory(Dev);0.1"
#define PPB_MEMORY_DEV_INTERFACE PPB_MEMORY_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_Memory interface</code> for functions
 * related to memory management.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The PPB_Memory_Dev interface contains pointers to functions related to memory
 * management.
 *
 */
struct PPB_Memory_Dev_0_1 {
  /**
   * MemAlloc is a pointer to a function that allocate memory.
   *
   * @param[in] num_bytes A number of bytes to allocate.
   * @return A pointer to the memory if successful, NULL If the
   * allocation fails.
   */
  void* (*MemAlloc)(uint32_t num_bytes);
  /**
   * MemFree is a pointer to a function that deallocates memory.
   *
   * @param[in] ptr A pointer to the memory to deallocate. It is safe to
   * pass NULL to this function.
   */
  void (*MemFree)(void* ptr);
};

typedef struct PPB_Memory_Dev_0_1 PPB_Memory_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_MEMORY_DEV_H_ */

