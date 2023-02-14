// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/messaging_api_message_filter.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_util.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/content_script_tracker.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/trace_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
      if (!util::CanRendererHostExtensionOrigin(
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
          bad_message::ReceivedBadMessage(
              &process,
              bad_message::EMF_INVALID_EXTENSION_ID_FOR_CONTENT_SCRIPT);
          return false;
        }
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
    if (!util::CanRendererHostExtensionOrigin(process.GetID(),
                                              worker_context.extension_id)) {
      bad_message::ReceivedBadMessage(
          &process, bad_message::EMF_INVALID_EXTENSION_ID_FOR_WORKER_CONTEXT);
      return false;
    }
  }

  // This function doesn't validate frame-flavoured `source_context`s, because
  // PortContext::FrameContext only contains frame's `routing_id` and therefore
  // inherently cannot spoof frames in another process (a frame is identified
  // by its `routing_id` *and* the `process_id` of the Renderer process hosting
  // the frame;  the latter is trustworthy / doesn't come from an IPC payload).

  // This function doesn't validate native app `source_context`s, because
  // `PortContext::ForNativeHost()` is called with trustoworthy inputs (e.g. it
  // doesn't take input from IPCs sent by a Renderer process).

  return true;
}

// Returns true if `source_url` can be legitimately claimed/used by `process`.
// Otherwise reports a bad IPC message and returns false (expecting the caller
// to not take any action based on the rejected, untrustworthy `source_url`).
bool IsValidSourceUrl(content::RenderProcessHost& process,
                      const GURL& source_url,
                      const PortContext& source_context) {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionSourceUrlEnforcement)) {
    return true;
  }

  // Some scenarios may end up with an empty `source_url` (e.g. this may have
  // been triggered by the ExtensionApiTabTest.TabConnect test).
  //
  // TODO(https://crbug.com/1370079): Remove this workaround once the bug is
  // fixed.
  if (source_url.is_empty())
    return true;

  // Extract the `base_origin`.
  //
  // We don't just use (or compare against) the trustworthy
  // `render_frame_host->GetLastCommittedURL()` because the renderer-side and
  // browser-side URLs may differ in some scenarios (e.g. see
  // https://crbug.com/1197308 or `document.write`).
  //
  // We don't use `ChildProcessSecurityPolicy::CanCommitURL` because: 1) it
  // doesn't cover service workers (e.g. see https://crbug.com/1038996#c35), 2)
  // it has bugs (e.g. https://crbug.com/1380576), and 3) we *can* extract the
  // `base_origin` (via `source_context.worker->extension_id` or
  // `GetLastCommittedOrigin`) and therefore *can* use the more fundamental
  // `CanAccessDataForOrigin` (whereas `CanCommitURL` tries to work even if the
  // base origin is not available).
  url::Origin base_origin;
  if (source_context.is_for_render_frame()) {
    content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
        process.GetID(), source_context.frame->routing_id);
    if (!frame) {
      // Not calling ReceivedBadMessage because it is possible that the frame
      // got deleted before the IPC arrived.
      // Returning `false` will result in dropping the IPC by the caller - this
      // is okay, because sending of the IPC was inherently racing with the
      // deletion of the frame.
      return false;
    }
    base_origin = frame->GetLastCommittedOrigin();
  } else if (source_context.is_for_service_worker()) {
    // Validate `source_context` before using it to validate `source_url`.
    // IsValidSourceContext will call ReceivedBadMessage if needed.
    if (!IsValidSourceContext(process, source_context))
      return false;

    // `base_origin` can be considered trustworthy, because `source_context` has
    // been validated above.
    base_origin = Extension::CreateOriginFromExtensionId(
        source_context.worker->extension_id);
  } else {
    DCHECK(source_context.is_for_native_host());
    // `ExtensionHostMsg_OpenChannelToExtension` is sent in
    // `//extensions/renderer/ipc_message_sender.cc` only for frames and
    // workers (and never for native hosts).
    bad_message::ReceivedBadMessage(
        &process,
        bad_message::EMF_INVALID_OPEN_CHANNEL_TO_EXTENSION_FROM_NATIVE_HOST);
    return false;
  }

  // Verify `source_url` via CanAccessDataForOrigin.
  url::Origin source_url_origin = url::Origin::Resolve(source_url, base_origin);
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  if (!policy->CanAccessDataForOrigin(process.GetID(), source_url_origin)) {
    bad_message::ReceivedBadMessage(&process,
                                    bad_message::EMF_INVALID_SOURCE_URL);
    return false;
  }

  return true;
}

