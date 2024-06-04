// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/extension_message_port.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/trace_util.h"

using content::BrowserThread;
using content::RenderProcessHost;
using perfetto::protos::pbzero::ChromeTrackEvent;

namespace extensions {

namespace {

// Returns true if `context` corresponds to a sandboxed extension frame. This
// can only be true for extension *frames*: extension ServiceWorkers are never
// sandboxed, since ServiceWorkers cannot be associated with an opaque origin,
// and native contexts cannot be sandboxed since they do not originate from a
// renderer process.
bool IsPortContextSandboxed(RenderProcessHost& process,
                            const PortContext& context) {
  if (!context.is_for_render_frame()) {
    return false;
  }

  content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
      process.GetID(), context.frame->routing_id);

  if (!frame) {
    // TODO(https://crbug.com/325410297): It should not be possible to reach
    // this check when `context.is_for_render_frame()` is true, and yet there's
    // no corresponding RenderFrameHost, since a PortContext for frames is
    // always created with a non-null RenderFrameHost (e.g., in
    // ExtensionFrameHost::OpenChannelToExtension()). Ensure there are no
    // unexpected reports of this and then remove the early return here and also
    // in IsValidSourceUrl().
    DUMP_WILL_BE_NOTREACHED();
    return false;
  }

  return frame->IsSandboxed(network::mojom::WebSandboxFlags::kOrigin);
}

// Returns true if `source_endpoint` can be legitimately claimed/used by
// `process`.  Otherwise reports a bad IPC message and returns false (expecting
// the caller to not take any action based on the rejected, untrustworthy
// `source_endpoint`). `source_context` provides additional information about
// the source, such as whether it refers to a frame or a worker.
bool IsValidMessagingSource(RenderProcessHost& process,
                            const MessagingEndpoint& source_endpoint,
                            const PortContext& source_context) {
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
              process.GetID(), source_endpoint.extension_id.value(),
              IsPortContextSandboxed(process, source_context))) {
        bad_message::ReceivedBadMessage(
            &process,
            bad_message::EMF_INVALID_EXTENSION_ID_FOR_EXTENSION_SOURCE);
        return false;
      }
      return true;

    case MessagingEndpoint::Type::kContentScript: {
      if (!source_endpoint.extension_id) {
        bad_message::ReceivedBadMessage(
            &process, bad_message::EMF_INVALID_EXTENSION_ID_FOR_CONTENT_SCRIPT);
        return false;
      }
      bool is_content_script_expected =
          ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
              process, *source_endpoint.extension_id);
      if (!is_content_script_expected) {
        debug::ScopedScriptInjectionTrackerFailureCrashKeys tracker_keys(
            *process.GetBrowserContext(), source_endpoint.extension_id.value());
        bad_message::ReceivedBadMessage(
            &process, bad_message::EMF_INVALID_EXTENSION_ID_FOR_CONTENT_SCRIPT);
        return false;
      }
      return true;
    }

    case MessagingEndpoint::Type::kUserScript: {
      if (!source_endpoint.extension_id) {
        bad_message::ReceivedBadMessage(
            &process, bad_message::EMF_INVALID_EXTENSION_ID_FOR_USER_SCRIPT);
        return false;
      }
      bool is_user_script_expected =
          ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
              process, *source_endpoint.extension_id);
      if (!is_user_script_expected) {
        bad_message::ReceivedBadMessage(
            &process, bad_message::EMF_INVALID_EXTENSION_ID_FOR_USER_SCRIPT);
        return false;
      }

      return true;
    }

    case MessagingEndpoint::Type::kWebPage:
      // NOTE: We classify hosted apps as kWebPage, but we don't include
      // the extension ID in the source for those messages.
      if (source_endpoint.extension_id) {
        bad_message::ReceivedBadMessage(
            &process, bad_message::EMF_INVALID_EXTENSION_ID_FOR_WEB_PAGE);
        return false;
      }
      return true;
  }
}

