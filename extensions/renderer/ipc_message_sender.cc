// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/ipc_message_sender.h"

#include <map>

#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/event_router.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/message_target.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace extensions {

namespace {

class MainThreadIPCMessageSender : public IPCMessageSender {
 public:
  MainThreadIPCMessageSender() : render_thread_(content::RenderThread::Get()) {}
  ~MainThreadIPCMessageSender() override {}

  void SendRequestIPC(ScriptContext* context,
                      mojom::RequestParamsPtr params) override {
    content::RenderFrame* frame = context->GetRenderFrame();
    if (!frame)
      return;

    int request_id = params->request_id;
    ExtensionFrameHelper::Get(frame)->GetLocalFrameHost()->Request(
        std::move(params),
        base::BindOnce(&MainThreadIPCMessageSender::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), request_id));
  }

  void SendOnRequestResponseReceivedIPC(int request_id) override {}

  void SendAddUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    if (!context->GetExtensionID().empty()) {
      GetEventRouter()->AddListenerForMainThread(
          mojom::EventListenerParam::NewExtensionId(context->GetExtensionID()),
          event_name);
    } else {
      GetEventRouter()->AddListenerForMainThread(
          mojom::EventListenerParam::NewListenerUrl(context->url()),
          event_name);
    }
  }

  void SendRemoveUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    if (!context->GetExtensionID().empty()) {
      GetEventRouter()->RemoveListenerForMainThread(
          mojom::EventListenerParam::NewExtensionId(context->GetExtensionID()),
          event_name);
    } else {
      GetEventRouter()->RemoveListenerForMainThread(
          mojom::EventListenerParam::NewListenerUrl(context->url()),
          event_name);
    }
  }

  void SendAddUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_AddLazyListener(
        context->GetExtensionID(), event_name));
  }

  void SendRemoveUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_RemoveLazyListener(
        context->GetExtensionID(), event_name));
  }

  void SendAddFilteredEventListenerIPC(ScriptContext* context,
                                       const std::string& event_name,
                                       const base::DictionaryValue& filter,
                                       bool is_lazy) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_AddFilteredListener(
        context->GetExtensionID(), event_name, absl::nullopt, filter, is_lazy));
  }

  void SendRemoveFilteredEventListenerIPC(ScriptContext* context,
                                          const std::string& event_name,
                                          const base::DictionaryValue& filter,
                                          bool remove_lazy_listener) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_RemoveFilteredListener(
        context->GetExtensionID(), event_name, absl::nullopt, filter,
        remove_lazy_listener));
  }

  void SendOpenMessageChannel(ScriptContext* script_context,
                              const PortId& port_id,
                              const MessageTarget& target,
                              const std::string& channel_name) override {
    content::RenderFrame* render_frame = script_context->GetRenderFrame();
    DCHECK(render_frame);
    PortContext frame_context =
        PortContext::ForFrame(render_frame->GetRoutingID());
    const Extension* extension = script_context->extension();

    switch (target.type) {
      case MessageTarget::EXTENSION: {
        ExtensionMsg_ExternalConnectionInfo info;
        if (extension && !extension->is_hosted_app()) {
          info.source_endpoint =
              script_context->context_type() == Feature::CONTENT_SCRIPT_CONTEXT
                  ? MessagingEndpoint::ForContentScript(extension->id())
                  : MessagingEndpoint::ForExtension(extension->id());
        } else {
          info.source_endpoint = MessagingEndpoint::ForWebPage();
        }
        info.target_id = *target.extension_id;
        info.source_url = script_context->url();

        render_thread_->Send(new ExtensionHostMsg_OpenChannelToExtension(
            frame_context, info, channel_name, port_id));
        break;
      }
      case MessageTarget::TAB: {
        DCHECK(extension);
        DCHECK_NE(script_context->context_type(),
                  Feature::CONTENT_SCRIPT_CONTEXT);
        ExtensionMsg_TabTargetConnectionInfo info;
        info.tab_id = *target.tab_id;
        info.frame_id = *target.frame_id;
        render_frame->Send(new ExtensionHostMsg_OpenChannelToTab(
            frame_context, info, extension->id(), channel_name, port_id));
        break;
      }
      case MessageTarget::NATIVE_APP:
        render_frame->Send(new ExtensionHostMsg_OpenChannelToNativeApp(
            frame_context, *target.native_application_name, port_id));
        break;
    }
  }

  void SendOpenMessagePort(int routing_id, const PortId& port_id) override {
    render_thread_->Send(new ExtensionHostMsg_OpenMessagePort(
        PortContext::ForFrame(routing_id), port_id));
  }

  void SendCloseMessagePort(int routing_id,
                            const PortId& port_id,
                            bool close_channel) override {
    render_thread_->Send(new ExtensionHostMsg_CloseMessagePort(
        PortContext::ForFrame(routing_id), port_id, close_channel));
  }

  void SendPostMessageToPort(const PortId& port_id,
                             const Message& message) override {
    render_thread_->Send(new ExtensionHostMsg_PostMessage(port_id, message));
  }

 private:
  void OnResponse(int request_id,
                  bool success,
                  base::Value response,
                  const std::string& error) {
    ExtensionsRendererClient::Get()
        ->GetDispatcher()
        ->bindings_system()
        ->HandleResponse(request_id, success,
                         base::Value::AsListValue(response), error);
  }

  mojom::EventRouter* GetEventRouter() {
    if (!event_router_remote_.is_bound()) {
      render_thread_->GetChannel()->GetRemoteAssociatedInterface(
          &event_router_remote_);
    }
    return event_router_remote_.get();
  }

  content::RenderThread* const render_thread_;
  mojo::AssociatedRemote<mojom::EventRouter> event_router_remote_;

  base::WeakPtrFactory<MainThreadIPCMessageSender> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MainThreadIPCMessageSender);
};

