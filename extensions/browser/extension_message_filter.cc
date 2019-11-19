// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_message_filter.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/crx_file/id_util.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/blob_holder.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/port_context.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ipc/ipc_message_macros.h"

using content::BrowserThread;
using content::RenderProcessHost;

namespace extensions {

namespace {

class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static ShutdownNotifierFactory* GetInstance() {
    return base::Singleton<ShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "ExtensionMessageFilter") {
    DependsOn(EventRouterFactory::GetInstance());
    DependsOn(ProcessManagerFactory::GetInstance());
  }
  ~ShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(ShutdownNotifierFactory);
};

}  // namespace

ExtensionMessageFilter::ExtensionMessageFilter(int render_process_id,
                                               content::BrowserContext* context)
    : BrowserMessageFilter(ExtensionMsgStart),
      render_process_id_(render_process_id),
      browser_context_(context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  shutdown_notifier_ =
      ShutdownNotifierFactory::GetInstance()->Get(context)->Subscribe(
          base::Bind(&ExtensionMessageFilter::ShutdownOnUIThread,
                     base::Unretained(this)));
}

void ExtensionMessageFilter::EnsureShutdownNotifierFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

ExtensionMessageFilter::~ExtensionMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

EventRouter* ExtensionMessageFilter::GetEventRouter() {
  DCHECK(browser_context_);
  return EventRouter::Get(browser_context_);
}

void ExtensionMessageFilter::ShutdownOnUIThread() {
  browser_context_ = nullptr;
  shutdown_notifier_.reset();
}

void ExtensionMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
    case ExtensionHostMsg_AddListener::ID:
    case ExtensionHostMsg_RemoveListener::ID:
    case ExtensionHostMsg_AddLazyListener::ID:
    case ExtensionHostMsg_RemoveLazyListener::ID:
    case ExtensionHostMsg_AddLazyServiceWorkerListener::ID:
    case ExtensionHostMsg_RemoveLazyServiceWorkerListener::ID:
    case ExtensionHostMsg_AddFilteredListener::ID:
    case ExtensionHostMsg_RemoveFilteredListener::ID:
    case ExtensionHostMsg_ShouldSuspendAck::ID:
    case ExtensionHostMsg_SuspendAck::ID:
    case ExtensionHostMsg_TransferBlobsAck::ID:
    case ExtensionHostMsg_WakeEventPage::ID:
    case ExtensionHostMsg_OpenChannelToExtension::ID:
    case ExtensionHostMsg_OpenChannelToTab::ID:
    case ExtensionHostMsg_OpenChannelToNativeApp::ID:
    case ExtensionHostMsg_OpenMessagePort::ID:
    case ExtensionHostMsg_CloseMessagePort::ID:
    case ExtensionHostMsg_PostMessage::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

void ExtensionMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

bool ExtensionMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionMessageFilter, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddListener,
                        OnExtensionAddListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveListener,
                        OnExtensionRemoveListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddLazyListener,
                        OnExtensionAddLazyListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveLazyListener,
                        OnExtensionRemoveLazyListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddLazyServiceWorkerListener,
                        OnExtensionAddLazyServiceWorkerListener);
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveLazyServiceWorkerListener,
                        OnExtensionRemoveLazyServiceWorkerListener);
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddFilteredListener,
                        OnExtensionAddFilteredListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveFilteredListener,
                        OnExtensionRemoveFilteredListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ShouldSuspendAck,
                        OnExtensionShouldSuspendAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_SuspendAck,
                        OnExtensionSuspendAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_TransferBlobsAck,
                        OnExtensionTransferBlobsAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_WakeEventPage,
                        OnExtensionWakeEventPage)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToExtension,
                        OnOpenChannelToExtension)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToTab, OnOpenChannelToTab)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToNativeApp,
                        OnOpenChannelToNativeApp)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenMessagePort, OnOpenMessagePort)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_CloseMessagePort, OnCloseMessagePort)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_PostMessage, OnPostMessage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionMessageFilter::OnExtensionAddListener(
    const std::string& extension_id,
    const GURL& listener_or_worker_scope_url,
    const std::string& event_name,
    int64_t service_worker_version_id,
    int worker_thread_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  EventRouter* event_router = GetEventRouter();
  if (crx_file::id_util::IdIsValid(extension_id)) {
    const bool is_service_worker_context = worker_thread_id != kMainThreadId;
    if (is_service_worker_context) {
      DCHECK(listener_or_worker_scope_url.is_valid());
      event_router->AddServiceWorkerEventListener(
          event_name, process, extension_id, listener_or_worker_scope_url,
          service_worker_version_id, worker_thread_id);
    } else {
      event_router->AddEventListener(event_name, process, extension_id);
    }
  } else if (listener_or_worker_scope_url.is_valid()) {
    event_router->AddEventListenerForURL(event_name, process,
                                         listener_or_worker_scope_url);
  } else {
    NOTREACHED() << "Tried to add an event listener without a valid "
                 << "extension ID nor listener URL";
  }
}

