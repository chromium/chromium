/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppp_messaging.idl modified Wed Jun  5 10:32:43 2013. */

#ifndef PPAPI_C_PPP_MESSAGING_H_
#define PPAPI_C_PPP_MESSAGING_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPP_MESSAGING_INTERFACE_1_0 "PPP_Messaging;1.0"
#define PPP_MESSAGING_INTERFACE PPP_MESSAGING_INTERFACE_1_0

/**
 * @file
 * This file defines the PPP_Messaging interface containing pointers to
 * functions that you must implement to handle postMessage messages
 * on the associated DOM element.
 *
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPP_Messaging</code> interface contains pointers to functions
 * that you must implement to handle postMessage events on the associated
 * DOM element.
 */
struct PPP_Messaging_1_0 {
  /**
   * HandleMessage() is a function that the browser calls when PostMessage()
   * is invoked on the DOM element for the module instance in JavaScript. Note
   * that PostMessage() in the JavaScript interface is asynchronous, meaning
   * JavaScript execution will not be blocked while HandleMessage() is
   * processing the message.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] message A <code>PP_Var</code> which has been converted from a
   * JavaScript value. JavaScript array/object types are supported from Chrome
   * M29 onward. All JavaScript values are copied when passing them to the
   * plugin.
   *
   * When converting JavaScript arrays, any object properties whose name
   * is not an array index are ignored. When passing arrays and objects, the
   * entire reference graph will be converted and transferred. If the reference
   * graph has cycles, the message will not be sent and an error will be logged
   * to the console.
   *
   * The following JavaScript code invokes <code>HandleMessage</code>, passing
   * the module instance on which it was invoked, with <code>message</code>
   * being a string <code>PP_Var</code> containing "Hello world!"
   *
   * <strong>Example:</strong>
   *
   * @code
   *
   * <body>
   *   <object id="plugin"
   *           type="application/x-ppapi-postMessage-example"/>
   *   <script type="text/javascript">
   *     document.getElementById('plugin').postMessage("Hello world!");
   *   </script>
   * </body>
   *
   * @endcode
   *
   */
  void (*HandleMessage)(PP_Instance instance, struct PP_Var message);
};

typedef struct PPP_Messaging_1_0 PPP_Messaging;
/**
 * @}
 */

#endif  /* PPAPI_C_PPP_MESSAGING_H_ */

