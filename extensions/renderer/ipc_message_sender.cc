// Copyright 2017 The Chromium Authors
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
#include "extensions/common/trace_util.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/trace_util.h"
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

  MainThreadIPCMessageSender(const MainThreadIPCMessageSender&) = delete;
  MainThreadIPCMessageSender& operator=(const MainThreadIPCMessageSender&) =
      delete;

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

  mojom::EventListenerParamPtr GetEventListenerParam(ScriptContext* context) {
    return !context->GetExtensionID().empty()
               ? mojom::EventListenerParam::NewExtensionId(
                     context->GetExtensionID())
               : mojom::EventListenerParam::NewListenerUrl(context->url());
  }

  void SendAddUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    GetEventRouter()->AddListenerForMainThread(GetEventListenerParam(context),
                                               event_name);
  }

  void SendRemoveUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    GetEventRouter()->RemoveListenerForMainThread(
        GetEventListenerParam(context), event_name);
  }

  void SendAddUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    GetEventRouter()->AddLazyListenerForMainThread(context->GetExtensionID(),
                                                   event_name);
  }

  void SendRemoveUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    GetEventRouter()->RemoveLazyListenerForMainThread(context->GetExtensionID(),
                                                      event_name);
  }

  void SendAddFilteredEventListenerIPC(ScriptContext* context,
                                       const std::string& event_name,
                                       const base::Value::Dict& filter,
                                       bool is_lazy) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    GetEventRouter()->AddFilteredListenerForMainThread(
        GetEventListenerParam(context), event_name, filter.Clone(), is_lazy);
  }

  void SendRemoveFilteredEventListenerIPC(ScriptContext* context,
                                          const std::string& event_name,
                                          const base::Value::Dict& filter,
                                          bool remove_lazy_listener) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    GetEventRouter()->RemoveFilteredListenerForMainThread(
        GetEventListenerParam(context), event_name, filter.Clone(),
        remove_lazy_listener);
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

        TRACE_RENDERER_EXTENSION_EVENT(
            "MainThreadIPCMessageSender::SendOpenMessageChannel/extension",
            *target.extension_id);
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
        if (target.document_id)
          info.document_id = *target.document_id;
        render_frame->Send(new ExtensionHostMsg_OpenChannelToTab(
            frame_context, info, channel_name, port_id));
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

  void SendMessageResponsePending(int routing_id,
                                  const PortId& port_id) override {
    render_thread_->Send(new ExtensionHostMsg_ResponsePending(
        PortContext::ForFrame(routing_id), port_id));
  }

  void SendActivityLogIPC(
      const ExtensionId& extension_id,
      ActivityLogCallType call_type,
      const ExtensionHostMsg_APIActionOrEvent_Params& params) override {
    switch (call_type) {
      case ActivityLogCallType::APICALL:
        render_thread_->Send(new ExtensionHostMsg_AddAPIActionToActivityLog(
            extension_id, params));
        break;
      case ActivityLogCallType::EVENT:
        render_thread_->Send(
            new ExtensionHostMsg_AddEventToActivityLog(extension_id, params));
        break;
    }
  }

 private:
  void OnResponse(int request_id,
                  bool success,
                  base::Value::List response,
                  const std::string& error,
                  mojom::ExtraResponseDataPtr response_data) {
    ExtensionsRendererClient::Get()
        ->GetDispatcher()
        ->bindings_system()
        ->HandleResponse(request_id, success, std::move(response), error,
                         std::move(response_data));
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
};

class WorkerThreadIPCMessageSender : public IPCMessageSender {
 public:
  WorkerThreadIPCMessageSender(WorkerThreadDispatcher* dispatcher,
                               int64_t service_worker_version_id)
      : dispatcher_(dispatcher),
        service_worker_version_id_(service_worker_version_id) {}

  WorkerThreadIPCMessageSender(const WorkerThreadIPCMessageSender&) = delete;
  WorkerThreadIPCMessageSender& operator=(const WorkerThreadIPCMessageSender&) =
      delete;

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

    dispatcher_->SendAddEventLazyListener(
        context->GetExtensionID(), context->service_worker_scope(), event_name);
  }

  void SendRemoveUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());

    dispatcher_->SendRemoveEventLazyListener(
        context->GetExtensionID(), context->service_worker_scope(), event_name);
  }

  void SendAddFilteredEventListenerIPC(ScriptContext* context,
                                       const std::string& event_name,
                                       const base::Value::Dict& filter,
                                       bool is_lazy) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              context->service_worker_version_id());

    dispatcher_->SendAddEventFilteredListener(
        context->GetExtensionID(), context->service_worker_scope(), event_name,
        context->service_worker_version_id(),
        content::WorkerThread::GetCurrentId(), filter.Clone(), is_lazy);
  }

  void SendRemoveFilteredEventListenerIPC(ScriptContext* context,
                                          const std::string& event_name,
                                          const base::Value::Dict& filter,
                                          bool remove_lazy_listener) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              context->service_worker_version_id());

    dispatcher_->SendRemoveEventFilteredListener(
        context->GetExtensionID(), context->service_worker_scope(), event_name,
        context->service_worker_version_id(),
        content::WorkerThread::GetCurrentId(), filter.Clone(),
        remove_lazy_listener);
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
        TRACE_RENDERER_EXTENSION_EVENT(
            "WorkerThreadIPCMessageSender::SendOpenMessageChannel/extension",
            *target.extension_id);
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
            PortContextForCurrentWorker(), info, channel_name, port_id));
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

  void SendMessageResponsePending(int routing_id,
                                  const PortId& port_id) override {
    DCHECK_EQ(MSG_ROUTING_NONE, routing_id);
    dispatcher_->Send(new ExtensionHostMsg_ResponsePending(
        PortContextForCurrentWorker(), port_id));
  }

  void SendActivityLogIPC(
      const ExtensionId& extension_id,
      ActivityLogCallType call_type,
      const ExtensionHostMsg_APIActionOrEvent_Params& params) override {
    switch (call_type) {
      case ActivityLogCallType::APICALL:
        dispatcher_->Send(new ExtensionHostMsg_AddAPIActionToActivityLog(
            extension_id, params));
        break;
      case ActivityLogCallType::EVENT:
        dispatcher_->Send(
            new ExtensionHostMsg_AddEventToActivityLog(extension_id, params));
        break;
    }
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
