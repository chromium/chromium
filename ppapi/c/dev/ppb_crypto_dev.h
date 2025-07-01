/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_crypto_dev.idl modified Wed Nov  7 13:40:08 2012. */

#ifndef PPAPI_C_DEV_PPB_CRYPTO_DEV_H_
#define PPAPI_C_DEV_PPB_CRYPTO_DEV_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_CRYPTO_DEV_INTERFACE_0_1 "PPB_Crypto(Dev);0.1"
#define PPB_CRYPTO_DEV_INTERFACE PPB_CRYPTO_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the PPB_Crypto_Dev interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Crypto_Dev_0_1 {
  /**
   * Fills the given buffer with random bytes. This is potentially slow so only
   * request the amount of data you need.
   */
  void (*GetRandomBytes)(char* buffer, uint32_t num_bytes);
};

typedef struct PPB_Crypto_Dev_0_1 PPB_Crypto_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_CRYPTO_DEV_H_ */

