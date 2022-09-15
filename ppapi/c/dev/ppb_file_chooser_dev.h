/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_file_chooser_dev.idl modified Mon Jun  4 12:44:29 2012. */

#ifndef PPAPI_C_DEV_PPB_FILE_CHOOSER_DEV_H_
#define PPAPI_C_DEV_PPB_FILE_CHOOSER_DEV_H_

#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_FILECHOOSER_DEV_INTERFACE_0_5 "PPB_FileChooser(Dev);0.5"
#define PPB_FILECHOOSER_DEV_INTERFACE_0_6 "PPB_FileChooser(Dev);0.6"
#define PPB_FILECHOOSER_DEV_INTERFACE PPB_FILECHOOSER_DEV_INTERFACE_0_6

/**
 * @file
 * This file defines the <code>PPB_FileChooser_Dev</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * This enumeration contains constants to control the behavior of the file
 * chooser dialog.
 */
typedef enum {
  /**
   * Mode for choosing a single existing file.
   */
  PP_FILECHOOSERMODE_OPEN = 0,
  /**
   * Mode for choosing multiple existing files.
   */
  PP_FILECHOOSERMODE_OPENMULTIPLE = 1
} PP_FileChooserMode_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_FileChooserMode_Dev, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_FileChooser_Dev_0_6 {
  /**
   * This function creates a file chooser dialog resource.  The chooser is
   * associated with a particular instance, so that it may be positioned on the
   * screen relative to the tab containing the instance.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] mode A <code>PP_FileChooserMode_Dev</code> value that controls
   * the behavior of the file chooser dialog.
   * @param[in] accept_types A comma-separated list of MIME types and file
   * extensions such as "audio/ *,text/plain,.html" (note there should be no
   * space between the '/' and the '*', but one is added to avoid confusing C++
   * comments). The dialog may restrict selectable files to the specified MIME
   * types and file extensions. If a string in the comma-separated list begins
   * with a period (.) then the string is interpreted as a file extension,
   * otherwise it is interpreted as a MIME-type. An empty string or an undefined
   * var may be given to indicate that all types should be accepted.
   *
   * @return A <code>PP_Resource</code> containing the file chooser if
   * successful or 0 if it could not be created.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        PP_FileChooserMode_Dev mode,
                        struct PP_Var accept_types);
  /**
   * Determines if the provided resource is a file chooser.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a generic
   * resource.
   *
   * @return A <code>PP_Bool</code> that is <code>PP_TRUE</code> if the given
   * resource is a file chooser resource, otherwise <code>PP_FALSE</code>.
   */
  PP_Bool (*IsFileChooser)(PP_Resource resource);
  /**
   * This function displays a previously created file chooser resource as a
   * dialog box, prompting the user to choose a file or files. This function
   * must be called in response to a user gesture, such as a mouse click or
   * touch event. The callback is called with PP_OK on successful completion
   * with a file (or files) selected, PP_ERROR_USERCANCEL if the user selected
   * no file, or another error code from pp_errors.h on failure.
   *
   * <b>Subtle note:</b> This function will only work when the tab containing
   * the plugin is visible. Show() will fail if the tab is in the background.
   * Since it's not normally possible to get input events while invisible, this
   * is not normally an issue. But there is a race condition because events are
   * processed asynchronously. If the user clicks and switches tabs very
   * quickly, a plugin could believe the tab is visible while Chrome believes
   * it is invisible and the Show() call will fail. This will not generally
   * cause user confusion since the user will have switched tabs and will not
   * want to see a file chooser from a different tab.
   *
   * @param[in] chooser The file chooser resource.
   *
   * @param[in] output An output array which will receive PP_Resource(s)
   * identifying the <code>PPB_FileRef</code> objects that the user selected on
   * success.
   *
   * @param[in] callback A <code>CompletionCallback</code> to be called after
   * the user has closed the file chooser dialog.
   *
   * @return PP_OK_COMPLETIONPENDING if request to show the dialog was
   * successful, another error code from pp_errors.h on failure.
   */
  int32_t (*Show)(PP_Resource chooser,
                  struct PP_ArrayOutput output,
                  struct PP_CompletionCallback callback);
};

typedef struct PPB_FileChooser_Dev_0_6 PPB_FileChooser_Dev;

struct PPB_FileChooser_Dev_0_5 {
  PP_Resource (*Create)(PP_Instance instance,
                        PP_FileChooserMode_Dev mode,
                        struct PP_Var accept_types);
  PP_Bool (*IsFileChooser)(PP_Resource resource);
  int32_t (*Show)(PP_Resource chooser, struct PP_CompletionCallback callback);
  PP_Resource (*GetNextChosenFile)(PP_Resource chooser);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_FILE_CHOOSER_DEV_H_ */

