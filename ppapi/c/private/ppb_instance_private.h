/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_instance_private.idl modified Tue Jul 23 13:19:04 2013. */

#ifndef PPAPI_C_PRIVATE_PPB_INSTANCE_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_INSTANCE_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_INSTANCE_PRIVATE_INTERFACE_0_1 "PPB_Instance_Private;0.1"
#define PPB_INSTANCE_PRIVATE_INTERFACE PPB_INSTANCE_PRIVATE_INTERFACE_0_1

/**
 * @file
 * This file defines the PPB_Instance_Private interface implemented by the
 * browser and containing pointers to functions available only to trusted plugin
 * instances.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * The <code>PP_ExternalPluginResult </code> enum contains result codes from
 * launching an external plugin.
 */
typedef enum {
  /** Successful external plugin call */
  PP_EXTERNAL_PLUGIN_OK = 0,
  /** Unspecified external plugin error */
  PP_EXTERNAL_PLUGIN_FAILED = 1,
  /** Error creating the module */
  PP_EXTERNAL_PLUGIN_ERROR_MODULE = 2,
  /** Error creating and initializing the instance */
  PP_EXTERNAL_PLUGIN_ERROR_INSTANCE = 3
} PP_ExternalPluginResult;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_ExternalPluginResult, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The PPB_Instance_Private interface contains functions available only to
 * trusted plugin instances.
 *
 */
struct PPB_Instance_Private_0_1 {
  /**
   * GetWindowObject is a pointer to a function that determines
   * the DOM window containing this module instance.
   *
   * @param[in] instance A PP_Instance whose WindowObject should be retrieved.
   * @return A PP_Var containing window object on success.
   */
  struct PP_Var (*GetWindowObject)(PP_Instance instance);
  /**
   * GetOwnerElementObject is a pointer to a function that determines
   * the DOM element containing this module instance.
   *
   * @param[in] instance A PP_Instance whose WindowObject should be retrieved.
   * @return A PP_Var containing DOM element on success.
   */
  struct PP_Var (*GetOwnerElementObject)(PP_Instance instance);
  /**
   * ExecuteScript is a pointer to a function that executes the given
   * script in the context of the frame containing the module.
   *
   * The exception, if any, will be returned in *exception. As with the PPB_Var
   * interface, the exception parameter, if non-NULL, must be initialized
   * to a "void" var or the function will immediately return. On success,
   * the exception parameter will be set to a "void" var. On failure, the
   * return value will be a "void" var.
   *
   * @param[in] script A string containing the JavaScript to execute.
   * @param[in/out] exception PP_Var containing the exception. Initialize
   * this to NULL if you don't want exception info; initialize this to a void
   * exception if want exception info.
   *
   * @return The result of the script execution, or a "void" var
   * if execution failed.
   */
  struct PP_Var (*ExecuteScript)(PP_Instance instance,
                                 struct PP_Var script,
                                 struct PP_Var* exception);
};

typedef struct PPB_Instance_Private_0_1 PPB_Instance_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_INSTANCE_PRIVATE_H_ */