void ExtensionMessageFilter::OnExtensionRemoveListener(
    const std::string& extension_id,
    const GURL& listener_or_worker_scope_url,
    const std::string& event_name,
    int64_t service_worker_version_id,
    int worker_thread_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  if (crx_file::id_util::IdIsValid(extension_id)) {
    const bool is_service_worker_context = worker_thread_id != kMainThreadId;
    if (is_service_worker_context) {
      DCHECK(listener_or_worker_scope_url.is_valid());
      GetEventRouter()->RemoveServiceWorkerEventListener(
          event_name, process, extension_id, listener_or_worker_scope_url,
          service_worker_version_id, worker_thread_id);
    } else {
      GetEventRouter()->RemoveEventListener(event_name, process, extension_id);
    }
  } else if (listener_or_worker_scope_url.is_valid()) {
    GetEventRouter()->RemoveEventListenerForURL(event_name, process,
                                                listener_or_worker_scope_url);
  } else {
    NOTREACHED() << "Tried to remove an event listener without a valid "
                 << "extension ID nor listener URL";
  }
}

void ExtensionMessageFilter::OnExtensionAddLazyListener(
    const std::string& extension_id,
    const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;
  GetEventRouter()->AddLazyEventListener(event_name, extension_id);
}

void ExtensionMessageFilter::OnExtensionAddLazyServiceWorkerListener(
    const std::string& extension_id,
    const std::string& event_name,
    const GURL& service_worker_scope) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  GetEventRouter()->AddLazyServiceWorkerEventListener(event_name, extension_id,
                                                      service_worker_scope);
}

void ExtensionMessageFilter::OnExtensionRemoveLazyListener(
    const std::string& extension_id,
    const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  GetEventRouter()->RemoveLazyEventListener(event_name, extension_id);
}

void ExtensionMessageFilter::OnExtensionRemoveLazyServiceWorkerListener(
    const std::string& extension_id,
    const std::string& event_name,
    const GURL& worker_scope_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  GetEventRouter()->RemoveLazyServiceWorkerEventListener(
      event_name, extension_id, worker_scope_url);
}

void ExtensionMessageFilter::OnExtensionAddFilteredListener(
    const std::string& extension_id,
    const std::string& event_name,
    base::Optional<ServiceWorkerIdentifier> sw_identifier,
    const base::DictionaryValue& filter,
    bool lazy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  GetEventRouter()->AddFilteredEventListener(event_name, process, extension_id,
                                             sw_identifier, filter, lazy);
}

void ExtensionMessageFilter::OnExtensionRemoveFilteredListener(
    const std::string& extension_id,
    const std::string& event_name,
    base::Optional<ServiceWorkerIdentifier> sw_identifier,
    const base::DictionaryValue& filter,
    bool lazy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  GetEventRouter()->RemoveFilteredEventListener(
      event_name, process, extension_id, sw_identifier, filter, lazy);
}

void ExtensionMessageFilter::OnExtensionShouldSuspendAck(
     const std::string& extension_id, int sequence_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  ProcessManager::Get(browser_context_)
      ->OnShouldSuspendAck(extension_id, sequence_id);
}

