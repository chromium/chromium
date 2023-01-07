/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppp_message_handler.idl modified Wed Sep 24 10:48:49 2014. */

#ifndef PPAPI_C_PPP_MESSAGE_HANDLER_H_
#define PPAPI_C_PPP_MESSAGE_HANDLER_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

/**
 * @file
 * This file defines the <code>PPP_MessageHandler</code> interface that plugins
 * can implement and register using PPB_Messaging::RegisterMessageHandler in
 * order to handle messages sent from JavaScript via postMessage() or
 * postMessageAndAwaitResponse().
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPP_MessageHandler</code> interface is implemented by the plugin
 * if the plugin wants to receive messages from a thread other than the main
 * Pepper thread, or if the plugin wants to handle blocking messages which
 * JavaScript may send via postMessageAndAwaitResponse().
 *
 * This interface struct should not be returned by PPP_GetInterface; instead it
 * must be passed as a parameter to PPB_Messaging::RegisterMessageHandler.
 */
struct PPP_MessageHandler_0_2 {
  /**
   * Invoked as a result of JavaScript invoking postMessage() on the plugin's
   * DOM element.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] user_data is the same pointer which was provided by a call to
   * RegisterMessageHandler().
   * @param[in] message A copy of the parameter that JavaScript provided to
   * postMessage().
   */
  void (*HandleMessage)(PP_Instance instance,
                        void* user_data,
                        const struct PP_Var* message);
  /**
   * Invoked as a result of JavaScript invoking postMessageAndAwaitResponse()
   * on the plugin's DOM element.
   *
   * NOTE: JavaScript execution is blocked during the duration of this call.
   * Hence, the plugin should respond as quickly as possible. For this reason,
   * blocking completion callbacks are disallowed while handling a blocking
   * message.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] user_data is the same pointer which was provided by a call to
   * RegisterMessageHandler().
   * @param[in] message is a copy of the parameter that JavaScript provided
   * to postMessageAndAwaitResponse().
   * @param[out] response will be copied to a JavaScript object which is
   * returned as the result of postMessageAndAwaitResponse() to the invoking
   *
   */
  void (*HandleBlockingMessage)(PP_Instance instance,
                                void* user_data,
                                const struct PP_Var* message,
                                struct PP_Var* response);
  /**
   * Invoked when the handler object is no longer needed. After this, no more
   * calls will be made which pass this same value for <code>instance</code>
   * and <code>user_data</code>.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] user_data is the same pointer which was provided by a call to
   * RegisterMessageHandler.
   */
  void (*Destroy)(PP_Instance instance, void* user_data);
};

typedef struct PPP_MessageHandler_0_2 PPP_MessageHandler;
/**
 * @}
 */

#endif  /* PPAPI_C_PPP_MESSAGE_HANDLER_H_ */

