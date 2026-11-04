// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_broker/resource_broker_routing.h"

#include <algorithm>
#include <string>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/agent_cluster_key.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "net/base/isolation_info.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

// `frame` may be speculative during commit. Do not read per-document state
// on it.
ResourceBrokerRoutingDecision ComputeDecision(
    RenderFrameHostImpl* frame,
    const url::Origin& origin_lock,
    const net::IsolationInfo& isolation_info) {
  const ProcessLock process_lock = frame->GetProcess()->GetProcessLock();

  if (process_lock.is_error_page()) {
    return ResourceBrokerRoutingDecision::kErrorPage;
  }
  if (origin_lock.opaque()) {
    return ResourceBrokerRoutingDecision::kOpaqueOrigin;
  }

  const std::string& scheme = origin_lock.scheme();
  if (std::ranges::contains(URLDataManagerBackend::GetWebUISchemes(), scheme)) {
    return ResourceBrokerRoutingDecision::kWebUI;
  }
  if (blink::CommonSchemeRegistry::IsExtensionScheme(scheme)) {
    return ResourceBrokerRoutingDecision::kExtension;
  }

  const WebExposedIsolationInfo web_exposed_isolation =
      process_lock.GetWebExposedIsolationInfo();
  if (web_exposed_isolation.is_isolated_application()) {
    return ResourceBrokerRoutingDecision::kIsolatedWebApp;
  }

  if (scheme != url::kHttpScheme && scheme != url::kHttpsScheme) {
    return ResourceBrokerRoutingDecision::kSchemeNotHttpOrHttps;
  }

  if (!process_lock.IsLockedToSite()) {
    return ResourceBrokerRoutingDecision::kNotLockedToSite;
  }
  if (process_lock.is_pdf()) {
    return ResourceBrokerRoutingDecision::kPdf;
  }
  if (process_lock.is_guest()) {
    return ResourceBrokerRoutingDecision::kGuest;
  }
  if (process_lock.is_privileged()) {
    return ResourceBrokerRoutingDecision::kPrivileged;
  }
  if (web_exposed_isolation.is_isolated() ||
      process_lock.agent_cluster_key().IsCrossOriginIsolated()) {
    return ResourceBrokerRoutingDecision::kCrossOriginIsolated;
  }

  // Credentialless iframes partition network state via a nonce.
  if (isolation_info.nonce().has_value()) {
    return ResourceBrokerRoutingDecision::kNoncePartitioned;
  }

  // `ServiceWorkerClient::CommitResponse()` attaches the client to `frame`
  // before `RenderFrameHostImpl::CommitNavigation()` creates subresource
  // factories. Uncontrolled pages that later gain a controller via
  // `clients.claim()` are only reflected if factories are recreated.
  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      frame->GetLastCommittedServiceWorkerClient();
  if (service_worker_client && service_worker_client->controller()) {
    return ResourceBrokerRoutingDecision::kServiceWorkerControlled;
  }

  return ResourceBrokerRoutingDecision::kRouted;
}

}  // namespace

ResourceBrokerRoutingDecision RecordResourceBrokerRoutingDecision(
    RenderFrameHost* frame,
    const url::Origin& origin_lock,
    const net::IsolationInfo& isolation_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(frame);

  const ResourceBrokerRoutingDecision decision = ComputeDecision(
      static_cast<RenderFrameHostImpl*>(frame), origin_lock, isolation_info);
  base::UmaHistogramEnumeration("ResourceBroker.RoutingDecision", decision);
  // Unlike `IsOutermostMainFrame()`, this also treats guest views as embedded.
  base::UmaHistogramEnumeration(
      frame->GetParentOrOuterDocumentOrEmbedder()
          ? "ResourceBroker.RoutingDecision.Subframe"
          : "ResourceBroker.RoutingDecision.MainFrame",
      decision);
  return decision;
}

}  // namespace content