void ExtensionMessageFilter::OnExtensionSuspendAck(
     const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  ProcessManager::Get(browser_context_)->OnSuspendAck(extension_id);
}

void ExtensionMessageFilter::OnExtensionTransferBlobsAck(
    const std::vector<std::string>& blob_uuids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  BlobHolder::FromRenderProcessHost(process)->DropBlobs(blob_uuids);
}

void ExtensionMessageFilter::OnExtensionWakeEventPage(
    int request_id,
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension) {
    // Don't kill the renderer, it might just be some context which hasn't
    // caught up to extension having been uninstalled.
    return;
  }

  ProcessManager* process_manager = ProcessManager::Get(browser_context_);

  if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    // Wake the event page if it's asleep, or immediately repond with success
    // if it's already awake.
    if (process_manager->IsEventPageSuspended(extension_id)) {
      process_manager->WakeEventPage(
          extension_id,
          base::BindOnce(&ExtensionMessageFilter::SendWakeEventPageResponse,
                         this, request_id));
    } else {
      SendWakeEventPageResponse(request_id, true);
    }
    return;
  }

  if (BackgroundInfo::HasPersistentBackgroundPage(extension)) {
    // No point in trying to wake a persistent background page. If it's open,
    // immediately return and call it a success. If it's closed, fail.
    SendWakeEventPageResponse(request_id,
                              process_manager->GetBackgroundHostForExtension(
                                  extension_id) != nullptr);
    return;
  }

  // The extension has no background page, so there is nothing to wake.
  SendWakeEventPageResponse(request_id, false);
}

void ExtensionMessageFilter::OnOpenChannelToExtension(
    const PortContext& source_context,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& channel_name,
    const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (info.source_endpoint.type == MessagingEndpoint::Type::kNativeApp) {
    // Requests for channels initiated by native applications don't originate
    // from renderer processes.
    bad_message::ReceivedBadMessage(
        this, bad_message::EMF_INVALID_CHANNEL_SOURCE_TYPE);
    return;
  }
  if (browser_context_) {
    ChannelEndpoint source_endpoint(browser_context_, render_process_id_,
                                    source_context);
    MessageService::Get(browser_context_)
        ->OpenChannelToExtension(source_endpoint, port_id, info.source_endpoint,
                                 nullptr /* opener_port */, info.target_id,
                                 info.source_url, channel_name);
  }
}

void ExtensionMessageFilter::OnOpenChannelToNativeApp(
    const PortContext& source_context,
    const std::string& native_app_name,
    const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  ChannelEndpoint source_endpoint(browser_context_, render_process_id_,
                                  source_context);
  MessageService::Get(browser_context_)
      ->OpenChannelToNativeApp(source_endpoint, port_id, native_app_name);
}

void ExtensionMessageFilter::OnOpenChannelToTab(
    const PortContext& source_context,
    const ExtensionMsg_TabTargetConnectionInfo& info,
    const std::string& extension_id,
    const std::string& channel_name,
    const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  ChannelEndpoint source_endpoint(browser_context_, render_process_id_,
                                  source_context);
  MessageService::Get(browser_context_)
      ->OpenChannelToTab(source_endpoint, port_id, info.tab_id, info.frame_id,
                         extension_id, channel_name);
}

void ExtensionMessageFilter::OnOpenMessagePort(const PortContext& source,
                                               const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  MessageService::Get(browser_context_)
      ->OpenPort(port_id, render_process_id_, source);
}

void ExtensionMessageFilter::OnCloseMessagePort(const PortContext& port_context,
                                                const PortId& port_id,
                                                bool force_close) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  MessageService::Get(browser_context_)
      ->ClosePort(port_id, render_process_id_, port_context, force_close);
}

void ExtensionMessageFilter::OnPostMessage(const PortId& port_id,
                                           const Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  MessageService::Get(browser_context_)->PostMessage(port_id, message);
}

void ExtensionMessageFilter::SendWakeEventPageResponse(int request_id,
                                                       bool success) {
  Send(new ExtensionMsg_WakeEventPageResponse(request_id, success));
}

}  // namespace extensions
