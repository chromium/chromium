// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/messaging/native_renderer_messaging_service.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/common/utils/extension_utils.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/api/messaging/messaging_util.h"
#include "extensions/renderer/api_activity_logger.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/get_per_context_data.h"
#include "extensions/renderer/extension_interaction_provider.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/script_context_set_iterable.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/per_context_data.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_scoped_window_focus_allowed_indicator.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-persistent-handle.h"

using blink::mojom::UserActivationNotificationType;

namespace extensions {

namespace {

struct MessagingPerContextData : public base::SupportsUserData::Data {
  static constexpr char kPerContextDataKey[] =
      "extension_messaging_per_context_data";

  // All the port objects that exist in this context.
  std::map<PortId, v8::Global<v8::Object>> ports;

  // The next available context-specific port id.
  int next_port_id = 0;
};

constexpr char MessagingPerContextData::kPerContextDataKey[];

bool ScriptContextIsValid(ScriptContext* script_context) {
  // TODO(devlin): This is in lieu of a similar check in the JS bindings that
  // null-checks ScriptContext::GetRenderFrame(). This is good because it
  // removes the reliance on RenderFrames (which make testing very difficult),
  // but is it fully analogous? It should be close enough, since the browser
  // has to deal with frames potentially disappearing before the IPC arrives
  // anyway.
  return script_context->is_valid();
}

}  // namespace

// This class implements the mojo messaging hooks. Since the
// NativeRendererMessagingService is shared by everything on the same thread
// we want a separate object so the mojo channels are organized based on
// their "scope", whether that be a Worker or a Frame.
class NativeRendererMessagingService::MessagePortScope
    : public mojom::MessagePort {
 public:
  explicit MessagePortScope(
      base::SafeRef<NativeRendererMessagingService> messaging_service)
      : messaging_service_(messaging_service) {}

  ~MessagePortScope() override = default;

  // MessagePort overrides:
  void DispatchDisconnect(const std::string& error) override {
    auto target_port_id = message_port_dispatchers_.current_context();
    messaging_service_->DispatchOnDisconnect(
        messaging_service_->bindings_system_->delegate()->GetScriptContextSet(),
        target_port_id, error,
        /*restrict_to_render_frame=*/GetRestrictToRenderFrame());
  }

  void DeliverMessage(extensions::Message message) override {
    auto target_port_id = message_port_dispatchers_.current_context();
    messaging_service_->DeliverMessage(
        messaging_service_->bindings_system_->delegate()->GetScriptContextSet(),
        target_port_id, message,
        /*restrict_to_render_frame=*/GetRestrictToRenderFrame());
  }

  mojo::ReceiverId AddPort(
      const PortId& target_port_id,
      mojo::PendingAssociatedReceiver<extensions::mojom::MessagePort> port,
      mojo::PendingAssociatedRemote<extensions::mojom::MessagePortHost>
          port_host) {
    auto receiver_id =
        message_port_dispatchers_.Add(this, std::move(port), target_port_id);
    CHECK(!base::Contains(message_port_hosts_, target_port_id));
    auto& bound_port_host = message_port_hosts_[target_port_id] =
        mojo::AssociatedRemote(std::move(port_host));
    bound_port_host.set_disconnect_handler(
        base::BindOnce(&MessagePortScope::PortDisconnected,
                       base::Unretained(this), target_port_id));
    return receiver_id;
  }

  void BindNewMessagePort(
      const PortId& port_id,
      mojo::PendingAssociatedRemote<mojom::MessagePort>& message_port_remote,
      mojo::PendingAssociatedReceiver<mojom::MessagePortHost>&
          message_port_host_receiver) {
    mojo::PendingAssociatedReceiver<mojom::MessagePort> message_port_receiver;
    mojo::PendingAssociatedRemote<mojom::MessagePortHost>
        message_port_host_remote;
    message_port_remote =
        message_port_receiver.InitWithNewEndpointAndPassRemote();
    message_port_host_receiver =
        message_port_host_remote.InitWithNewEndpointAndPassReceiver();
    message_port_dispatchers_.Add(this, std::move(message_port_receiver),
                                  port_id);
    CHECK(!base::Contains(message_port_hosts_, port_id));
    auto& bound_port_host = message_port_hosts_[port_id] =
        mojo::AssociatedRemote(std::move(message_port_host_remote));
    bound_port_host.set_disconnect_handler(base::BindOnce(
        &MessagePortScope::PortDisconnected, base::Unretained(this), port_id));
  }

  void Remove(mojo::ReceiverId receiver_id, const PortId& target_port_id) {
    message_port_dispatchers_.Remove(receiver_id);
    PortDisconnected(target_port_id);
  }

  void PortDisconnected(const PortId& port_id) {
    message_port_hosts_.erase(port_id);
  }

  size_t GetNumHosts() { return message_port_hosts_.size(); }

  base::WeakPtr<MessagePortScope> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  virtual content::RenderFrame* GetRestrictToRenderFrame() { return nullptr; }

  void CloseMessagePort(const PortId& port_id, bool close_channel) {
    // BFCache can disconnect the mojo pipe but leave the GinPort thinking
    // it is open.
    if (!HasPort(port_id)) {
      return;
    }
    GetMessagePortHost(port_id)->ClosePort(close_channel);
    message_port_hosts_.erase(port_id);
  }

  bool HasPort(const PortId& port_id) {
    return base::Contains(message_port_hosts_, port_id);
  }

  mojom::MessagePortHost* GetMessagePortHost(const PortId& port_id) {
    auto* result = GetMessagePortHostIfExists(port_id);
    CHECK(result);
    return result;
  }

  mojom::MessagePortHost* GetMessagePortHostIfExists(const PortId& port_id) {
    auto it = message_port_hosts_.find(port_id);
    if (it == message_port_hosts_.end()) {
      return nullptr;
    }
    return it->second.get();
  }

 private:
  base::SafeRef<NativeRendererMessagingService> messaging_service_;
  std::map<PortId, mojo::AssociatedRemote<mojom::MessagePortHost>>
      message_port_hosts_;
  mojo::AssociatedReceiverSet<mojom::MessagePort, PortId>
      message_port_dispatchers_;
  base::WeakPtrFactory<MessagePortScope> weak_ptr_factory_{this};
};

const void* const kRenderFrameMessagePortsKey = &kRenderFrameMessagePortsKey;

// This class extensions MessagePortScope but associates the storage of
// the class with UserData on each RenderFrame.
class NativeRendererMessagingService::RenderFrameMessagePorts
    : public base::SupportsUserData::Data,
      public NativeRendererMessagingService::MessagePortScope {
 public:
  RenderFrameMessagePorts(
      base::SafeRef<NativeRendererMessagingService> messaging_service,
      content::RenderFrame* render_frame)
      : MessagePortScope(std::move(messaging_service)),
        render_frame_(render_frame) {}
  ~RenderFrameMessagePorts() override = default;

  static RenderFrameMessagePorts* GetOrCreate(
      NativeRendererMessagingService* messaging_service,
      content::RenderFrame* render_frame) {
    RenderFrameMessagePorts* per_frame_data =
        static_cast<RenderFrameMessagePorts*>(
            render_frame->GetUserData(kRenderFrameMessagePortsKey));
    if (!per_frame_data) {
      auto created_data = std::make_unique<RenderFrameMessagePorts>(
          messaging_service->AsSafeRef(), render_frame);
      per_frame_data = created_data.get();
      render_frame->SetUserData(kRenderFrameMessagePortsKey,
                                std::move(created_data));
    }
    return per_frame_data;
  }

  content::RenderFrame* GetRestrictToRenderFrame() override {
    return render_frame_;
  }

 private:
  // Safe raw ptr since this object is UserData owned by RenderFrame itself.
  raw_ptr<content::RenderFrame> render_frame_;
};

NativeRendererMessagingService::NativeRendererMessagingService(
    NativeExtensionBindingsSystem* bindings_system)
    : bindings_system_(bindings_system),
      one_time_message_handler_(bindings_system) {
  default_scope_ = std::make_unique<MessagePortScope>(AsSafeRef());
}
NativeRendererMessagingService::~NativeRendererMessagingService() = default;

void NativeRendererMessagingService::DispatchOnConnect(
    ScriptContextSetIterable* context_set,
    const PortId& target_port_id,
    mojom::ChannelType channel_type,
    const std::string& channel_name,
    const TabConnectionInfo& source,
    const ExternalConnectionInfo& info,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePort> port,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePortHost> port_host,
    content::RenderFrame* restrict_to_render_frame,
    ConnectCallback callback) {
  DCHECK(!target_port_id.is_opener);
  auto scope = GetMessagePortScope(restrict_to_render_frame)->GetWeakPtr();
  auto receiver_id =
      scope->AddPort(target_port_id, std::move(port), std::move(port_host));

  bool port_created = false;
  context_set->ForEach(
      GenerateHostIdFromExtensionId(info.target_id), restrict_to_render_frame,
      base::BindRepeating(
          &NativeRendererMessagingService::DispatchOnConnectToScriptContext,
          base::Unretained(this), target_port_id, channel_type, channel_name,
          std::ref(source), std::ref(info), &port_created));
  // Note: |restrict_to_render_frame| may have been deleted at this point!

  if (!port_created && scope) {
    scope->Remove(receiver_id, target_port_id);
  }

  // Note: |restrict_to_render_frame| may have been deleted at this point!
  std::move(callback).Run(port_created);
}

void NativeRendererMessagingService::DeliverMessage(
    ScriptContextSetIterable* context_set,
    const PortId& target_port_id,
    const Message& message,
    content::RenderFrame* restrict_to_render_frame) {
  context_set->ForEach(
      restrict_to_render_frame,
      base::BindRepeating(
          &NativeRendererMessagingService::DeliverMessageToScriptContext,
          base::Unretained(this), message, target_port_id));
}

void NativeRendererMessagingService::DispatchOnDisconnect(
    ScriptContextSetIterable* context_set,
    const PortId& port_id,
    const std::string& error_message,
    content::RenderFrame* restrict_to_render_frame) {
  context_set->ForEach(
      restrict_to_render_frame,
      base::BindRepeating(
          &NativeRendererMessagingService::DispatchOnDisconnectToScriptContext,
          base::Unretained(this), port_id, error_message));
}

gin::Handle<GinPort> NativeRendererMessagingService::Connect(
    ScriptContext* script_context,
    const MessageTarget& target,
    const std::string& channel_name,
    mojom::SerializationFormat format) {
  if (!ScriptContextIsValid(script_context))
    return gin::Handle<GinPort>();

  MessagingPerContextData* data = GetPerContextData<MessagingPerContextData>(
      script_context->v8_context(), kCreateIfMissing);
  if (!data)
    return gin::Handle<GinPort>();

  MessagePortScope* scope =
      GetMessagePortScope(script_context->GetRenderFrame());
  bool is_opener = true;
  mojo::PendingAssociatedRemote<mojom::MessagePort> messsage_port;
  mojo::PendingAssociatedReceiver<mojom::MessagePortHost> messsage_port_host;
  mojom::ChannelType channel_type = target.type == MessageTarget::NATIVE_APP
                                        ? mojom::ChannelType::kNative
                                        : mojom::ChannelType::kConnect;
  gin::Handle<GinPort> port =
      CreatePort(script_context, channel_name, channel_type,
                 PortId(script_context->context_id(), data->next_port_id++,
                        is_opener, format));
  scope->BindNewMessagePort(port->port_id(), messsage_port, messsage_port_host);

  bindings_system_->GetIPCMessageSender()->SendOpenMessageChannel(
      script_context, port->port_id(), target, channel_type, channel_name,
      std::move(messsage_port), std::move(messsage_port_host));
  return port;
}

v8::Local<v8::Promise> NativeRendererMessagingService::SendOneTimeMessage(
    ScriptContext* script_context,
    const MessageTarget& target,
    mojom::ChannelType channel_type,
    const Message& message,
    binding::AsyncResponseType async_type,
    v8::Local<v8::Function> response_callback) {
  if (!ScriptContextIsValid(script_context))
    return v8::Local<v8::Promise>();

  MessagingPerContextData* data = GetPerContextData<MessagingPerContextData>(
      script_context->v8_context(), kCreateIfMissing);

  MessagePortScope* scope =
      GetMessagePortScope(script_context->GetRenderFrame());
  bool is_opener = true;

  // TODO(crbug.com/40321352): Instead of inferring the
  // mojom::SerializationFormat from Message, it'd be better to have the clients
  // pass it directly. This is because, in case of `kStructuredCloned` to
  // `kJson` fallback, the format for the ports will also be `kJson`. This is
  // inconsistent with what we do for ports for long-lived channels where the
  // port's `mojom::SerializationFormat` is always the same as that passed by
  // messaging clients and is independent of any fallback behavior.
  PortId port_id(script_context->context_id(), data->next_port_id++, is_opener,
                 message.format);
  mojo::PendingAssociatedRemote<mojom::MessagePort> message_port;
  mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
      message_port_host_receiver;
  mojom::MessagePortHost* message_port_host = nullptr;
  scope->BindNewMessagePort(port_id, message_port, message_port_host_receiver);
  message_port_host = scope->GetMessagePortHost(port_id);

  return one_time_message_handler_.SendMessage(
      script_context, port_id, target, channel_type, message, async_type,
      response_callback, message_port_host, std::move(message_port),
      std::move(message_port_host_receiver));
}

void NativeRendererMessagingService::PostMessageToPort(
    v8::Local<v8::Context> context,
    const PortId& port_id,
    std::unique_ptr<Message> message) {
  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  CHECK(script_context);
  if (!ScriptContextIsValid(script_context))
    return;

  auto* scope = GetMessagePortScope(script_context->GetRenderFrame());
  // BFCache can disconnect the mojo pipe but leave the GinPort thinking
  // it is open.
  if (!scope->HasPort(port_id)) {
    return;
  }
  scope->GetMessagePortHost(port_id)->PostMessage(*message);
}

void NativeRendererMessagingService::ClosePort(v8::Local<v8::Context> context,
                                               const PortId& port_id) {
  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  CHECK(script_context);

  MessagingPerContextData* data = GetPerContextData<MessagingPerContextData>(
      script_context->v8_context(), kDontCreateIfMissing);
  if (!data)
    return;

  size_t erased = data->ports.erase(port_id);
  DCHECK_GT(erased, 0u);

  if (!ScriptContextIsValid(script_context))
    return;

  CloseMessagePort(script_context, port_id, /*close_channel=*/true);
}

gin::Handle<GinPort> NativeRendererMessagingService::CreatePortForTesting(
    ScriptContext* script_context,
    const std::string& channel_name,
    const mojom::ChannelType channel_type,
    const PortId& port_id,
    mojo::PendingAssociatedRemote<mojom::MessagePort>& message_port_remote,
    mojo::PendingAssociatedReceiver<mojom::MessagePortHost>&
        message_port_host_receiver) {
  BindPortForTesting(script_context, port_id, message_port_remote,  // IN-TEST
                     message_port_host_receiver);
  return CreatePort(script_context, channel_name, channel_type, port_id);
}

gin::Handle<GinPort> NativeRendererMessagingService::GetPortForTesting(
    ScriptContext* script_context,
    const PortId& port_id) {
  return GetPort(script_context, port_id);
}

bool NativeRendererMessagingService::HasPortForTesting(
    ScriptContext* script_context,
    const PortId& port_id) {
  return ContextHasMessagePort(script_context, port_id);
}

void NativeRendererMessagingService::BindPortForTesting(
    ScriptContext* script_context,
    const PortId& port_id,
    mojo::PendingAssociatedRemote<mojom::MessagePort>& message_port_remote,
    mojo::PendingAssociatedReceiver<mojom::MessagePortHost>&
        message_port_host_receiver) {
  auto* scope = GetMessagePortScope(script_context->GetRenderFrame());
  scope->BindNewMessagePort(port_id, message_port_remote,
                            message_port_host_receiver);
}

void NativeRendererMessagingService::DispatchOnConnectToScriptContext(
    const PortId& target_port_id,
    mojom::ChannelType channel_type,
    const std::string& channel_name,
    const TabConnectionInfo& source,
    const ExternalConnectionInfo& info,
    bool* port_created,
    ScriptContext* script_context) {
  // If the channel was opened by this same context, ignore it. This should only
  // happen when messages are sent to an entire process (rather than a single
  // frame) as an optimization; otherwise the browser process filters this out.
  if (script_context->context_id() == target_port_id.context_id)
    return;

  // First, determine the event we'll use to connect.
  std::string target_extension_id = script_context->GetExtensionID();
  std::string event_name = messaging_util::GetEventForChannel(
      info.source_endpoint, target_extension_id, channel_type);

  // If there are no listeners for the given event, then we know the port won't
  // be used in this context.
  if (!bindings_system_->HasEventListenerInContext(event_name,
                                                   script_context)) {
    return;
  }
  *port_created = true;

  DispatchOnConnectToListeners(script_context, target_port_id,
                               target_extension_id, channel_type, channel_name,
                               source, info, event_name);
}

void NativeRendererMessagingService::DeliverMessageToScriptContext(
    const Message& message,
    const PortId& target_port_id,
    ScriptContext* script_context) {
  if (!ContextHasMessagePort(script_context, target_port_id)) {
    return;
  }

  if (script_context->IsForServiceWorker()) {
    DeliverMessageToWorker(message, target_port_id, script_context);
  } else {
    DeliverMessageToBackgroundPage(message, target_port_id, script_context);
  }
}

void NativeRendererMessagingService::DeliverMessageToWorker(
    const Message& message,
    const PortId& target_port_id,
    ScriptContext* script_context) {
  // Note |scoped_extension_interaction| requires a HandleScope.
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);
  std::unique_ptr<InteractionProvider::Scope> scoped_extension_interaction;
  if (message.user_gesture) {
    // TODO(crbug.com/41467311): Add logging for privilege level for
    // sender and receiver and decide if want to allow unprivileged to
    // privileged support.
    scoped_extension_interaction =
        ExtensionInteractionProvider::Scope::ForWorker(
            script_context->v8_context());
  }

