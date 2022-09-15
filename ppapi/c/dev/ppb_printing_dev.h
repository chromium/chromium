/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_printing_dev.idl modified Fri Apr 19 10:45:09 2013. */

#ifndef PPAPI_C_DEV_PPB_PRINTING_DEV_H_
#define PPAPI_C_DEV_PPB_PRINTING_DEV_H_

#include "ppapi/c/dev/pp_print_settings_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_PRINTING_DEV_INTERFACE_0_7 "PPB_Printing(Dev);0.7"
#define PPB_PRINTING_DEV_INTERFACE PPB_PRINTING_DEV_INTERFACE_0_7

/**
 * @file
 * Definition of the PPB_Printing interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Printing_Dev_0_7 {
  /** Create a resource for accessing printing functionality.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   *
   * @return A <code>PP_Resource</code> containing the printing resource if
   * successful or 0 if it could not be created.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Outputs the default print settings for the default printer into
   * <code>print_settings</code>. The callback is called with
   * <code>PP_OK</code> when the settings have been retrieved successfully.
   *
   * @param[in] resource The printing resource.
   *
   * @param[in] callback A <code>CompletionCallback</code> to be called when
   * <code>print_settings</code> have been retrieved.
   *
   * @return PP_OK_COMPLETIONPENDING if request for the default print settings
   * was successful, another error code from pp_errors.h on failure.
   */
  int32_t (*GetDefaultPrintSettings)(
      PP_Resource resource,
      struct PP_PrintSettings_Dev* print_settings,
      struct PP_CompletionCallback callback);
};

typedef struct PPB_Printing_Dev_0_7 PPB_Printing_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_PRINTING_DEV_H_ */

