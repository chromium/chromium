// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_MESSAGING_NATIVE_RENDERER_MESSAGING_SERVICE_H_
#define EXTENSIONS_RENDERER_API_MESSAGING_NATIVE_RENDERER_MESSAGING_SERVICE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/message_port.mojom.h"
#include "extensions/renderer/api/messaging/gin_port.h"
#include "extensions/renderer/api/messaging/one_time_message_handler.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "gin/handle.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "v8/include/v8-forward.h"

namespace content {
class RenderFrame;
}

namespace extensions {

namespace mojom {
enum class ChannelType;
enum class SerializationFormat;
}  // namespace mojom

class NativeExtensionBindingsSystem;
class ScriptContextSetIterable;
struct Message;
struct MessageTarget;
struct PortId;

// The messaging service to handle dispatching extension messages and connection
// events to different contexts.
// This primarily handles long-lived port-based communications (like
// runtime.connect). A basic flow will create an "opener" port and one ore more
// "receiver" ports in different contexts (and possibly processes). This class
// manages the communication with the browser to forward these messages along.
// From JavaScript, a basic flow would be:
// // page1.js
// var port = chrome.runtime.connect();
// port.onMessage.addListener(function() { <handle message> });
// port.postMessage('hi!');
// <eventually> port.disconnect();
// // page2.js
// chrome.runtime.onConnect.addListener(function(port) {
//   port.onMessage.addListener(function() { <handle message> });
//   port.postMessage('hey!');
// });
// This causes the following steps in the messaging service:
// Connection:
// * connect() triggers OpenChannelToExtension, which notifies the browser of
//   a new connection.
// * The browser dispatches OnConnect messages to different renderers. If a
//   renderer has a listener, it will respond with an OpenMessagePort message.
//   If no renderer has a listener, the browser will close the port.
// Message Posting
// * Calls to postMessage() trigger a PostMessageToPort messge to the browser.
// * The browser sends a DeliverMessage message to listening renderers. These
//   then dispatch the onMessage event to listeners.
// Disconnecting
// * disconnect() calls result in sending a CloseMessagePort message to the
//   browser.
// * The browser then sends a DispatchOnDisconnect message to other renderers,
//   which triggers the onDisconnect() event.
// TODO(devlin): This is a pretty large comment for a class, and it documents
// browser/renderer interaction. I wonder if this would be better in a
// messaging.md document?
class NativeRendererMessagingService : public GinPort::Delegate {
 public:
  explicit NativeRendererMessagingService(
      NativeExtensionBindingsSystem* bindings_system);

  NativeRendererMessagingService(const NativeRendererMessagingService&) =
      delete;
  NativeRendererMessagingService& operator=(
      const NativeRendererMessagingService&) = delete;

  ~NativeRendererMessagingService() override;

  using ConnectCallback = base::OnceCallback<void(bool success)>;
  using TabConnectionInfo = mojom::TabConnectionInfo;
  using ExternalConnectionInfo = mojom::ExternalConnectionInfo;
  // Dispatches the onConnect content script messaging event to some contexts
  // in |context_set|. If |restrict_to_render_frame| is specified, only contexts
  // in that render frame will receive the message.
  void DispatchOnConnect(
      ScriptContextSetIterable* context_set,
      const PortId& target_port_id,
      mojom::ChannelType channel_type,
      const std::string& channel_name,
      const TabConnectionInfo& tab_info,
      const ExternalConnectionInfo& external_info,
      mojo::PendingAssociatedReceiver<extensions::mojom::MessagePort> port,
      mojo::PendingAssociatedRemote<extensions::mojom::MessagePortHost>
          port_host,
      content::RenderFrame* restrict_to_render_frame,
      ConnectCallback);

  // Delivers a message sent using content script messaging to some of the
  // contexts in |bindings_context_set|. If |restrict_to_render_frame| is
  // specified, only contexts in that render view will receive the message.
  void DeliverMessage(ScriptContextSetIterable* context_set,
                      const PortId& target_port_id,
                      const Message& message,
                      content::RenderFrame* restrict_to_render_frame);

  // Dispatches the onDisconnect event in response to the channel being closed.
  void DispatchOnDisconnect(ScriptContextSetIterable* context_set,
                            const PortId& port_id,
                            const std::string& error_message,
                            content::RenderFrame* restrict_to_render_frame);

  // Creates and opens a new message port in the specified context.
  gin::Handle<GinPort> Connect(ScriptContext* script_context,
                               const MessageTarget& target,
                               const std::string& name,
                               mojom::SerializationFormat format);

  // Sends a one-time message, as is used by runtime.sendMessage. Returns a
  // Promise if used in a promise based API call, otherwise returns an empty
  // v8::Local<>.
  v8::Local<v8::Promise> SendOneTimeMessage(
      ScriptContext* script_context,
      const MessageTarget& target,
      mojom::ChannelType channel_type,
      const Message& message,
      binding::AsyncResponseType async_type,
      v8::Local<v8::Function> response_callback);