  DispatchOnMessageToListeners(script_context, message, target_port_id);
}

void NativeRendererMessagingService::DeliverMessageToBackgroundPage(
    const Message& message,
    const PortId& target_port_id,
    ScriptContext* script_context) {
  std::unique_ptr<blink::WebScopedWindowFocusAllowedIndicator>
      allow_window_focus;
  if (message.user_gesture && script_context->web_frame()) {
    bool sender_is_privileged = message.from_privileged_context;
    bool receiver_is_privileged =
        script_context->context_type() ==
        extensions::mojom::ContextType::kPrivilegedExtension;
    UserActivationNotificationType notification_type;
    if (sender_is_privileged && receiver_is_privileged) {
      notification_type =
          UserActivationNotificationType::kExtensionMessagingBothPrivileged;
    } else if (sender_is_privileged && !receiver_is_privileged) {
      notification_type =
          UserActivationNotificationType::kExtensionMessagingSenderPrivileged;
    } else if (!sender_is_privileged && receiver_is_privileged) {
      notification_type =
          UserActivationNotificationType::kExtensionMessagingReceiverPrivileged;
    } else /* !sender_is_privileged && !receiver_is_privileged */ {
      notification_type =
          UserActivationNotificationType::kExtensionMessagingNeitherPrivileged;
    }

    blink::WebLocalFrame* frame = script_context->web_frame();
    bool has_unrestricted_user_activation =
        frame->HasTransientUserActivation() &&
        !frame->LastActivationWasRestricted();
    // IMPORTANT: Only notify the web frame of a user activation if there isn't
    // already an unrestricted user activation. Otherwise, this will override
    // the currently-active, more-privileged activation. See
    // https://crbug.com/355266358.
    // TODO(https://crbug.com/356418716): Ideally, this would be unnecessary,
    // and the blink API would properly track these activations independently.
    // Remove this if-check when that happens.
    if (!has_unrestricted_user_activation) {
      script_context->web_frame()->NotifyUserActivation(notification_type);
    }

    blink::WebDocument document = script_context->web_frame()->GetDocument();
    allow_window_focus =
        std::make_unique<blink::WebScopedWindowFocusAllowedIndicator>(
            &document);
  }

  DispatchOnMessageToListeners(script_context, message, target_port_id);
}

