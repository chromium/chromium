/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_file_ref_private.idl modified Fri Dec 16 17:34:59 2011. */

#ifndef PPAPI_C_PRIVATE_PPB_FILE_REF_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_FILE_REF_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_FILEREFPRIVATE_INTERFACE_0_1 "PPB_FileRefPrivate;0.1"
#define PPB_FILEREFPRIVATE_INTERFACE PPB_FILEREFPRIVATE_INTERFACE_0_1

/**
 * @file
 * This file contains the <code>PPB_FileRefPrivate</code> interface. */


/**
 * @addtogroup Interfaces
 * @{
 */
/* PPB_FileRefPrivate interface */
struct PPB_FileRefPrivate_0_1 {
  /**
   * GetAbsolutePath() returns the absolute path of the file.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   *
   * @return A <code>PP_Var</code> containing the absolute path of the file.
   */
  struct PP_Var (*GetAbsolutePath)(PP_Resource file_ref);
};

typedef struct PPB_FileRefPrivate_0_1 PPB_FileRefPrivate;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_FILE_REF_PRIVATE_H_ */

