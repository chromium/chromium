/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_file_io_private.idl modified Wed Mar 27 14:43:25 2013. */

#ifndef PPAPI_C_PRIVATE_PPB_FILE_IO_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_FILE_IO_PRIVATE_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_FILEIO_PRIVATE_INTERFACE_0_1 "PPB_FileIO_Private;0.1"
#define PPB_FILEIO_PRIVATE_INTERFACE PPB_FILEIO_PRIVATE_INTERFACE_0_1

/**
 * @file
 */


#include "ppapi/c/private/pp_file_handle.h"

/**
 * @addtogroup Interfaces
 * @{
 */
/* PPB_FileIO_Private interface */
struct PPB_FileIO_Private_0_1 {
  /**
   * Returns a file handle corresponding to the given FileIO
   * object.  The FileIO object must have been opened with a
   * successful call to FileIO::Open.  The caller gets the ownership
   * of the returned file handle and must close it.
   */
  int32_t (*RequestOSFileHandle)(PP_Resource file_io,
                                 PP_FileHandle* handle,
                                 struct PP_CompletionCallback callback);
};

typedef struct PPB_FileIO_Private_0_1 PPB_FileIO_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_FILE_IO_PRIVATE_H_ */