void NativeRendererMessagingService::DispatchOnDisconnectToScriptContext(
    const PortId& port_id,
    const std::string& error_message,
    ScriptContext* script_context) {
  if (!ContextHasMessagePort(script_context, port_id))
    return;

  DispatchOnDisconnectToListeners(script_context, port_id, error_message);
}

bool NativeRendererMessagingService::ContextHasMessagePort(
    ScriptContext* script_context,
    const PortId& port_id) {
  if (one_time_message_handler_.HasPort(script_context, port_id))
    return true;
  v8::HandleScope handle_scope(script_context->isolate());
  MessagingPerContextData* data = GetPerContextData<MessagingPerContextData>(
      script_context->v8_context(), kDontCreateIfMissing);
  return data && base::Contains(data->ports, port_id);
}

void NativeRendererMessagingService::DispatchOnConnectToListeners(
    ScriptContext* script_context,
    const PortId& target_port_id,
    const ExtensionId& target_extension_id,
    mojom::ChannelType channel_type,
    const std::string& channel_name,
    const TabConnectionInfo& source,
    const ExternalConnectionInfo& info,
    const std::string& event_name) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = script_context->v8_context();
  v8::Context::Scope context_scope(v8_context);

  gin::DataObjectBuilder sender_builder(isolate);
  if (info.source_endpoint.extension_id)
    sender_builder.Set("id", *info.source_endpoint.extension_id);
  if (info.source_endpoint.native_app_name) {
    sender_builder.Set("nativeApplication",
                       *info.source_endpoint.native_app_name);
  }
  if (!info.source_url.is_empty())
    sender_builder.Set("url", info.source_url.spec());
  if (info.source_origin)
    sender_builder.Set("origin", info.source_origin->Serialize());
  if (source.frame_id >= 0) {
    sender_builder.Set("frameId", source.frame_id);
  }
  if (!source.document_id.empty()) {
    sender_builder.Set("documentId", source.document_id);
  }
  if (!source.document_lifecycle.empty()) {
    sender_builder.Set("documentLifecycle", source.document_lifecycle);
  }

  const Extension* extension = script_context->extension();
  if (extension) {
    if (!source.tab.empty() && !extension->is_platform_app()) {
      sender_builder.Set("tab", content::V8ValueConverter::Create()->ToV8Value(
                                    source.tab, v8_context));
    }

    ExternallyConnectableInfo* externally_connectable =
        ExternallyConnectableInfo::Get(extension);
    if (externally_connectable &&
        externally_connectable->accepts_tls_channel_id) {
      sender_builder.Set("tlsChannelId", std::string());
    }

    if (info.guest_process_id != content::kInvalidChildProcessUniqueId) {
      CHECK(Manifest::IsComponentLocation(extension->location()))
          << "GuestProcessId can only be exposed to component extensions.";
      sender_builder.Set("guestProcessId", info.guest_process_id)
          .Set("guestRenderFrameRoutingId", info.guest_render_frame_routing_id);
    }
  }

  v8::Local<v8::Object> sender = sender_builder.Build();

  if (channel_type == mojom::ChannelType::kSendRequest ||
      channel_type == mojom::ChannelType::kSendMessage) {
    one_time_message_handler_.AddReceiver(script_context, target_port_id,
                                          sender, event_name);
  } else {
    CHECK(channel_type == mojom::ChannelType::kConnect ||
          channel_type == mojom::ChannelType::kNative);
    gin::Handle<GinPort> port =
        CreatePort(script_context, channel_name, channel_type, target_port_id);
    port->SetSender(v8_context, sender);
    v8::LocalVector<v8::Value> args(isolate, {port.ToV8()});
    bindings_system_->api_system()->event_handler()->FireEventInContext(
        event_name, v8_context, &args, nullptr, JSRunner::ResultCallback());
  }
  // Note: Arbitrary JS may have run; the context may now be deleted.

  if (binding::IsContextValid(v8_context) &&
      APIActivityLogger::IsLoggingEnabled()) {
    base::Value::List list;
    list.reserve(2u);
    if (info.source_endpoint.extension_id)
      list.Append(*info.source_endpoint.extension_id);
    else if (info.source_endpoint.native_app_name)
      list.Append(*info.source_endpoint.native_app_name);
    else
      list.Append(base::Value());

    if (!info.source_url.is_empty())
      list.Append(info.source_url.spec());
    else
      list.Append(base::Value());

    APIActivityLogger::LogEvent(bindings_system_->GetIPCMessageSender(),
                                script_context, event_name, std::move(list));
  }
}

