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
#include "extensions/common/mojom/message_port.mojom.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "v8/include/v8-forward.h"

namespace base {
class Value;
}

namespace gin {
class Arguments;
}

namespace extensions {

namespace mojom {
enum class ChannelType;
}

class NativeExtensionBindingsSystem;
class ScriptContext;
struct Message;
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
class OneTimeMessageHandler {
 public:
  explicit OneTimeMessageHandler(
      NativeExtensionBindingsSystem* bindings_system);

  OneTimeMessageHandler(const OneTimeMessageHandler&) = delete;
  OneTimeMessageHandler& operator=(const OneTimeMessageHandler&) = delete;

  ~OneTimeMessageHandler();

  // Returns true if the given context has a port with the specified id.
  bool HasPort(ScriptContext* script_context, const PortId& port_id);

  // Initiates a flow to send a message from the given |script_context|. Returns
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

  // Adds a receiving port port to the given |script_context| in preparation
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
  // to the sender, if one exists with the specified |target_port_id|. Returns
  // true if a message was delivered (i.e., an open channel existed), and false
  // otherwise.
  bool DeliverMessage(ScriptContext* script_context,
                      const Message& message,
                      const PortId& target_port_id);

  // Disconnects the port in the context, if one exists with the specified
  // |target_port_id|. Returns true if a port was disconnected (i.e., an open
  // channel existed), and false otherwise.
  bool Disconnect(ScriptContext* script_context,
                  const PortId& port_id,
                  const std::string& error_message);

  // Gets the number of pending callbacks on the associated per context data for
  // testing purposes.
  int GetPendingCallbackCountForTest(ScriptContext* script_context);

 private:
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

  // Triggered when a receiver responds to a message.
  void OnOneTimeMessageResponse(const PortId& port_id,
                                gin::Arguments* arguments);

  // Identifier for a `OneTimeMessageCallback` to scope the lifetime for
  // references. `CallbackID` is derived from `OneTimeMessageCallback*`, used in
  // comparison only, and are never deferenced.
  using CallbackID = std::uintptr_t;

  // Triggered when the callback for replying is garbage collected. Used to
  // clean up data that was stored for the callback and for closing the
  // associated message port. |raw_callback| is a raw pointer to the associated
  // OneTimeMessageCallback, needed for finding and erasing it from the
  // OneTimeMessageContextData.
  void OnResponseCallbackCollected(ScriptContext* script_context,
                                   const PortId& port_id,
                                   CallbackID callback_id);

  // Called when the messaging event has been dispatched with the result of the
  // listeners.
  void OnEventFired(const PortId& port_id,
                    v8::Local<v8::Context> context,
                    std::optional<base::Value> result);

  // The associated bindings system. Outlives this object.
  const raw_ptr<NativeExtensionBindingsSystem> bindings_system_;

  base::WeakPtrFactory<OneTimeMessageHandler> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_MESSAGING_ONE_TIME_MESSAGE_HANDLER_H_