bool IsValidMessagingTarget(RenderProcessHost& process,
                            const MessagingEndpoint& source_endpoint,
                            const ExtensionId& target_id) {
  switch (source_endpoint.type) {
    case MessagingEndpoint::Type::kNativeApp:
    case MessagingEndpoint::Type::kExtension:
    case MessagingEndpoint::Type::kWebPage:
    case MessagingEndpoint::Type::kContentScript:
      // The API allows these to target any source. The connection may be
      // refused (e.g. if the target extension isn't installed or doesn't accept
      // a connection from the source), but it isn't a sign of a bad IPC.
      return true;
    case MessagingEndpoint::Type::kUserScript:
      // User scripts can only target their own corresponding extension.
      // `source_endpoint.extension_id` should have been validated above in
      // `IsValidMessagingSource()`.
      CHECK(source_endpoint.extension_id);
      if (source_endpoint.extension_id != target_id) {
        bad_message::ReceivedBadMessage(
            &process,
            bad_message::EMF_INVALID_EXTERNAL_EXTENSION_ID_FOR_USER_SCRIPT);
        return false;
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
    if (!util::CanRendererHostExtensionOrigin(
            process.GetID(), worker_context.extension_id,
            IsPortContextSandboxed(process, source_context))) {
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
  // TODO(crbug.com/40240882): Remove this workaround once the bug is
  // fixed.
  if (source_url.is_empty()) {
    return true;
  }

  // Extract the `base_origin`.
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

    if (frame->GetLastCommittedURL() == source_url) {
      // If the trustworthy, browser-side URL matches `source_url` from the IPC
      // payload, then report that the IPC is valid.  If the URLs don't match
      // then we can't assume that the IPC is malformed and `return false`,
      // because the renderer-side and browser-side URLs may differ in some
      // scenarios (e.g. see https://crbug.com/1197308 or `document.write`).  In
      // such scenarios we want to fall back to `base_origin`-based /
      // `source_url_origin``-based checks, but these checks are not 100%
      // correct (see https://crbug.com/1449796), so `GetLastCommittedURL` is
      // consulted first.
      return true;
    }

    base_origin = frame->GetLastCommittedOrigin();
  } else if (source_context.is_for_service_worker()) {
    // Validate `source_context` before using it to validate `source_url`.
    // IsValidSourceContext will call ReceivedBadMessage if needed.
    if (!IsValidSourceContext(process, source_context)) {
      return false;
    }

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

  // Verify `source_url` via ChildProcessSecurityPolicy::HostsOrigin.
  //
  // TODO(crbug.com/40915015): Stop partially/not-100%-correctly
  // replicating checks from `RenderFrameHostImpl::CanCommitOriginAndUrl`.
  // The code below correctly handles URLs like `about:blank`, but may diverge
  // from //content checks in some cases (e.g. WebUI checks are not replicated
  // here;  MHTML divergence is avoided via GetLastCommittedURL() check above).
  url::Origin source_url_origin = url::Origin::Resolve(source_url, base_origin);
  if (IsPortContextSandboxed(process, source_context)) {
    // If `source_url` came from a sandboxed extension, convert the origin to
    // an opaque origin, since HostsOrigin() enforces that sandboxed processes
    // can only access opaque origins. Note that `source_url`'s origin will be
    // maintained in the precursor and also validated by HostsOrigin().
    source_url_origin = source_url_origin.DeriveNewOpaqueOrigin();
  }
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  if (!policy->HostsOrigin(process.GetID(), source_url_origin)) {
    SCOPED_CRASH_KEY_STRING256(
        "EMF_INVALID_SOURCE_URL", "base_origin",
        base_origin.GetDebugString(false /* include_nonce */));
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
      const MessageService::ExternalConnectionInfo& info)
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
std::optional<ExtensionId> ValidateSourceContextAndExtractExtensionId(
    content::RenderProcessHost& process,
    const PortContext& source_context) {
  if (!IsValidSourceContext(process, source_context)) {
    return std::nullopt;
  }

  if (source_context.is_for_service_worker()) {
    return source_context.worker->extension_id;
  }

  if (source_context.is_for_render_frame()) {
    content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
        process.GetID(), source_context.frame->routing_id);
    if (!frame) {
      // Not calling ReceivedBadMessage because it is possible that the frame
      // got deleted before the IPC arrived.
      return std::nullopt;
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
      SCOPED_CRASH_KEY_STRING256(
          "EMF_NON_EXTENSION_SENDER_FRAME", "origin",
          origin.GetDebugString(false /* include_nonce */));
      bad_message::ReceivedBadMessage(
          &process, bad_message::EMF_NON_EXTENSION_SENDER_FRAME);
      return std::nullopt;
    }

    return scheme_host_port.host();
  }

  DCHECK(source_context.is_for_native_host());
  bad_message::ReceivedBadMessage(
      &process, bad_message::EMF_NON_EXTENSION_SENDER_NATIVE_HOST);
  return std::nullopt;
}

}  // namespace

void MessageService::OpenChannelToExtension(
    const ChannelEndpoint& source,
    const PortId& source_port_id,
    const ExternalConnectionInfo& info,
    mojom::ChannelType channel_type,
    const std::string& channel_name,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
        port_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process =
      content::RenderProcessHost::FromID(source.render_process_id());
  if (!process) {
    return;
  }
  ScopedExternalConnectionInfoCrashKeys info_crash_keys(info);
  debug::ScopedPortContextCrashKeys port_context_crash_keys(
      source.port_context());
  if (!IsValidMessagingSource(*process, info.source_endpoint,
                              source.port_context()) ||
      !IsValidMessagingTarget(*process, info.source_endpoint, info.target_id) ||
      !IsValidSourceUrl(*process, info.source_url, source.port_context()) ||
      !IsValidSourceContext(*process, source.port_context())) {
    // No need to call ReceivedBadMessage here, because it will be called (when
    // appropriate) within IsValidSourceContext and/or IsValidMessagingSource.
    return;
  }

  std::unique_ptr<MessagePort> opener_port =
      ExtensionMessagePort::CreateForEndpoint(
          weak_factory_.GetWeakPtr(), source_port_id,
          info.source_endpoint.extension_id ? *info.source_endpoint.extension_id
                                            : ExtensionId(),
          source, std::move(port), std::move(port_host));

  OpenChannelToExtension(source, source_port_id, info.source_endpoint,
                         std::move(opener_port), info.target_id,
                         info.source_url, channel_type, channel_name);
}

void MessageService::OpenChannelToNativeApp(
    const ChannelEndpoint& source,
    const PortId& source_port_id,
    const std::string& native_app_name,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
        port_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process =
      content::RenderProcessHost::FromID(source.render_process_id());
  if (!process) {
    return;
  }
  debug::ScopedPortContextCrashKeys port_context_crash_keys(
      source.port_context());
  if (!IsValidSourceContext(*process, source.port_context())) {
    // No need to call ReceivedBadMessage here, because it will be called (when
    // appropriate) within IsValidSourceContext.
    return;
  }

  OpenChannelToNativeAppImpl(source, source_port_id, native_app_name,
                             std::move(port), std::move(port_host));
}

void MessageService::OpenChannelToTab(
    const ChannelEndpoint& source,
    const PortId& source_port_id,
    int tab_id,
    int frame_id,
    const std::string& document_id,
    mojom::ChannelType channel_type,
    const std::string& channel_name,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
        port_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process =
      content::RenderProcessHost::FromID(source.render_process_id());
  if (!process) {
    return;
  }
  std::optional<ExtensionId> extension_id =
      ValidateSourceContextAndExtractExtensionId(*process,
                                                 source.port_context());
  if (!extension_id) {
    // No need to call ReceivedBadMessage here, because it will be called (when
    // appropriate) within ValidateSourceContextAndExtractExtensionId.
    return;
  }

  OpenChannelToTabImpl(source, source_port_id, tab_id, frame_id, document_id,
                       *extension_id, channel_type, channel_name,
                       std::move(port), std::move(port_host));
}

void MessageService::OpenPort(RenderProcessHost* process,
                              const PortId& port_id,
                              const PortContext& source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!IsValidSourceContext(*process, source)) {
    return;
  }
  OpenPortImpl(port_id, process->GetID(), source);
}

void MessageService::ClosePort(RenderProcessHost* process,
                               const PortId& port_id,
                               const PortContext& port_context,
                               bool force_close) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!port_context.is_for_render_frame() &&
      !port_context.is_for_service_worker()) {
    bad_message::ReceivedBadMessage(process,
                                    bad_message::EMF_INVALID_PORT_CONTEXT);
    return;
  }

  if (!IsValidSourceContext(*process, port_context)) {
    return;
  }
  int routing_id =
      port_context.frame ? port_context.frame->routing_id : MSG_ROUTING_NONE;
  int worker_thread_id =
      port_context.worker ? port_context.worker->thread_id : kMainThreadId;
  ClosePortImpl(port_id, process->GetID(), routing_id, worker_thread_id,
                force_close, std::string());
}

void MessageService::NotifyResponsePending(RenderProcessHost* process,
                                           const PortId& port_id,
                                           const PortContext& port_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!IsValidSourceContext(*process, port_context)) {
    return;
  }

  NotifyResponsePending(port_id);
}

}  // namespace extensions