void NativeRendererMessagingService::DispatchOnMessageToListeners(
    ScriptContext* script_context,
    const Message& message,
    const PortId& target_port_id) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(script_context->v8_context());

  if (one_time_message_handler_.DeliverMessage(script_context, message,
                                               target_port_id)) {
    return;
  }

  gin::Handle<GinPort> port = GetPort(script_context, target_port_id);
  DCHECK(!port.IsEmpty());

  port->DispatchOnMessage(script_context->v8_context(), message);
  // Note: Arbitrary JS may have run; the context may now be deleted.
}

void NativeRendererMessagingService::DispatchOnDisconnectToListeners(
    ScriptContext* script_context,
    const PortId& port_id,
    const std::string& error_message) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = script_context->v8_context();
  v8::Context::Scope context_scope(v8_context);

  if (one_time_message_handler_.Disconnect(script_context, port_id,
                                           error_message)) {
    return;
  }

  gin::Handle<GinPort> port = GetPort(script_context, port_id);
  DCHECK(!port.IsEmpty());
  if (!error_message.empty()) {
    // TODO(devlin): Subtle: If the JS event to disconnect the port happens
    // asynchronously because JS is suspended, this last error won't be
    // correctly set for listeners. Given this exceedingly rare, and shouldn't
    // behave too strangely, this is somewhat low priority.
    bindings_system_->api_system()->request_handler()->last_error()->SetError(
        v8_context, error_message);
  }

  port->DispatchOnDisconnect(v8_context);
  // Note: Arbitrary JS may have run; the context may now be deleted.

  if (!binding::IsContextValid(v8_context))
    return;

  if (!error_message.empty()) {
    bindings_system_->api_system()->request_handler()->last_error()->ClearError(
        v8_context, true);
  }

  MessagingPerContextData* data = GetPerContextData<MessagingPerContextData>(
      v8_context, kDontCreateIfMissing);
  DCHECK(data);
  data->ports.erase(port_id);
}

