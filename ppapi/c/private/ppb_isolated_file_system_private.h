/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_isolated_file_system_private.idl,
 *   modified Fri Nov  8 02:21:15 2013.
 */

#ifndef PPAPI_C_PRIVATE_PPB_ISOLATED_FILE_SYSTEM_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_ISOLATED_FILE_SYSTEM_PRIVATE_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_ISOLATEDFILESYSTEM_PRIVATE_INTERFACE_0_2 \
    "PPB_IsolatedFileSystem_Private;0.2"
#define PPB_ISOLATEDFILESYSTEM_PRIVATE_INTERFACE \
    PPB_ISOLATEDFILESYSTEM_PRIVATE_INTERFACE_0_2

/**
 * @file
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * The <code>PP_IsolatedFileSystemType_Private</code> values indicate the type
 * of isolated file systems.
 */
typedef enum {
  /** Type for invalid file systems */
  PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_INVALID = 0,
  /** Type for CRX file systems */
  PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_CRX = 1
} PP_IsolatedFileSystemType_Private;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_IsolatedFileSystemType_Private, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/* <code>PPB_IsolatedFileSystem_Private</code> interface */
struct PPB_IsolatedFileSystem_Private_0_2 {
  /**
   * Open() opens a file system corresponding the given file system type.
   *
   * When opening the CRX file system, this should be called from an extension
   * context, otherwise it will fail.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying the instance
   * with the file system.
   * @param[in] type A file system type as defined by
   * <code>PP_IsolatedFileSystemType_Private</code> enum.
   * @param[out] file_system An output <code>PP_Resource</code> corresponding
   * to a PPB_FileSystem.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of Open.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*Open)(PP_Instance instance,
                  PP_IsolatedFileSystemType_Private type,
                  PP_Resource* file_system,
                  struct PP_CompletionCallback callback);
};

typedef struct PPB_IsolatedFileSystem_Private_0_2
    PPB_IsolatedFileSystem_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_ISOLATED_FILE_SYSTEM_PRIVATE_H_ */

