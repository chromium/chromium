// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/ipc_message_sender.h"

#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/port_context.h"
#include "extensions/common/constants.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/automation_registry.mojom.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_router.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/common/mojom/renderer_host.mojom.h"
#include "extensions/common/trace_util.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/service_worker_data.h"
#include "extensions/renderer/trace_util.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"

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

  void SendResponseAckIPC(ScriptContext* context,
                          const base::Uuid& request_uuid) override {
    CHECK(!context->IsForServiceWorker());
    CHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    content::RenderFrame* frame = context->GetRenderFrame();
    CHECK(frame);

    ExtensionFrameHelper::Get(frame)->GetLocalFrameHost()->ResponseAck(
        request_uuid);
  }

  mojom::EventListenerOwnerPtr GetEventListenerOwner(ScriptContext* context) {
    return !context->GetExtensionID().empty()
               ? mojom::EventListenerOwner::NewExtensionId(
                     context->GetExtensionID())
               : mojom::EventListenerOwner::NewListenerUrl(context->url());
  }

  void SendAddUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    GetEventRouter(context)->AddListenerForMainThread(mojom::EventListener::New(
        GetEventListenerOwner(context), event_name, nullptr, std::nullopt));
  }

  void SendRemoveUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    GetEventRouter(context)->RemoveListenerForMainThread(
        mojom::EventListener::New(GetEventListenerOwner(context), event_name,
                                  nullptr, std::nullopt));
  }

  void SendAddUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    GetEventRouter(context)->AddLazyListenerForMainThread(
        context->GetExtensionID(), event_name);
  }

  void SendRemoveUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    GetEventRouter(context)->RemoveLazyListenerForMainThread(
        context->GetExtensionID(), event_name);
  }

  void SendAddFilteredEventListenerIPC(ScriptContext* context,
                                       const std::string& event_name,
                                       const base::Value::Dict& filter,
                                       bool is_lazy) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    GetEventRouter(context)->AddFilteredListenerForMainThread(
        GetEventListenerOwner(context), event_name, filter.Clone(), is_lazy);
  }

  void SendRemoveFilteredEventListenerIPC(ScriptContext* context,
                                          const std::string& event_name,
                                          const base::Value::Dict& filter,
                                          bool remove_lazy_listener) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    GetEventRouter(context)->RemoveFilteredListenerForMainThread(
        GetEventListenerOwner(context), event_name, filter.Clone(),
        remove_lazy_listener);
  }

  void SendBindAutomationIPC(
      ScriptContext* context,
      mojo::PendingAssociatedRemote<ax::mojom::Automation> pending_remote)
      override {
    CHECK(!context->IsForServiceWorker());

    GetRendererAutomationRegistry(context)->BindAutomation(
        std::move(pending_remote));
  }

  void SendOpenMessageChannel(
      ScriptContext* script_context,
      const PortId& port_id,
      const MessageTarget& target,
      mojom::ChannelType channel_type,
      const std::string& channel_name,
      mojo::PendingAssociatedRemote<mojom::MessagePort> port,
      mojo::PendingAssociatedReceiver<mojom::MessagePortHost> port_host)
      override {
    content::RenderFrame* render_frame = script_context->GetRenderFrame();
    DCHECK(render_frame);
    const Extension* extension = script_context->extension();

    // TODO(crbug.com/40263335): We should just avoid passing a
    // channel name in at all for non-connect messages; we no longer need to.
    std::string channel_name_to_use =
        channel_type == mojom::ChannelType::kConnect ? channel_name
                                                     : std::string();
    switch (target.type) {
      case MessageTarget::EXTENSION: {
        auto info = mojom::ExternalConnectionInfo::New();
        if (extension && !extension->is_hosted_app()) {
          switch (script_context->context_type()) {
            case mojom::ContextType::kPrivilegedExtension:
            case mojom::ContextType::kUnprivilegedExtension:
            case mojom::ContextType::kLockscreenExtension:
            case mojom::ContextType::kOffscreenExtension:
              info->source_endpoint =
                  MessagingEndpoint::ForExtension(extension->id());
              break;
            case mojom::ContextType::kContentScript:
              info->source_endpoint =
                  MessagingEndpoint::ForContentScript(extension->id());
              break;
            case mojom::ContextType::kUserScript:
              info->source_endpoint =
                  MessagingEndpoint::ForUserScript(extension->id());
              break;
            case mojom::ContextType::kUnspecified:
            case mojom::ContextType::kWebPage:
            case mojom::ContextType::kPrivilegedWebPage:
            case mojom::ContextType::kWebUi:
            case mojom::ContextType::kUntrustedWebUi:
              NOTREACHED() << "Unexpected Context Encountered: "
                           << script_context->GetDebugString();
          }
        } else {
          info->source_endpoint = MessagingEndpoint::ForWebPage();
        }
        info->target_id = *target.extension_id;
        info->source_url = script_context->url();

        TRACE_RENDERER_EXTENSION_EVENT(
            "MainThreadIPCMessageSender::SendOpenMessageChannel/extension",
            *target.extension_id);
        ExtensionFrameHelper::Get(render_frame)
            ->GetLocalFrameHost()
            ->OpenChannelToExtension(std::move(info), channel_type,
                                     channel_name_to_use, port_id,
                                     std::move(port), std::move(port_host));
        break;
      }
      case MessageTarget::TAB: {
        DCHECK(extension);
        DCHECK_NE(script_context->context_type(),
                  mojom::ContextType::kContentScript);
        ExtensionFrameHelper::Get(render_frame)
            ->GetLocalFrameHost()
            ->OpenChannelToTab(*target.tab_id, *target.frame_id,
                               target.document_id, channel_type,
                               channel_name_to_use, port_id, std::move(port),
                               std::move(port_host));
        break;
      }
      case MessageTarget::NATIVE_APP:
        CHECK_EQ(mojom::ChannelType::kNative, channel_type);
        ExtensionFrameHelper::Get(render_frame)
            ->GetLocalFrameHost()
            ->OpenChannelToNativeApp(*target.native_application_name, port_id,
                                     std::move(port), std::move(port_host));
        break;
    }
  }

  void SendActivityLogIPC(ScriptContext* context,
                          const ExtensionId& extension_id,
                          ActivityLogCallType call_type,
                          const std::string& call_name,
                          base::Value::List args,
                          const std::string& extra) override {
    switch (call_type) {
      case ActivityLogCallType::APICALL:
        GetRendererHost(context)->AddAPIActionToActivityLog(
            extension_id, call_name, std::move(args), extra);
        break;
      case ActivityLogCallType::EVENT:
        GetRendererHost(context)->AddEventToActivityLog(extension_id, call_name,
                                                        std::move(args), extra);
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
        ->dispatcher()
        ->bindings_system()
        ->HandleResponse(request_id, success, std::move(response), error,
                         std::move(response_data));
  }

  mojom::EventRouter* GetEventRouter(ScriptContext* context) {
    content::RenderFrame* frame = context->GetRenderFrame();
    CHECK(frame);
    return ExtensionFrameHelper::Get(frame)->GetEventRouter();
  }

  mojom::RendererAutomationRegistry* GetRendererAutomationRegistry(
      ScriptContext* context) {
    content::RenderFrame* frame = context->GetRenderFrame();
    CHECK(frame);
    return ExtensionFrameHelper::Get(frame)->GetRendererAutomationRegistry();
  }

  mojom::RendererHost* GetRendererHost(ScriptContext* context) {
    content::RenderFrame* frame = context->GetRenderFrame();
    CHECK(frame);
    return ExtensionFrameHelper::Get(frame)->GetRendererHost();
  }

  const raw_ptr<content::RenderThread, DanglingUntriaged> render_thread_;
  mojo::AssociatedRemote<mojom::RendererHost> renderer_host_;

  base::WeakPtrFactory<MainThreadIPCMessageSender> weak_ptr_factory_{this};
};

