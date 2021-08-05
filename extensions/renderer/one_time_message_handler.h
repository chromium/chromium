// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_ONE_TIME_MESSAGE_HANDLER_H_
#define EXTENSIONS_RENDERER_ONE_TIME_MESSAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace extensions {
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
  ~OneTimeMessageHandler();

  // Returns true if the given context has a port with the specified id.
  bool HasPort(ScriptContext* script_context, const PortId& port_id);

  // Initiates a flow to send a message from the given |script_context|.
  void SendMessage(ScriptContext* script_context,
                   const PortId& new_port_id,
                   const MessageTarget& target_id,
                   const std::string& method_name,
                   const Message& message,
                   v8::Local<v8::Function> response_callback);

  // Adds a receiving port port to the given |script_context| in preparation
  // for receiving a message to post to the onMessage event.
  void AddReceiver(ScriptContext* script_context,
                   const PortId& target_port_id,
                   v8::Local<v8::Object> sender,
                   const std::string& event_name);

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

  // Triggered when the callback to reply is garbage collected.
  void OnResponseCallbackCollected(ScriptContext* script_context,
                                   const PortId& port_id);

  // Called when the messaging event has been dispatched with the result of the
  // listeners.
  void OnEventFired(const PortId& port_id,
                    v8::Local<v8::Context> context,
                    v8::MaybeLocal<v8::Value> result);

  // The associated bindings system. Outlives this object.
  NativeExtensionBindingsSystem* const bindings_system_;

  base::WeakPtrFactory<OneTimeMessageHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OneTimeMessageHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_ONE_TIME_MESSAGE_HANDLER_H_
