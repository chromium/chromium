// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/network_restrictions_worker_throttle.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/connection_allowlist_gating.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "services/network/public/cpp/connection_allowlist_metrics.h"
#include "services/network/public/cpp/features.h"

namespace content {

// static
std::unique_ptr<NetworkRestrictionsWorkerThrottle>
NetworkRestrictionsWorkerThrottle::Create(
    base::WeakPtr<StoragePartitionImpl> storage_partition,
    const base::UnguessableToken& network_restrictions_id,
    PolicyContainerPolicies creator_policies,
    base::WeakPtr<RenderFrameHost> ancestor_render_frame_host) {
  if (!base::FeatureList::IsEnabled(network::features::kConnectionAllowlists)) {
    return nullptr;
  }
  return std::make_unique<NetworkRestrictionsWorkerThrottle>(
      std::move(storage_partition), network_restrictions_id,
      std::move(creator_policies), ancestor_render_frame_host);
}

NetworkRestrictionsWorkerThrottle::NetworkRestrictionsWorkerThrottle(
    base::WeakPtr<StoragePartitionImpl> storage_partition,
    const base::UnguessableToken& network_restrictions_id,
    PolicyContainerPolicies creator_policies,
    base::WeakPtr<RenderFrameHost> ancestor_render_frame_host)
    : storage_partition_(std::move(storage_partition)),
      network_restrictions_id_(network_restrictions_id),
      creator_policies_(std::move(creator_policies)),
      ancestor_render_frame_host_(ancestor_render_frame_host) {}

NetworkRestrictionsWorkerThrottle::~NetworkRestrictionsWorkerThrottle() =
    default;

void NetworkRestrictionsWorkerThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  if (!storage_partition_) {
    return;
  }

  PolicyContainerPolicies policies;
  // Feature `network::features::kConnectionAllowlists` is not checked here
  // because the throttle cannot be created if the feature is disabled.
  if (response_url.SchemeIsLocal()) {
    policies.connection_allowlists = creator_policies_.connection_allowlists;
  } else if (ResponseContainsConnectionAllowlist(response_head) &&
             ResponseEnablesConnectionAllowlistsOriginTrial(
                 response_url, response_head->headers.get())) {
    // Connection allowlist needs to be enforced for workers once the allowlist
    // response header is received. The origin trial token for this feature is
    // received within the same response. The token is parsed here to query the
    // trial status. See https://wicg.github.io/connection-allowlists/.
    policies.connection_allowlists =
        response_head->parsed_headers->connection_allowlists;
  }

  if (!policies.connection_allowlists.enforced &&
      !policies.connection_allowlists.report_only) {
    return;
  }

  if (ancestor_render_frame_host_) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        ancestor_render_frame_host_.get(),
        blink::mojom::WebFeature::kConnectionAllowlist);
  }

  if (policies.connection_allowlists.enforced) {
    network::LogConnectionAllowlistTypeHistogram(
        network::ConnectionAllowlistType::kEnforced);
  }
  if (policies.connection_allowlists.report_only) {
    network::LogConnectionAllowlistTypeHistogram(
        network::ConnectionAllowlistType::kReportOnly);
  }

  *defer = true;
  storage_partition_->RestrictNetworkForIdsInNetworkContext(
      {{network_restrictions_id_, policies.connection_allowlists}},
      base::BindOnce(&NetworkRestrictionsWorkerThrottle::OnRestrictionsApplied,
                     weak_factory_.GetWeakPtr()));
}

const char*
NetworkRestrictionsWorkerThrottle::NameForLoggingWillProcessResponse() {
  return "NetworkRestrictionsWorkerThrottle";
}

void NetworkRestrictionsWorkerThrottle::OnRestrictionsApplied() {
  delegate_->Resume();
}

}  // namespace content