gin::Handle<GinPort> NativeRendererMessagingService::CreatePort(
    ScriptContext* script_context,
    const std::string& channel_name,
    const mojom::ChannelType channel_type,
    const PortId& port_id) {
  // Note: no HandleScope because it would invalidate the gin::Handle::wrapper_.
  v8::Isolate* isolate = script_context->isolate();
  v8::Local<v8::Context> context = script_context->v8_context();
  // Note: needed because gin::CreateHandle infers the context from the active
  // context on the isolate.
  v8::Context::Scope context_scope(context);

  // If this port is an opener, then it should have been created in this
  // context. Otherwise, it should have been created in another context, because
  // we don't support intra-context message passing.
  if (port_id.is_opener)
    DCHECK_EQ(port_id.context_id, script_context->context_id());
  else
    DCHECK_NE(port_id.context_id, script_context->context_id());

  MessagingPerContextData* data =
      GetPerContextData<MessagingPerContextData>(context, kCreateIfMissing);
  DCHECK(data);
  DCHECK(!base::Contains(data->ports, port_id));

  gin::Handle<GinPort> port_handle = gin::CreateHandle(
      isolate,
      new GinPort(context, port_id, channel_name, channel_type,
                  bindings_system_->api_system()->event_handler(), this));

  v8::Local<v8::Object> port_object = port_handle.ToV8().As<v8::Object>();
  data->ports[port_id].Reset(isolate, port_object);

  return port_handle;
}

