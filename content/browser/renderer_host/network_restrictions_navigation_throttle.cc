// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/network_restrictions_navigation_throttle.h"

#include "base/functional/bind.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/connection_allowlist_metrics.h"
#include "services/network/public/cpp/features.h"

namespace content {

// static
void NetworkRestrictionsNavigationThrottle::MaybeCreateAndAdd(
    NavigationThrottleRegistry& registry) {
  NavigationRequest* navigation_request =
      static_cast<NavigationRequest*>(&registry.GetNavigationHandle());

  if (!base::FeatureList::IsEnabled(network::features::kConnectionAllowlists)) {
    return;
  }

  if (navigation_request->IsSameDocument()) {
    return;
  }

  // The feature currently does not impact fenced frames.
  // TODO(crbug.com/447954811): Revisit this if the feature needs to be enabled
  // and fenced frames need to be supported.
  if (navigation_request->frame_tree_node()->IsInFencedFrameTree()) {
    return;
  }

  navigation_request->set_network_restrictions_id(
      base::UnguessableToken::Create());

  registry.AddThrottle(
      std::make_unique<NetworkRestrictionsNavigationThrottle>(registry));
}

// static
NetworkRestrictionsNavigationThrottle::NetworkRestrictionsResult
NetworkRestrictionsNavigationThrottle::MaybeApplyNetworkRestrictions(
    NavigationRequest& navigation_request,
    base::OnceClosure on_complete) {
  auto network_restrictions_id = navigation_request.network_restrictions_id();
  CHECK(network_restrictions_id.has_value());

  const auto& policy_container_policies =
      navigation_request.GetPolicyContainerPolicies();

  if (policy_container_policies.connection_allowlists.enforced) {
    network::LogConnectionAllowlistTypeHistogram(
        network::ConnectionAllowlistType::kEnforced);
  }
  if (policy_container_policies.connection_allowlists.report_only) {
    network::LogConnectionAllowlistTypeHistogram(
        network::ConnectionAllowlistType::kReportOnly);
  }

  // The origin trial status is tied to the existence of allowlists in policy
  // container. If there does not exist an enforced allowlist in policies, it
  // means either:
  // 1. the trial was not active for that context.
  // 2. or the parsed enforced allowlist is null. For example, the
  // "Connection-Allowlist" header has an empty field value.
  //
  // The network restriction id is not applied in either case.
  if (!policy_container_policies.connection_allowlists.enforced &&
      !policy_container_policies.connection_allowlists.report_only) {
    return NetworkRestrictionsResult::kProceed;
  }

  // Defer the commit until the network restrictions have been applied.
  network::ConnectionAllowlists allowlists =
      policy_container_policies.connection_allowlists;
  // Here, we're using the reporting source of the document we're committing,
  // not the initiator's reporting source.
  //
  // TODO(482728970): We shouldn't modify a copy of the policy container here.
  // Instead, we should modify the content of the policy container itself so
  // that other future callers will have a consistent view of the policy state.
  // That's difficult at the moment, as we calculate the policy container prior
  // to choosing an RFH for the document (because of sandboxing flags, etc),
  // but can't populate the reporting source until after we've chosen an RFH
  // and obtained a document token.
  allowlists.reporting_source =
      navigation_request.GetRenderFrameHost()->GetReportingSource();
  navigation_request.GetRenderFrameHost()
      ->GetStoragePartition()
      ->RestrictNetworkForIdsInNetworkContext(
          {{*network_restrictions_id, std::move(allowlists)}},
          std::move(on_complete));

  return NetworkRestrictionsResult::kDefer;
}

NetworkRestrictionsNavigationThrottle::NetworkRestrictionsNavigationThrottle(
    NavigationThrottleRegistry& registry)
    : NavigationThrottle(registry) {}

NetworkRestrictionsNavigationThrottle::
    ~NetworkRestrictionsNavigationThrottle() = default;

NavigationThrottle::ThrottleCheckResult
NetworkRestrictionsNavigationThrottle::WillProcessResponse() {
  NavigationRequest* navigation_request =
      NavigationRequest::From(navigation_handle());
  if (MaybeApplyNetworkRestrictions(
          *navigation_request,
          base::BindOnce(&NetworkRestrictionsNavigationThrottle::Resume,
                         weak_factory_.GetWeakPtr())) ==
      NetworkRestrictionsResult::kDefer) {
    return NavigationThrottle::DEFER;
  }
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
NetworkRestrictionsNavigationThrottle::WillCommitWithoutUrlLoader() {
  NavigationRequest* navigation_request =
      NavigationRequest::From(navigation_handle());
  if (MaybeApplyNetworkRestrictions(
          *navigation_request,
          base::BindOnce(&NetworkRestrictionsNavigationThrottle::Resume,
                         weak_factory_.GetWeakPtr())) ==
      NetworkRestrictionsResult::kDefer) {
    return NavigationThrottle::DEFER;
  }
  return NavigationThrottle::PROCEED;
}

const char* NetworkRestrictionsNavigationThrottle::GetNameForLogging() {
  return "NetworkRestrictionsNavigationThrottle";
}

}  // namespace content