  // GinPort::Delegate:
  void PostMessageToPort(v8::Local<v8::Context> context,
                         const PortId& port_id,
                         std::unique_ptr<Message> message) override;
  void ClosePort(v8::Local<v8::Context> context,
                 const PortId& port_id) override;

  gin::Handle<GinPort> CreatePortForTesting(
      ScriptContext* script_context,
      const std::string& channel_name,
      const mojom::ChannelType channel_type,
      const PortId& port_id,
      mojo::PendingAssociatedRemote<mojom::MessagePort>& message_port_remote,
      mojo::PendingAssociatedReceiver<mojom::MessagePortHost>&
          message_port_host_receiver);
  gin::Handle<GinPort> GetPortForTesting(ScriptContext* script_context,
                                         const PortId& port_id);
  bool HasPortForTesting(ScriptContext* script_context, const PortId& port_id);

  void BindPortForTesting(
      ScriptContext* script_context,
      const PortId& port_id,
      mojo::PendingAssociatedRemote<mojom::MessagePort>& message_port_remote,
      mojo::PendingAssociatedReceiver<mojom::MessagePortHost>&
          message_port_host_receiver);

  void CloseMessagePort(ScriptContext* script_context,
                        const PortId& port_id,
                        bool close_channel);
  // Returns the associated MessagePortHost. This method asserts that it
  // exists.
  mojom::MessagePortHost* GetMessagePortHost(ScriptContext* script_context,
                                             const PortId& port_id);
  // Similar to `GetMessagePortHost` but will return null if the port
  // no longer exists.
  mojom::MessagePortHost* GetMessagePortHostIfExists(
      ScriptContext* script_context,
      const PortId& port_id);

 private:
  class MessagePortScope;
  class RenderFrameMessagePorts;
  friend class OneTimeMessageHandlerTest;

  // Helpers for the public methods to perform the action in a single
  // ScriptContext.
  void DispatchOnConnectToScriptContext(const PortId& target_port_id,
                                        mojom::ChannelType channel_type,
                                        const std::string& channel_name,
                                        const TabConnectionInfo& source,
                                        const ExternalConnectionInfo& info,
                                        bool* port_created,
                                        ScriptContext* script_context);
  void DeliverMessageToScriptContext(const Message& message,
                                     const PortId& target_port_id,
                                     ScriptContext* script_context);
  void DeliverMessageToWorker(const Message& message,
                              const PortId& target_port_id,
                              ScriptContext* script_context);
  void DeliverMessageToBackgroundPage(const Message& message,
                                      const PortId& target_port_id,
                                      ScriptContext* script_context);
  void DispatchOnDisconnectToScriptContext(const PortId& port_id,
                                           const std::string& error_message,
                                           ScriptContext* script_context);

  // Returns true if the given |script_context| has a port with the given
  // |port_id|.
  bool ContextHasMessagePort(ScriptContext* script_context,
                             const PortId& port_id);

  // Dispatches the onConnect event to listeners in the given |script_context|.
  void DispatchOnConnectToListeners(ScriptContext* script_context,
                                    const PortId& target_port_id,
                                    const ExtensionId& target_extension_id,
                                    mojom::ChannelType channel_type,
                                    const std::string& channel_name,
                                    const TabConnectionInfo& source,
                                    const ExternalConnectionInfo& info,
                                    const std::string& event_name);

  // Dispatches the onMessage event to listeners in the given |script_context|.
  // This will only be called if the context has a port with the given id.
  void DispatchOnMessageToListeners(ScriptContext* script_context,
                                    const Message& message,
                                    const PortId& target_port_id);

  // Dispatches the onDisconnect event to listeners in the given
  // |script_context|. This will only be called if the context has a port
  // with the given id.
  void DispatchOnDisconnectToListeners(ScriptContext* script_context,
                                       const PortId& port_id,
                                       const std::string& error);

  // Creates a new port in the given context, with the specified |channel_name|
  // and |port_id|. Assumes no such port exists.
  gin::Handle<GinPort> CreatePort(ScriptContext* script_context,
                                  const std::string& channel_name,
                                  const mojom::ChannelType channel_type,
                                  const PortId& port_id);

  // Returns the port with the given |port_id| in the given |script_context|;
  // requires that such a port exists.
  gin::Handle<GinPort> GetPort(ScriptContext* script_context,
                               const PortId& port_id);

  MessagePortScope* GetMessagePortScope(content::RenderFrame* render_frame);
  base::SafeRef<NativeRendererMessagingService> AsSafeRef();

  // The associated bindings system; guaranteed to outlive this object.
  const raw_ptr<NativeExtensionBindingsSystem> bindings_system_;

  OneTimeMessageHandler one_time_message_handler_;
  std::unique_ptr<MessagePortScope> default_scope_;
  base::WeakPtrFactory<NativeRendererMessagingService> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_MESSAGING_NATIVE_RENDERER_MESSAGING_SERVICE_H_
