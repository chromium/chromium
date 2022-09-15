/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From trusted/ppb_file_chooser_trusted.idl,
 *   modified Fri Mar 16 10:00:48 2012.
 */

#ifndef PPAPI_C_TRUSTED_PPB_FILE_CHOOSER_TRUSTED_H_
#define PPAPI_C_TRUSTED_PPB_FILE_CHOOSER_TRUSTED_H_

#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_FILECHOOSER_TRUSTED_INTERFACE_0_5 "PPB_FileChooserTrusted;0.5"
#define PPB_FILECHOOSER_TRUSTED_INTERFACE_0_6 "PPB_FileChooserTrusted;0.6"
#define PPB_FILECHOOSER_TRUSTED_INTERFACE PPB_FILECHOOSER_TRUSTED_INTERFACE_0_6

/**
 * @file
 * This file defines the <code>PPB_FileChooser_Trusted</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_FileChooserTrusted_0_6 {
  /**
   * This function displays a previously created file chooser resource as a
   * dialog box, prompting the user to choose a file or files to open, or a
   * single file for saving. The callback is called with PP_OK on successful
   * completion with a file (or files) selected or PP_ERROR_USERCANCEL if the
   * user selected no file.
   *
   * @param[in] chooser The file chooser resource.
   * @param[in] save_as A <code>PP_Bool</code> value indicating if this dialog
   * is choosing a file for saving.
   * @param[in] suggested_file_name If saving, the suggested name for the
   * file, otherwise, null or undefined.
   * @param[in] callback A <code>CompletionCallback</code> to be called after
   * the user has closed the file chooser dialog.
   *
   * @return PP_OK_COMPLETIONPENDING if request to show the dialog was
   * successful, another error code from pp_errors.h on failure.
   */
  int32_t (*ShowWithoutUserGesture)(PP_Resource chooser,
                                    PP_Bool save_as,
                                    struct PP_Var suggested_file_name,
                                    struct PP_ArrayOutput output,
                                    struct PP_CompletionCallback callback);
};

typedef struct PPB_FileChooserTrusted_0_6 PPB_FileChooserTrusted;

struct PPB_FileChooserTrusted_0_5 {
  int32_t (*ShowWithoutUserGesture)(PP_Resource chooser,
                                    PP_Bool save_as,
                                    struct PP_Var suggested_file_name,
                                    struct PP_CompletionCallback callback);
};
/**
 * @}
 */

#endif  /* PPAPI_C_TRUSTED_PPB_FILE_CHOOSER_TRUSTED_H_ */

