// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_message_filter.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "components/crx_file/id_util.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/content_script_tracker.h"
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

// Returns true if the process corresponding to `render_process_id` can host an
// extension with `extension_id`.  (It doesn't necessarily mean that the process
// *does* host this specific extension at this point in time.)
bool CanRendererHostExtensionOrigin(int render_process_id,
                                    const std::string& extension_id) {
  GURL extension_url = Extension::GetBaseURLFromExtensionId(extension_id);
  url::Origin extension_origin = url::Origin::Create(extension_url);
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  return policy->CanAccessDataForOrigin(render_process_id, extension_origin);
}

// Returns true if `source_endpoint` can be legitimately claimed/used by
// `process`.  Otherwise reports a bad IPC message and returns false (expecting
// the caller to not take any action based on the rejected, untrustworthy
// `source_endpoint`).
bool IsValidMessagingSource(RenderProcessHost& process,
                            const MessagingEndpoint& source_endpoint) {
  switch (source_endpoint.type) {
    case MessagingEndpoint::Type::kNativeApp:
      // Requests for channels initiated by native applications don't originate
      // from renderer processes.
      bad_message::ReceivedBadMessage(
          &process, bad_message::EMF_INVALID_CHANNEL_SOURCE_TYPE);
      return false;

    case MessagingEndpoint::Type::kExtension:
      if (!source_endpoint.extension_id.has_value()) {
        bad_message::ReceivedBadMessage(
            &process, bad_message::EMF_NO_EXTENSION_ID_FOR_EXTENSION_SOURCE);
        return false;
      }
      if (!CanRendererHostExtensionOrigin(
              process.GetID(), source_endpoint.extension_id.value())) {
        bad_message::ReceivedBadMessage(
            &process,
            bad_message::EMF_INVALID_EXTENSION_ID_FOR_EXTENSION_SOURCE);
        return false;
      }
      return true;

    case MessagingEndpoint::Type::kTab:
      if (source_endpoint.extension_id.has_value() &&
          !ContentScriptTracker::DidProcessRunContentScriptFromExtension(
              process, source_endpoint.extension_id.value())) {
        // TODO(https://crbug.com/1212918: Re-enable the enforcement after
        // investigating and fixing the root cause of bad message reports coming
        // from the end users.
        LOG(ERROR) << "EMF_INVALID_EXTENSION_ID_FOR_CONTENT_SCRIPT";
      }
      return true;
  }
}

// Returns true if `source_context` can be legitimately claimed/used by
// `render_process_id`.  Otherwise reports a bad IPC message and returns false
// (expecting the caller to not take any action based on the rejected,
// untrustworthy `source_context`).
bool IsValidSourceContext(RenderProcessHost& process,
                          const PortContext& source_context) {
  if (source_context.is_for_service_worker()) {
    const PortContext::WorkerContext& worker_context =
        source_context.worker.value();

    // Only crude checks via CanRendererHostExtensionOrigin are done here,
    // because more granular, worker-specific checks (e.g. checking if a worker
    // exists using ProcessManager::HasServiceWorker) might incorrectly return
    // false=invalid-IPC for IPCs from workers that were recently torn down /
    // made inactive.
    if (!CanRendererHostExtensionOrigin(process.GetID(),
                                        worker_context.extension_id)) {
      bad_message::ReceivedBadMessage(
          &process, bad_message::EMF_INVALID_EXTENSION_ID_FOR_WORKER_CONTEXT);
      return false;
    }
  }

  return true;
}

base::debug::CrashKeyString* GetTargetIdCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ExternalConnectionInfo::target_id", base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetSourceOriginCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ExternalConnectionInfo::source_origin",
      base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetSourceUrlCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ExternalConnectionInfo::source_url", base::debug::CrashKeySize::Size256);
  return crash_key;
}

class ScopedExternalConnectionInfoCrashKeys {
 public:
  explicit ScopedExternalConnectionInfoCrashKeys(
      const ExtensionMsg_ExternalConnectionInfo& info)
      : target_id_(GetTargetIdCrashKey(), info.target_id),
        source_endpoint_(info.source_endpoint),
        source_origin_(GetSourceOriginCrashKey(),
                       base::OptionalOrNullptr(info.source_origin)),
        source_url_(GetSourceUrlCrashKey(),
                    info.source_url.possibly_invalid_spec()) {}

  ~ScopedExternalConnectionInfoCrashKeys() = default;

  ScopedExternalConnectionInfoCrashKeys(
      const ScopedExternalConnectionInfoCrashKeys&) = delete;
  ScopedExternalConnectionInfoCrashKeys& operator=(
      const ScopedExternalConnectionInfoCrashKeys&) = delete;

 private:
  base::debug::ScopedCrashKeyString target_id_;
  extensions::debug::ScopedMessagingEndpointCrashKeys source_endpoint_;
  url::debug::ScopedOriginCrashKey source_origin_;
  base::debug::ScopedCrashKeyString source_url_;
};

}  // namespace

ExtensionMessageFilter::ExtensionMessageFilter(int render_process_id,
                                               content::BrowserContext* context)
    : BrowserMessageFilter(ExtensionMsgStart),
      render_process_id_(render_process_id),
      browser_context_(context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  shutdown_notifier_subscription_ =
      ShutdownNotifierFactory::GetInstance()->Get(context)->Subscribe(
          base::BindRepeating(&ExtensionMessageFilter::ShutdownOnUIThread,
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
  shutdown_notifier_subscription_ = {};
}

void ExtensionMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
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
  if (!browser_context_)
    return;

  // The IPC might race with RenderProcessHost destruction.  This may only
  // happen in scenarios that are already inherently racey, so dropping the IPC
  // is okay and won't lead to any additional risk of data loss.
  auto* process = content::RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  ScopedExternalConnectionInfoCrashKeys info_crash_keys(info);
  if (!IsValidMessagingSource(*process, info.source_endpoint) ||
      !IsValidSourceContext(*process, source_context)) {
    return;
  }

  ChannelEndpoint source_endpoint(browser_context_, render_process_id_,
                                  source_context);
  MessageService::Get(browser_context_)
      ->OpenChannelToExtension(source_endpoint, port_id, info.source_endpoint,
                               nullptr /* opener_port */, info.target_id,
                               info.source_url, channel_name);
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

  // Note, we need to add more stringent IPC validation here.
  if (!port_context.is_for_render_frame() &&
      !port_context.is_for_service_worker()) {
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::EMF_INVALID_PORT_CONTEXT);
    return;
  }

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
