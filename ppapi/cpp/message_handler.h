// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MESSAGE_HANDLER_H_
#define PPAPI_CPP_MESSAGE_HANDLER_H_

namespace pp {

/// <code>MessageHandler</code> is an abstract base class that the plugin may
/// implement if it wants to receive messages from JavaScript on a background
/// thread when JavaScript invokes  postMessage() or
/// postMessageAndAwaitResponse(). See pp::Instance::RegisterMessageHandler()
/// for usage.
class MessageHandler {
 public:
  virtual ~MessageHandler() {}

  /// Invoked as a result of JavaScript invoking postMessage() on the plugin's
  /// DOM element.
  ///
  /// @param[in] instance An <code>InstanceHandle</code> identifying one
  /// instance of a module.
  /// @param[in] message_data A copy of the parameter that JavaScript provided
  /// to postMessage().
  virtual void HandleMessage(pp::InstanceHandle instance,
                             const Var& message_data) = 0;

  /// Invoked as a result of JavaScript invoking postMessageAndAwaitResponse()
  /// on the plugin's DOM element.
  ///
  /// NOTE: JavaScript execution is blocked during the duration of this call.
  /// Hence, the plugin should respond as quickly as possible. For this reason,
  /// blocking completion callbacks are disallowed while handling a blocking
  /// message.
  ///
  /// @param[in] instance An <code>InstanceHandle</code> identifying one
  /// instance of a module.
  /// @param[in] message_data A copy of the parameter that JavaScript provided
  /// to postMessage().
  /// @return Returns a pp::Var that is then copied to a JavaScript object
  /// which is returned as the result of JavaScript's call of
  /// postMessageAndAwaitResponse().
  virtual pp::Var HandleBlockingMessage(pp::InstanceHandle instance,
                                        const Var& message_data) = 0;

  /// Invoked when this MessageHandler is no longer needed. After this, no more
  /// calls will be made to this object.
  ///
  /// @param[in] instance An <code>InstanceHandle</code> identifying one
  /// instance of a module.
  virtual void WasUnregistered(pp::InstanceHandle instance) = 0;
};

}  // namespace pp

#endif  // PPAPI_CPP_MESSAGE_HANDLER_H_
