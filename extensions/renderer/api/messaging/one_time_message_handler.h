// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_MESSAGING_ONE_TIME_MESSAGE_HANDLER_H_
#define EXTENSIONS_RENDERER_API_MESSAGING_ONE_TIME_MESSAGE_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "extensions/common/mojom/message_port.mojom.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/get_per_context_data.h"
#include "v8/include/v8-forward.h"

namespace gin {
class Arguments;
}

namespace extensions {

namespace mojom {
enum class ChannelType;
}

class NativeExtensionBindingsSystem;
class NativeRendererMessagingService;
class ScriptContext;
class Message;
struct MessageTarget;
struct PortId;

// A class for handling one-time message communication, including
// runtime.sendMessage and extension.sendRequest. These methods use the same
// underlying architecture as long-lived port-based communications (like
// runtime.connect), but are exposed through a simpler API.
// A basic flow will be from an "opener" (the original sender) and a "receiver"
// (the event listener), which will be in two separate contexts (and potentially
// renderer processes). The flow is outlined below:
//
// chrome.runtime.sendMessage(  // initiates the sendMessage flow, triggering
//                              // SendMessage().
//     {foo: bar},              // The data sent with SendMessage().
//     function() { ... });     // The response callback in SendMessage().
//
// This creates a new opener port in the context, and posts a message to it
// with the data. The browser then dispatches this to other renderers.
//
// In another context, we have:
// chrome.runtime.onMessage.addListener(function(message, sender, reply) {
//   ...
//   reply(...);
// });
//
// When the renderer receives the connection message, we will create a
// new receiver port in this context via AddReceiver().
// When the message comes in, we reply with DeliverMessage() to the receiver's
// port ID.
// If the receiver replies via the reply callback, it will send a new message
// back along the port to the browser. The browser then sends this message back
// to the opener's renderer, where it is delivered via DeliverMessage().
//
// This concludes the one-time message flow.
//
// For managing the callbacks that are called in response to replies from
// listeners, this class delegates to a `OneTimeMessageCallbackManager`.
//
// This object is owned by the `NativeRendererMessagingService`, and has its
// lifetime bound to it.
class OneTimeMessageHandler {
 public:
  // A unique identifier that identifies which C++ callback is associated with
  // which `v8:Function` when the function is called for a message response.
  using CallbackID = base::UnguessableToken;

  // A multi-use callback that is bound to a v8 function and is called when the
  // v8 function is called in the context.
  using OneTimeMessageCallback =
      base::OnceCallback<void(gin::Arguments* arguments)>;

  explicit OneTimeMessageHandler(
      NativeExtensionBindingsSystem* bindings_system);

  OneTimeMessageHandler(const OneTimeMessageHandler&) = delete;
  OneTimeMessageHandler& operator=(const OneTimeMessageHandler&) = delete;

  ~OneTimeMessageHandler();

  // Returns true if the given context has a port with the specified id.
  bool HasPort(ScriptContext* script_context, const PortId& port_id);

  // Initiates a flow to send a message from the given `script_context`. Returns
  // the associated promise if this is a promise based request, otherwise
  // returns an empty promise.
  v8::Local<v8::Promise> SendMessage(
      ScriptContext* script_context,
      const PortId& new_port_id,
      const MessageTarget& target_id,
      mojom::ChannelType channel_type,
      const Message& message,
      binding::AsyncResponseType async_type,
      v8::Local<v8::Function> response_callback,
      mojom::MessagePortHost* message_port_host,
      mojo::PendingAssociatedRemote<mojom::MessagePort> message_port,
      mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
          message_port_host_receiver);

  // Adds a receiving port port to the given `script_context` in preparation
  // for receiving a message to post to the onMessage event.
  void AddReceiver(ScriptContext* script_context,
                   const PortId& target_port_id,
                   v8::Local<v8::Object> sender,
                   const std::string& event_name);

  void AddReceiverForTesting(
      ScriptContext* script_context,
      const PortId& target_port_id,
      v8::Local<v8::Object> sender,
      const std::string& event_name,
      mojo::PendingAssociatedRemote<mojom::MessagePort>& message_port_remote,
      mojo::PendingAssociatedReceiver<mojom::MessagePortHost>&
          message_port_host_receiver);

  // Delivers a message to the port, either the event listener or in response
  // to the sender, if one exists with the specified `target_port_id`. Returns
  // true if a message was delivered (i.e., an open channel existed), and false
  // otherwise.
  bool DeliverMessage(ScriptContext* script_context,
                      const Message& message,
                      const PortId& target_port_id);

  // Disconnects the port in the context, if one exists with the specified
  // `target_port_id`. Returns true if a port was disconnected (i.e., an open
  // channel existed), and false otherwise.
  bool Disconnect(ScriptContext* script_context,
                  const PortId& port_id,
                  const std::string& error_message);