class WorkerThreadIPCMessageSender : public IPCMessageSender {
 public:
  WorkerThreadIPCMessageSender(
      WorkerThreadDispatcher* dispatcher,
      blink::WebServiceWorkerContextProxy* context_proxy,
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

    const int request_id = params->request_id;
    WorkerThreadDispatcher::GetServiceWorkerData()
        ->GetServiceWorkerHost()
        ->RequestWorker(std::move(params),
                        base::BindOnce(
                            [](int request_id, bool success,
                               base::Value::List args, const std::string& error,
                               mojom::ExtraResponseDataPtr extra_data) {
                              WorkerThreadDispatcher::GetServiceWorkerData()
                                  ->bindings_system()
                                  ->HandleResponse(request_id, success,
                                                   std::move(args), error,
                                                   std::move(extra_data));
                            },
                            request_id));
  }

  void SendResponseAckIPC(ScriptContext* context,
                          const base::Uuid& request_uuid) override {
    CHECK(!context->GetRenderFrame());
    CHECK(context->IsForServiceWorker());
    CHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());

    WorkerThreadDispatcher::GetServiceWorkerData()
        ->GetServiceWorkerHost()
        ->WorkerResponseAck(request_uuid);
  }

  void SendAddUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              context->service_worker_version_id());

    auto event_listener = mojom::EventListener::New(
        mojom::EventListenerOwner::NewExtensionId(context->GetExtensionID()),
        event_name,
        mojom::ServiceWorkerContext::New(context->service_worker_scope(),
                                         context->service_worker_version_id(),
                                         content::WorkerThread::GetCurrentId()),
        /*event_filter=*/std::nullopt);
    WorkerThreadDispatcher::GetServiceWorkerData()
        ->GetEventRouter()
        ->AddListenerForServiceWorker(std::move(event_listener));
  }

  void SendRemoveUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              context->service_worker_version_id());

    auto event_listener = mojom::EventListener::New(
        mojom::EventListenerOwner::NewExtensionId(context->GetExtensionID()),
        event_name,
        mojom::ServiceWorkerContext::New(context->service_worker_scope(),
                                         context->service_worker_version_id(),
                                         content::WorkerThread::GetCurrentId()),
        /*event_filter=*/std::nullopt);

    WorkerThreadDispatcher::GetServiceWorkerData()
        ->GetEventRouter()
        ->RemoveListenerForServiceWorker(std::move(event_listener));
  }

  void SendAddUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());

    WorkerThreadDispatcher::GetServiceWorkerData()
        ->GetEventRouter()
        ->AddLazyListenerForServiceWorker(context->GetExtensionID(),
                                          context->service_worker_scope(),
                                          event_name);
  }

  void SendRemoveUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());

    WorkerThreadDispatcher::GetServiceWorkerData()
        ->GetEventRouter()
        ->RemoveLazyListenerForServiceWorker(context->GetExtensionID(),
                                             context->service_worker_scope(),
                                             event_name);
  }

  void SendAddFilteredEventListenerIPC(ScriptContext* context,
                                       const std::string& event_name,
                                       const base::Value::Dict& filter,
                                       bool is_lazy) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              context->service_worker_version_id());

    WorkerThreadDispatcher::GetServiceWorkerData()
        ->GetEventRouter()
        ->AddFilteredListenerForServiceWorker(
            context->GetExtensionID(), event_name,
            mojom::ServiceWorkerContext::New(
                context->service_worker_scope(),
                context->service_worker_version_id(),
                content::WorkerThread::GetCurrentId()),
            filter.Clone(), is_lazy);
  }

  void SendRemoveFilteredEventListenerIPC(ScriptContext* context,
                                          const std::string& event_name,
                                          const base::Value::Dict& filter,
                                          bool remove_lazy_listener) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              context->service_worker_version_id());

    WorkerThreadDispatcher::GetServiceWorkerData()
        ->GetEventRouter()
        ->RemoveFilteredListenerForServiceWorker(
            context->GetExtensionID(), event_name,
            mojom::ServiceWorkerContext::New(
                context->service_worker_scope(),
                context->service_worker_version_id(),
                content::WorkerThread::GetCurrentId()),
            filter.Clone(), remove_lazy_listener);
  }

  void SendBindAutomationIPC(
      ScriptContext* context,
      mojo::PendingAssociatedRemote<ax::mojom::Automation> pending_remote)
      override {
    CHECK(context->IsForServiceWorker());
    WorkerThreadDispatcher::GetServiceWorkerData()
        ->GetAutomationRegistry()
        ->BindAutomation(std::move(pending_remote));
  }

  void SendOpenMessageChannel(
      ScriptContext* script_context,
      const PortId& port_id,
      const MessageTarget& target,
      mojom::ChannelType channel_type,
      const std::string& channel_name,
      mojo::PendingAssociatedRemote<mojom::MessagePort> port,
      mojo::PendingAssociatedReceiver<mojom::MessagePortHost> port_host)
      override {
    DCHECK(!script_context->GetRenderFrame());
    DCHECK(script_context->IsForServiceWorker());
    const Extension* extension = script_context->extension();

    // TODO(crbug.com/40263335): We should just avoid passing a
    // channel name in at all for non-connect messages; we no longer need to.
    std::string channel_name_to_use =
        channel_type == mojom::ChannelType::kConnect ? channel_name
                                                     : std::string();

    switch (target.type) {
      case MessageTarget::EXTENSION: {
        auto info = mojom::ExternalConnectionInfo::New();
        if (extension && !extension->is_hosted_app()) {
          info->source_endpoint =
              MessagingEndpoint::ForExtension(extension->id());
        }
        info->target_id = *target.extension_id;
        info->source_url = script_context->url();
        TRACE_RENDERER_EXTENSION_EVENT(
            "WorkerThreadIPCMessageSender::SendOpenMessageChannel/extension",
            *target.extension_id);
        WorkerThreadDispatcher::GetServiceWorkerData()
            ->GetServiceWorkerHost()
            ->OpenChannelToExtension(std::move(info), channel_type,
                                     channel_name_to_use, port_id,
                                     std::move(port), std::move(port_host));
        break;
      }
      case MessageTarget::TAB: {
        DCHECK(extension);
        WorkerThreadDispatcher::GetServiceWorkerData()
            ->GetServiceWorkerHost()
            ->OpenChannelToTab(*target.tab_id, *target.frame_id,
                               target.document_id, channel_type,
                               channel_name_to_use, port_id, std::move(port),
                               std::move(port_host));
        break;
      }
      case MessageTarget::NATIVE_APP:
        CHECK_EQ(mojom::ChannelType::kNative, channel_type);
        WorkerThreadDispatcher::GetServiceWorkerData()
            ->GetServiceWorkerHost()
            ->OpenChannelToNativeApp(*target.native_application_name, port_id,
                                     std::move(port), std::move(port_host));
        break;
    }
  }

  void SendActivityLogIPC(ScriptContext* context,
                          const ExtensionId& extension_id,
                          ActivityLogCallType call_type,
                          const std::string& call_name,
                          base::Value::List args,
                          const std::string& extra) override {
    switch (call_type) {
      case ActivityLogCallType::APICALL:
        GetRendererHost()->AddAPIActionToActivityLog(extension_id, call_name,
                                                     std::move(args), extra);
        break;
      case ActivityLogCallType::EVENT:
        GetRendererHost()->AddEventToActivityLog(extension_id, call_name,
                                                 std::move(args), extra);
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

  mojom::RendererHost* GetRendererHost() {
    return WorkerThreadDispatcher::GetServiceWorkerData()->GetRendererHost();
  }

  const raw_ptr<WorkerThreadDispatcher> dispatcher_;
  const int64_t service_worker_version_id_;
  std::optional<ExtensionId> extension_id_;
};

}  // namespace

IPCMessageSender::IPCMessageSender() = default;

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
    blink::WebServiceWorkerContextProxy* context_proxy,
    int64_t service_worker_version_id) {
  return std::make_unique<WorkerThreadIPCMessageSender>(
      dispatcher, context_proxy, service_worker_version_id);
}

}  // namespace extensions