class WorkerThreadIPCMessageSender : public IPCMessageSender {
 public:
  WorkerThreadIPCMessageSender(WorkerThreadDispatcher* dispatcher,
                               int64_t service_worker_version_id)
      : dispatcher_(dispatcher),
        service_worker_version_id_(service_worker_version_id) {}
  ~WorkerThreadIPCMessageSender() override {}

  void SendRequestIPC(ScriptContext* context,
                      mojom::RequestParamsPtr params) override {
    DCHECK(!context->GetRenderFrame());
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());

    int worker_thread_id = content::WorkerThread::GetCurrentId();
    params->worker_thread_id = worker_thread_id;
    params->service_worker_version_id = service_worker_version_id_;

    std::string guid = base::GenerateGUID();
    request_id_to_guid_[params->request_id] = guid;

    // Keeps the worker alive during extension function call. Balanced in
    // HandleWorkerResponse().
    dispatcher_->Send(new ExtensionHostMsg_IncrementServiceWorkerActivity(
        service_worker_version_id_, guid));
    dispatcher_->Send(new ExtensionHostMsg_RequestWorker(*params));
  }

  void SendOnRequestResponseReceivedIPC(int request_id) override {
    auto iter = request_id_to_guid_.find(request_id);
    DCHECK(iter != request_id_to_guid_.end());
    dispatcher_->Send(new ExtensionHostMsg_DecrementServiceWorkerActivity(
        service_worker_version_id_, iter->second));
    request_id_to_guid_.erase(iter);
  }

  void SendAddUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              context->service_worker_version_id());

    dispatcher_->SendAddEventListener(
        context->GetExtensionID(), context->service_worker_scope(), event_name,
        context->service_worker_version_id(),
        content::WorkerThread::GetCurrentId());
  }

  void SendRemoveUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              context->service_worker_version_id());

    dispatcher_->SendRemoveEventListener(
        context->GetExtensionID(), context->service_worker_scope(), event_name,
        context->service_worker_version_id(),
        content::WorkerThread::GetCurrentId());
  }

  void SendAddUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());

    dispatcher_->Send(new ExtensionHostMsg_AddLazyServiceWorkerListener(
        context->GetExtensionID(), event_name,
        context->service_worker_scope()));
  }

  void SendRemoveUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());

    dispatcher_->Send(new ExtensionHostMsg_RemoveLazyServiceWorkerListener(
        context->GetExtensionID(), event_name,
        context->service_worker_scope()));
  }

  void SendAddFilteredEventListenerIPC(ScriptContext* context,
                                       const std::string& event_name,
                                       const base::DictionaryValue& filter,
                                       bool is_lazy) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              context->service_worker_version_id());
    ServiceWorkerIdentifier sw_identifier;
    sw_identifier.scope = context->service_worker_scope();
    sw_identifier.thread_id = content::WorkerThread::GetCurrentId();
    sw_identifier.version_id = context->service_worker_version_id();
    dispatcher_->Send(new ExtensionHostMsg_AddFilteredListener(
        context->GetExtensionID(), event_name, sw_identifier, filter, is_lazy));
  }

  void SendRemoveFilteredEventListenerIPC(ScriptContext* context,
                                          const std::string& event_name,
                                          const base::DictionaryValue& filter,
                                          bool remove_lazy_listener) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              context->service_worker_version_id());
    ServiceWorkerIdentifier sw_identifier;
    sw_identifier.scope = context->service_worker_scope();
    sw_identifier.thread_id = content::WorkerThread::GetCurrentId();
    sw_identifier.version_id = context->service_worker_version_id();
    dispatcher_->Send(new ExtensionHostMsg_RemoveFilteredListener(
        context->GetExtensionID(), event_name, sw_identifier, filter,
        remove_lazy_listener));
  }

  void SendOpenMessageChannel(ScriptContext* script_context,
                              const PortId& port_id,
                              const MessageTarget& target,
                              const std::string& channel_name) override {
    DCHECK(!script_context->GetRenderFrame());
    DCHECK(script_context->IsForServiceWorker());
    const Extension* extension = script_context->extension();

    switch (target.type) {
      case MessageTarget::EXTENSION: {
        ExtensionMsg_ExternalConnectionInfo info;
        if (extension && !extension->is_hosted_app()) {
          info.source_endpoint =
              MessagingEndpoint::ForExtension(extension->id());
        }
        info.target_id = *target.extension_id;
        info.source_url = script_context->url();
        dispatcher_->Send(new ExtensionHostMsg_OpenChannelToExtension(
            PortContextForCurrentWorker(), info, channel_name, port_id));
        break;
      }
      case MessageTarget::TAB: {
        DCHECK(extension);
        ExtensionMsg_TabTargetConnectionInfo info;
        info.tab_id = *target.tab_id;
        info.frame_id = *target.frame_id;
        dispatcher_->Send(new ExtensionHostMsg_OpenChannelToTab(
            PortContextForCurrentWorker(), info, extension->id(), channel_name,
            port_id));
        break;
      }
      case MessageTarget::NATIVE_APP:
        dispatcher_->Send(new ExtensionHostMsg_OpenChannelToNativeApp(
            PortContextForCurrentWorker(), *target.native_application_name,
            port_id));
        break;
    }
  }

  void SendOpenMessagePort(int routing_id, const PortId& port_id) override {
    DCHECK_EQ(MSG_ROUTING_NONE, routing_id);
    dispatcher_->Send(new ExtensionHostMsg_OpenMessagePort(
        PortContextForCurrentWorker(), port_id));
  }

  void SendCloseMessagePort(int routing_id,
                            const PortId& port_id,
                            bool close_channel) override {
    DCHECK_EQ(MSG_ROUTING_NONE, routing_id);
    dispatcher_->Send(new ExtensionHostMsg_CloseMessagePort(
        PortContextForCurrentWorker(), port_id, close_channel));
  }

  void SendPostMessageToPort(const PortId& port_id,
                             const Message& message) override {
    dispatcher_->Send(new ExtensionHostMsg_PostMessage(port_id, message));
  }

 private:
  const ExtensionId& GetExtensionId() {
    if (!extension_id_)
      extension_id_ = dispatcher_->GetScriptContext()->extension()->id();
    return *extension_id_;
  }

  PortContext PortContextForCurrentWorker() {
    return PortContext::ForWorker(content::WorkerThread::GetCurrentId(),
                                  service_worker_version_id_, GetExtensionId());
  }

  WorkerThreadDispatcher* const dispatcher_;
  const int64_t service_worker_version_id_;
  absl::optional<ExtensionId> extension_id_;

  // request id -> GUID map for each outstanding requests.
  std::map<int, std::string> request_id_to_guid_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThreadIPCMessageSender);
};

}  // namespace

IPCMessageSender::IPCMessageSender() {}
IPCMessageSender::~IPCMessageSender() = default;

// static
std::unique_ptr<IPCMessageSender>
IPCMessageSender::CreateMainThreadIPCMessageSender() {
  return std::make_unique<MainThreadIPCMessageSender>();
}

// static
std::unique_ptr<IPCMessageSender>
IPCMessageSender::CreateWorkerThreadIPCMessageSender(
    WorkerThreadDispatcher* dispatcher,
    int64_t service_worker_version_id) {
  return std::make_unique<WorkerThreadIPCMessageSender>(
      dispatcher, service_worker_version_id);
}

}  // namespace extensions