  // See OneTimeMessageCallbackManager::GetPendingCallbackCountForTest().
  int GetPendingCallbackCountForTest(ScriptContext* script_context,
                                     const PortId& port_id);

 private:
  class OneTimeMessageCallbackManager;

  // Creates a callback to handle a reply from a message listener.
  std::unique_ptr<OneTimeMessageCallback> CreateMessageResponseCallback(
      const PortId& port_id);

  // Creates a callback to handle a rejected promise from a message listener.
  std::unique_ptr<OneTimeMessageCallback> CreatePromiseRejectedCallback(
      const PortId& port_id);

  // Creates a callback to handle when a listener throws an error while it is
  // processing the message dispatched to it.
  std::unique_ptr<OneTimeMessageCallback> CreateListenerErrorCallback(
      const PortId& port_id);

  // Creates a callback to be called after an event is dispatched.
  // `listener_error_callback_id` is provided if
  // extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport is
  // enabled to help cleanup the listener error callback.
  std::unique_ptr<OneTimeMessageCallback> CreateEventDispatchCallback(
      const PortId& port_id,
      std::optional<CallbackID> listener_error_callback_id);

  // Close the message port because all possible message response callbacks have
  // been collected and can no longer be called in v8. Doesn't close the channel
  // because another receiver may reply.
  void OnAllCallbacksCollected(ScriptContext* script_context,
                               v8::Local<v8::Context> context,
                               const PortId& port_id);

  // Helper methods to deliver a message to an opener/receiver.
  bool DeliverMessageToReceiver(ScriptContext* script_context,
                                const Message& message,
                                const PortId& target_port_id);
  bool DeliverReplyToOpener(ScriptContext* script_context,
                            const Message& message,
                            const PortId& target_port_id);

  // Helper methods to disconnect an opener/receiver.
  bool DisconnectReceiver(ScriptContext* script_context, const PortId& port_id);
  bool DisconnectOpener(ScriptContext* script_context,
                        const PortId& port_id,
                        const std::string& error_message);

  // Closes the receiver message port and cleans up all the port's state if
  // `close_channel` is false. If `close_channel` is true, then we request the
  // entire channel to close. `error` can be provided to provide an error to the
  // message sender when closing the channel.
  void CloseReceiverMessagePortOrChannel(ScriptContext* script_context,
                                         const PortId& port_id,
                                         bool close_channel,
                                         std::optional<std::string> error);

  // Returns the message response from v8 back to the message sender. Triggered
  // the first time a receiver responds to a message. Will immediately send the
  // response if another response or error wasn't already returned to the
  // sender. Otherwise no response is returned.
  void OnOneTimeMessageResponse(const PortId& port_id,
                                gin::Arguments* arguments);

  // Returns the JS `.message` property from `possible_error_value`.
  // If `possible_error_value->IsNativeError` is not true, the message cannot be
  // found, or the message is empty then `std::nullopt` is returned.
  std::optional<std::string> GetErrorMessageFromValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> possible_error_value);

  // Returns the promise reject response from v8 back to the message sender.
  // Triggered the first time  a receiver's returned promise rejects. Will
  // immediately send the error if another response or error wasn't already
  // returned to the sender. Otherwise no error is returned.
  void OnPromiseRejectedResponse(const PortId& port_id,
                                 gin::Arguments* arguments);

  // Returns an error thrown in a listener from v8 back to the message sender.
  // Triggered the first time a listener throws an error synchronously while it
  // is processing the message dispatched to it. Will immediately send the error
  // if another response or error wasn't already returned to the sender.
  // Otherwise no error is returned.
  void OnListenerThrowsError(const PortId& port_id, gin::Arguments* arguments);

  // Called when the messaging event has been dispatched with the result of the
  // listeners.
  // `listener_error_callback_id` is provided if
  // extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport is
  // enabled to help cleanup the listener error callback.
  void OnEventFired(const PortId& port_id,
                    std::optional<CallbackID> listener_error_callback_id,
                    gin::Arguments* arguments);

  // Returns true if any of the listeners responded with `true` or (if enabled)
  // a Promise, indicating they will respond to the call asynchronously. If a
  // Promise is returned, `promise_resolved_function` is attached to its resolve
  // and a reject function is attached to its reject.
  bool CheckAndHandleAsyncListenerReply(
      v8::Isolate* isolate,
      v8::Local<v8::Context> context,
      ScriptContext& script_context,
      v8::Local<v8::Value> result,
      const PortId& port_id,
      v8::Local<v8::Function> promise_resolved_function);

  // The messaging service of the associated bindings system used to close the
  // messaging port and/or channel.
  NativeRendererMessagingService* messaging_service();

  // The associated bindings system. Outlives this object.
  const raw_ptr<NativeExtensionBindingsSystem> bindings_system_;

  std::unique_ptr<OneTimeMessageCallbackManager> callback_manager_;

  base::WeakPtrFactory<OneTimeMessageHandler> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_MESSAGING_ONE_TIME_MESSAGE_HANDLER_H_
