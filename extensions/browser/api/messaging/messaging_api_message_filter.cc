// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/messaging_api_message_filter.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/content_script_tracker.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/trace_util.h"

using content::BrowserThread;
using content::RenderProcessHost;
using perfetto::protos::pbzero::ChromeTrackEvent;

namespace extensions {

namespace {

class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  ShutdownNotifierFactory(const ShutdownNotifierFactory&) = delete;
  ShutdownNotifierFactory& operator=(const ShutdownNotifierFactory&) = delete;

  static ShutdownNotifierFactory* GetInstance() {
    return base::Singleton<ShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "ExtensionMessageFilter") {
    DependsOn(EventRouterFactory::GetInstance());
  }
  ~ShutdownNotifierFactory() override = default;
};

// Returns true if the process corresponding to `render_process_id` can host an
// extension with `extension_id`.  (It doesn't necessarily mean that the process
// *does* host this specific extension at this point in time.)
bool CanRendererHostExtensionOrigin(int render_process_id,
                                    const std::string& extension_id) {
  url::Origin extension_origin =
      Extension::CreateOriginFromExtensionId(extension_id);
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
        if (!base::FeatureList::IsEnabled(
                extensions_features::kCheckingNoExtensionIdInExtensionIpcs)) {
          base::UmaHistogramSparse(
              "Stability.BadMessageTerminated.Extensions",
              bad_message::EMF_NO_EXTENSION_ID_FOR_EXTENSION_SOURCE);
          return true;
        }
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
      if (source_endpoint.extension_id.has_value()) {
        const std::string& extension_id = source_endpoint.extension_id.value();
        bool is_content_script_expected =
            ContentScriptTracker::DidProcessRunContentScriptFromExtension(
                process, extension_id);
        if (!is_content_script_expected) {
          // TODO(https://crbug.com/1212918): Remove some of the more excessive
          // tracing once there are no more bad message reports to investigate.
          // (Remove here + in ContentScriptTracker.)
          TRACE_EVENT_INSTANT("extensions",
                              "IsValidMessagingSource: kTab: bad message",
                              ChromeTrackEvent::kRenderProcessHost, process,
                              ChromeTrackEvent::kChromeExtensionId,
                              ExtensionIdForTracing(extension_id));
          if (!base::FeatureList::IsEnabled(
                  extensions_features::
                      kCheckingUnexpectedExtensionIdInContentScriptIpcs)) {
            base::UmaHistogramSparse(
                "Stability.BadMessageTerminated.Extensions",
                bad_message::EMF_INVALID_EXTENSION_ID_FOR_CONTENT_SCRIPT);
            return true;
          }
          bad_message::ReceivedBadMessage(
              &process,
              bad_message::EMF_INVALID_EXTENSION_ID_FOR_CONTENT_SCRIPT);
          return false;
        }
        TRACE_EVENT_INSTANT("extensions", "IsValidMessagingSource: kTab: ok",
                            ChromeTrackEvent::kRenderProcessHost, process,
                            ChromeTrackEvent::kChromeExtensionId,
                            ExtensionIdForTracing(extension_id));
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

MessagingAPIMessageFilter::MessagingAPIMessageFilter(
    int render_process_id,
    content::BrowserContext* context)
    : BrowserMessageFilter(ExtensionMsgStart),
      render_process_id_(render_process_id),
      browser_context_(context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  shutdown_notifier_subscription_ =
      ShutdownNotifierFactory::GetInstance()->Get(context)->Subscribe(
          base::BindRepeating(&MessagingAPIMessageFilter::Shutdown,
                              base::Unretained(this)));
}

MessagingAPIMessageFilter::~MessagingAPIMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void MessagingAPIMessageFilter::Shutdown() {
  browser_context_ = nullptr;
  shutdown_notifier_subscription_ = {};
}

void MessagingAPIMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
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

void MessagingAPIMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

bool MessagingAPIMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MessagingAPIMessageFilter, message)
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

void MessagingAPIMessageFilter::OnOpenChannelToExtension(
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
  TRACE_EVENT("extensions", "MessageFilter::OnOpenChannelToExtension",
              ChromeTrackEvent::kRenderProcessHost, *process);

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

void MessagingAPIMessageFilter::OnOpenChannelToNativeApp(
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

void MessagingAPIMessageFilter::OnOpenChannelToTab(
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
                         info.document_id, extension_id, channel_name);
}

void MessagingAPIMessageFilter::OnOpenMessagePort(const PortContext& source,
                                                  const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  MessageService::Get(browser_context_)
      ->OpenPort(port_id, render_process_id_, source);
}

void MessagingAPIMessageFilter::OnCloseMessagePort(
    const PortContext& port_context,
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

void MessagingAPIMessageFilter::OnPostMessage(const PortId& port_id,
                                              const Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  MessageService::Get(browser_context_)->PostMessage(port_id, message);
}

}  // namespace extensions
