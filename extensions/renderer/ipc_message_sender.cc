// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/ipc_message_sender.h"

#include <map>

#include "base/guid.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/message_target.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace extensions {

namespace {

// These values are logged to UMA. Entries should not be renumbered.
enum class IncludeTlsChannelIdBehavior {
  // The TLS channel ID was not requested.
  kNotRequested = 0,

  // DEPRECATED: The TLS channel ID was requested, but was not included because
  // the target extension did not allow it.
  // kRequestedButDenied = 1,
  // DEPRECATED: The TLS channel ID was requested, but was not found.
  // kRequestedButNotFound = 2,
  // DEPRECATED: The TLS channel ID was requested, allowed, and included in the
  // response.
  // kRequestedAndIncluded = 3,

  // The TLS channel ID was requested, but was not provided because Channel ID
  // is no longer supported.
  kRequestedButNotSupported = 4,

  kMaxValue = kRequestedButNotSupported,
};

void RecordIncludeTlsChannelIdBehavior(bool include_tls_channel_id) {
  auto tls_channel_id_behavior =
      include_tls_channel_id
          ? IncludeTlsChannelIdBehavior::kRequestedButNotSupported
          : IncludeTlsChannelIdBehavior::kNotRequested;
  UMA_HISTOGRAM_ENUMERATION("Extensions.Messaging.IncludeChannelIdBehavior",
                            tls_channel_id_behavior);
}

class MainThreadIPCMessageSender : public IPCMessageSender {
 public:
  MainThreadIPCMessageSender() : render_thread_(content::RenderThread::Get()) {}
  ~MainThreadIPCMessageSender() override {}

  void SendRequestIPC(
      ScriptContext* context,
      std::unique_ptr<ExtensionHostMsg_Request_Params> params) override {
    content::RenderFrame* frame = context->GetRenderFrame();
    if (!frame)
      return;

    frame->Send(new ExtensionHostMsg_Request(frame->GetRoutingID(), *params));
  }

  void SendOnRequestResponseReceivedIPC(int request_id) override {}

  void SendAddUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_AddListener(
        context->GetExtensionID(), context->url(), event_name,
        blink::mojom::kInvalidServiceWorkerVersionId, kMainThreadId));
  }

  void SendRemoveUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_RemoveListener(
        context->GetExtensionID(), context->url(), event_name,
        blink::mojom::kInvalidServiceWorkerVersionId, kMainThreadId));
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
        context->GetExtensionID(), event_name, base::nullopt, filter, is_lazy));
  }

  void SendRemoveFilteredEventListenerIPC(ScriptContext* context,
                                          const std::string& event_name,
                                          const base::DictionaryValue& filter,
                                          bool remove_lazy_listener) override {
    DCHECK(!context->IsForServiceWorker());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_RemoveFilteredListener(
        context->GetExtensionID(), event_name, base::nullopt, filter,
        remove_lazy_listener));
  }

  void SendOpenMessageChannel(ScriptContext* script_context,
                              const PortId& port_id,
                              const MessageTarget& target,
                              const std::string& channel_name,
                              bool include_tls_channel_id) override {
    RecordIncludeTlsChannelIdBehavior(include_tls_channel_id);
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
  content::RenderThread* const render_thread_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadIPCMessageSender);
};

class WorkerThreadIPCMessageSender : public IPCMessageSender {
 public:
  WorkerThreadIPCMessageSender(WorkerThreadDispatcher* dispatcher,
                               int64_t service_worker_version_id)
      : dispatcher_(dispatcher),
        service_worker_version_id_(service_worker_version_id) {}
  ~WorkerThreadIPCMessageSender() override {}

  void SendRequestIPC(
      ScriptContext* context,
      std::unique_ptr<ExtensionHostMsg_Request_Params> params) override {
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

    dispatcher_->Send(new ExtensionHostMsg_AddListener(
        context->GetExtensionID(), context->service_worker_scope(), event_name,
        context->service_worker_version_id(),
        content::WorkerThread::GetCurrentId()));
  }

  void SendRemoveUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK(context->IsForServiceWorker());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              context->service_worker_version_id());

    dispatcher_->Send(new ExtensionHostMsg_RemoveListener(
        context->GetExtensionID(), context->service_worker_scope(), event_name,
        context->service_worker_version_id(),
        content::WorkerThread::GetCurrentId()));
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
                              const std::string& channel_name,
                              bool include_tls_channel_id) override {
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
  base::Optional<ExtensionId> extension_id_;

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
