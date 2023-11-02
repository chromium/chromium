/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_console.idl modified Fri Nov 16 15:28:43 2012. */

#ifndef PPAPI_C_PPB_CONSOLE_H_
#define PPAPI_C_PPB_CONSOLE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_CONSOLE_INTERFACE_1_0 "PPB_Console;1.0"
#define PPB_CONSOLE_INTERFACE PPB_CONSOLE_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_Console</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  PP_LOGLEVEL_TIP = 0,
  PP_LOGLEVEL_LOG = 1,
  PP_LOGLEVEL_WARNING = 2,
  PP_LOGLEVEL_ERROR = 3
} PP_LogLevel;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_LogLevel, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Console_1_0 {
  /**
   * Logs the given message to the JavaScript console associated with the
   * given plugin instance with the given logging level. The name of the plugin
   * issuing the log message will be automatically prepended to the message.
   * The value may be any type of Var.
   */
  void (*Log)(PP_Instance instance, PP_LogLevel level, struct PP_Var value);
  /**
   * Logs a message to the console with the given source information rather
   * than using the internal PPAPI plugin name. The name must be a string var.
   *
   * The regular log function will automatically prepend the name of your
   * plugin to the message as the "source" of the message. Some plugins may
   * wish to override this. For example, if your plugin is a Python
   * interpreter, you would want log messages to contain the source .py file
   * doing the log statement rather than have "python" show up in the console.
   */
  void (*LogWithSource)(PP_Instance instance,
                        PP_LogLevel level,
                        struct PP_Var source,
                        struct PP_Var value);
};

typedef struct PPB_Console_1_0 PPB_Console;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_CONSOLE_H_ */