gin::Handle<GinPort> NativeRendererMessagingService::GetPort(
    ScriptContext* script_context,
    const PortId& port_id) {
  // Note: no HandleScope because it would invalidate the gin::Handle::wrapper_.
  v8::Isolate* isolate = script_context->isolate();
  v8::Local<v8::Context> context = script_context->v8_context();

  MessagingPerContextData* data = GetPerContextData<MessagingPerContextData>(
      script_context->v8_context(), kDontCreateIfMissing);
  DCHECK(data);
  DCHECK(base::Contains(data->ports, port_id));

  GinPort* port = nullptr;
  gin::Converter<GinPort*>::FromV8(context->GetIsolate(),
                                   data->ports[port_id].Get(isolate), &port);
  CHECK(port);

  return gin::CreateHandle(isolate, port);
}

void NativeRendererMessagingService::CloseMessagePort(
    ScriptContext* script_context,
    const PortId& port_id,
    bool close_channel) {
  auto* scope = GetMessagePortScope(script_context->GetRenderFrame());
  scope->CloseMessagePort(port_id, close_channel);
}

NativeRendererMessagingService::MessagePortScope*
NativeRendererMessagingService::GetMessagePortScope(
    content::RenderFrame* render_frame) {
  if (render_frame) {
    return RenderFrameMessagePorts::GetOrCreate(this, render_frame);
  }
  return default_scope_.get();
}

mojom::MessagePortHost* NativeRendererMessagingService::GetMessagePortHost(
    ScriptContext* script_context,
    const PortId& port_id) {
  return GetMessagePortScope(script_context->GetRenderFrame())
      ->GetMessagePortHost(port_id);
}

mojom::MessagePortHost*
NativeRendererMessagingService::GetMessagePortHostIfExists(
    ScriptContext* script_context,
    const PortId& port_id) {
  if (!script_context) {
    return nullptr;
  }
  return GetMessagePortScope(script_context->GetRenderFrame())
      ->GetMessagePortHostIfExists(port_id);
}

base::SafeRef<NativeRendererMessagingService>
NativeRendererMessagingService::AsSafeRef() {
  return weak_ptr_factory_.GetSafeRef();
}

}  // namespace extensions