base::debug::CrashKeyString* GetTargetIdCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ExternalConnectionInfo-target_id", base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetSourceOriginCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ExternalConnectionInfo-source_origin",
      base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetSourceUrlCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ExternalConnectionInfo-source_url", base::debug::CrashKeySize::Size256);
  return crash_key;
}

class ScopedExternalConnectionInfoCrashKeys {
 public:
  explicit ScopedExternalConnectionInfoCrashKeys(
      const ExtensionMsg_ExternalConnectionInfo& info)
      : target_id_(GetTargetIdCrashKey(), info.target_id),
        source_endpoint_(info.source_endpoint),
        source_origin_(GetSourceOriginCrashKey(),
                       base::OptionalToPtr(info.source_origin)),
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

// Validates whether `source_context` can be legitimately used in the IPC
// messages sent from the given renderer `process`.  If the validation fails, or
// the sender is not associated with an extension, then `nullopt` is returned.
// The sender should ignore the IPC when `nullopt` is returned.
absl::optional<ExtensionId> ValidateSourceContextAndExtractExtensionId(
    content::RenderProcessHost& process,
    const PortContext& source_context) {
  if (!IsValidSourceContext(process, source_context))
    return absl::nullopt;

  if (source_context.is_for_service_worker())
    return source_context.worker->extension_id;

  if (source_context.is_for_render_frame()) {
    content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
        process.GetID(), source_context.frame->routing_id);
    if (!frame) {
      // Not calling ReceivedBadMessage because it is possible that the frame
      // got deleted before the IPC arrived.
      return absl::nullopt;
    }

    // These extension IPCs are on the same pipe as DidCommit() (and thus can't
    // arrive out-of-order), and therefore we can rely on
    // `frame->GetLastCommittedOrigin()` to return the origin of the IPC sender.
    const url::Origin& origin = frame->GetLastCommittedOrigin();
    // Sandboxed extension URLs have access to extension APIs (this is a bit
    // unusual - typically an opaque origin has no capabilities associated with
    // the original, precursor origin).  To avoid breaking such scenarios we
    // need to look at the precursor origin.  See https://crbug.com/1407087 for
    // an example of breakage avoided by GetTupleOrPrecursorTupleIfOpaque call.
    const url::SchemeHostPort& scheme_host_port =
        origin.GetTupleOrPrecursorTupleIfOpaque();
    if (scheme_host_port.scheme() != kExtensionScheme) {
      bad_message::ReceivedBadMessage(
          &process, bad_message::EMF_NON_EXTENSION_SENDER_FRAME);
      return absl::nullopt;
    }

    return scheme_host_port.host();
  }

  DCHECK(source_context.is_for_native_host());
  bad_message::ReceivedBadMessage(
      &process, bad_message::EMF_NON_EXTENSION_SENDER_NATIVE_HOST);
  return absl::nullopt;
}

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

