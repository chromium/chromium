/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_ext_crx_file_system_private.idl,
 *   modified Fri Nov  1 12:23:59 2013.
 */

#ifndef PPAPI_C_PRIVATE_PPB_EXT_CRX_FILE_SYSTEM_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_EXT_CRX_FILE_SYSTEM_PRIVATE_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_EXT_CRXFILESYSTEM_PRIVATE_INTERFACE_0_1 \
    "PPB_Ext_CrxFileSystem_Private;0.1"
#define PPB_EXT_CRXFILESYSTEM_PRIVATE_INTERFACE \
    PPB_EXT_CRXFILESYSTEM_PRIVATE_INTERFACE_0_1

/**
 * @file
 * This file contains the <code>PPB_Ext_CrxFileSystem_Private</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/* <code>PPB_Ext_CrxFileSystem_Private</code> interface */
struct PPB_Ext_CrxFileSystem_Private_0_1 {
  /**
   * Open() opens the CRX file system for the current extension.  It will fail
   * when called from non-extension context.
   *
   * @param[in] crxfs A <code>PP_Resource</code> corresponding to a
   * CrxFileSystem.
   * @param[out] file_system An output <code>PP_Resource</code> corresponding
   * to a PPB_FileSystem.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of Open.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*Open)(PP_Instance instance,
                  PP_Resource* file_system,
                  struct PP_CompletionCallback callback);
};

typedef struct PPB_Ext_CrxFileSystem_Private_0_1 PPB_Ext_CrxFileSystem_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_EXT_CRX_FILE_SYSTEM_PRIVATE_H_ */

