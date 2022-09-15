/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_messaging.idl modified Wed Sep 24 10:48:37 2014. */

#ifndef PPAPI_C_PPB_MESSAGING_H_
#define PPAPI_C_PPB_MESSAGING_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_message_handler.h"

#define PPB_MESSAGING_INTERFACE_1_0 "PPB_Messaging;1.0"
#define PPB_MESSAGING_INTERFACE_1_2 "PPB_Messaging;1.2"
#define PPB_MESSAGING_INTERFACE PPB_MESSAGING_INTERFACE_1_2

/**
 * @file
 * This file defines the <code>PPB_Messaging</code> interface implemented
 * by the browser for sending messages to DOM elements associated with a
 * specific module instance.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_Messaging</code> interface is implemented by the browser
 * and is related to sending messages to JavaScript message event listeners on
 * the DOM element associated with specific module instance.
 */
struct PPB_Messaging_1_2 {
  /**
   * PostMessage() asynchronously invokes any listeners for message events on
   * the DOM element for the given module instance. A call to PostMessage()
   * will not block while the message is processed.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] message A <code>PP_Var</code> containing the data to be sent to
   * JavaScript.
   * <code>message</code> can be any <code>PP_Var</code> type except
   * <code>PP_VARTYPE_OBJECT</code>. Array/Dictionary types are supported from
   * Chrome M29 onward. All var types are copied when passing them to
   * JavaScript.
   *
   * When passing array or dictionary <code>PP_Var</code>s, the entire reference
   * graph will be converted and transferred. If the reference graph has cycles,
   * the message will not be sent and an error will be logged to the console.
   *
   * Listeners for message events in JavaScript code will receive an object
   * conforming to the HTML 5 <code>MessageEvent</code> interface.
   * Specifically, the value of message will be contained as a property called
   *  data in the received <code>MessageEvent</code>.
   *
   * This messaging system is similar to the system used for listening for
   * messages from Web Workers. Refer to
   * <code>http://www.whatwg.org/specs/web-workers/current-work/</code> for
   * further information.
   *
   * <strong>Example:</strong>
   *
   * @code
   *
   * <body>
   *   <object id="plugin"
   *           type="application/x-ppapi-postMessage-example"/>
   *   <script type="text/javascript">
   *     var plugin = document.getElementById('plugin');
   *     plugin.addEventListener("message",
   *                             function(message) { alert(message.data); },
   *                             false);
   *   </script>
   * </body>
   *
   * @endcode
   *
   * The module instance then invokes PostMessage() as follows:
   *
   * @code
   *
   *  char hello_world[] = "Hello world!";
   *  PP_Var hello_var = ppb_var_interface->VarFromUtf8(instance,
   *                                                    hello_world,
   *                                                    sizeof(hello_world));
   *  ppb_messaging_interface->PostMessage(instance, hello_var); // Copies var.
   *  ppb_var_interface->Release(hello_var);
   *
   * @endcode
   *
   * The browser will pop-up an alert saying "Hello world!"
   */
  void (*PostMessage)(PP_Instance instance, struct PP_Var message);
  /**
   * Registers a handler for receiving messages from JavaScript. If a handler
   * is registered this way, it will replace PPP_Messaging, and all messages
   * sent from JavaScript via postMessage and postMessageAndAwaitResponse will
   * be dispatched to <code>handler</code>.
   *
   * The function calls will be dispatched via <code>message_loop</code>. This
   * means that the functions will be invoked on the thread to which
   * <code>message_loop</code> is attached, when <code>message_loop</code> is
   * run. It is illegal to pass the main thread message loop;
   * RegisterMessageHandler will return PP_ERROR_WRONG_THREAD in that case.
   * If you quit <code>message_loop</code> before calling Unregister(),
   * the browser will not be able to call functions in the plugin's message
   * handler any more. That could mean missing some messages or could cause a
   * leak if you depend on Destroy() to free hander data. So you should,
   * whenever possible, Unregister() the handler prior to quitting its event
   * loop.
   *
   * Attempting to register a message handler when one is already registered
   * will cause the current MessageHandler to be unregistered and replaced. In
   * that case, no messages will be sent to the "default" message handler
   * (PPP_Messaging). Messages will stop arriving at the prior message handler
   * and will begin to be dispatched at the new message handler.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] user_data A pointer the plugin may choose to use when handling
   * calls to functions within PPP_MessageHandler. The browser will pass this
   * same pointer when invoking functions within PPP_MessageHandler.
   * @param[in] handler The plugin-provided set of functions for handling
   * messages.
   * @param[in] message_loop Represents the message loop on which
   * PPP_MessageHandler functions should be invoked.
   * @return PP_OK on success, or an error from pp_errors.h.
   */
  int32_t (*RegisterMessageHandler)(
      PP_Instance instance,
      void* user_data,
      const struct PPP_MessageHandler_0_2* handler,
      PP_Resource message_loop);
  /**
   * Unregisters the current message handler for <code>instance</code> if one
   * is registered. After this call, the message handler (if one was
   * registered) will have "Destroy" called on it and will receive no further
   * messages after that point. After that point, all messages sent from
   * JavaScript using postMessage() will be dispatched to PPP_Messaging (if
   * the plugin supports PPP_MESSAGING_INTERFACE). Attempts to call
   * postMessageAndAwaitResponse() from JavaScript will fail.
   *
   * Attempting to unregister a message handler when none is registered has no
   * effect.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   */
  void (*UnregisterMessageHandler)(PP_Instance instance);
};

typedef struct PPB_Messaging_1_2 PPB_Messaging;

struct PPB_Messaging_1_0 {
  void (*PostMessage)(PP_Instance instance, struct PP_Var message);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_MESSAGING_H_ */