content::RenderProcessHost* MessagingAPIMessageFilter::GetRenderProcessHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return nullptr;

  // The IPC might race with RenderProcessHost destruction.  This may only
  // happen in scenarios that are already inherently racey, so returning nullptr
  // (and dropping the IPC) is okay and won't lead to any additional risk of
  // data loss.
  return content::RenderProcessHost::FromID(render_process_id_);
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
    case ExtensionHostMsg_ResponsePending::ID:
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
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ResponsePending, OnResponsePending)
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
  auto* process = GetRenderProcessHost();
  if (!process)
    return;
  TRACE_EVENT("extensions", "MessageFilter::OnOpenChannelToExtension",
              ChromeTrackEvent::kRenderProcessHost, *process);

  ScopedExternalConnectionInfoCrashKeys info_crash_keys(info);
  debug::ScopedPortContextCrashKeys port_context_crash_keys(source_context);
  if (!IsValidMessagingSource(*process, info.source_endpoint) ||
      !IsValidSourceUrl(*process, info.source_url, source_context) ||
      !IsValidSourceContext(*process, source_context)) {
    // No need to call ReceivedBadMessage here, because it will be called (when
    // appropriate) within IsValidSourceContext and/or IsValidMessagingSource.
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
  auto* process = GetRenderProcessHost();
  if (!process)
    return;
  TRACE_EVENT("extensions", "MessageFilter::OnOpenChannelToNativeApp",
              ChromeTrackEvent::kRenderProcessHost, *process);

  debug::ScopedPortContextCrashKeys port_context_crash_keys(source_context);
  if (!IsValidSourceContext(*process, source_context)) {
    // No need to call ReceivedBadMessage here, because it will be called (when
    // appropriate) within IsValidSourceContext.
    return;
  }

  ChannelEndpoint source_endpoint(browser_context_, render_process_id_,
                                  source_context);
  MessageService::Get(browser_context_)
      ->OpenChannelToNativeApp(source_endpoint, port_id, native_app_name);
}

void MessagingAPIMessageFilter::OnOpenChannelToTab(
    const PortContext& source_context,
    const ExtensionMsg_TabTargetConnectionInfo& info,
    const std::string& channel_name,
    const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = GetRenderProcessHost();
  if (!process)
    return;
  TRACE_EVENT("extensions", "MessageFilter::OnOpenChannelToTab",
              ChromeTrackEvent::kRenderProcessHost, *process);

  absl::optional<ExtensionId> extension_id =
      ValidateSourceContextAndExtractExtensionId(*process, source_context);
  if (!extension_id) {
    // No need to call ReceivedBadMessage here, because it will be called (when
    // appropriate) within ValidateSourceContextAndExtractExtensionId.
    return;
  }

  ChannelEndpoint source_endpoint(browser_context_, render_process_id_,
                                  source_context);
  MessageService::Get(browser_context_)
      ->OpenChannelToTab(source_endpoint, port_id, info.tab_id, info.frame_id,
                         info.document_id, *extension_id, channel_name);
}

void MessagingAPIMessageFilter::OnOpenMessagePort(const PortContext& source,
                                                  const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = GetRenderProcessHost();
  if (!process) {
    return;
  }
  TRACE_EVENT("extensions", "MessageFilter::OnOpenMessagePort",
              ChromeTrackEvent::kRenderProcessHost, *process);

  if (!IsValidSourceContext(*process, source)) {
    return;
  }

  MessageService::Get(browser_context_)
      ->OpenPort(port_id, render_process_id_, source);
}

void MessagingAPIMessageFilter::OnCloseMessagePort(
    const PortContext& port_context,
    const PortId& port_id,
    bool force_close) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = GetRenderProcessHost();
  if (!process) {
    return;
  }
  TRACE_EVENT("extensions", "MessageFilter::OnCloseMessagePort",
              ChromeTrackEvent::kRenderProcessHost, *process);

  if (!port_context.is_for_render_frame() &&
      !port_context.is_for_service_worker()) {
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::EMF_INVALID_PORT_CONTEXT);
    return;
  }

  if (!IsValidSourceContext(*process, port_context)) {
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

void MessagingAPIMessageFilter::OnResponsePending(
    const PortContext& port_context,
    const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = GetRenderProcessHost();
  if (!process) {
    return;
  }
  TRACE_EVENT("extensions", "MessageFilter::OnResponsePending",
              ChromeTrackEvent::kRenderProcessHost, *process);

  if (!IsValidSourceContext(*process, port_context)) {
    return;
  }

  MessageService::Get(browser_context_)
      ->NotifyResponsePending(port_id, render_process_id_, port_context);
}

}  // namespace extensions
