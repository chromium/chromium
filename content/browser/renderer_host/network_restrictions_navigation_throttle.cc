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
  if (!policy_container_policies.connection_allowlists.enforced) {
    return NetworkRestrictionsResult::kProceed;
  }

  std::set<std::string> allowlisted_patterns;
  for (const auto& pattern_string :
       policy_container_policies.connection_allowlists.enforced->allowlist) {
    allowlisted_patterns.insert(pattern_string);
  }

  // Defer the commit until the network restrictions have been applied.
  navigation_request.frame_tree_node()
      ->current_frame_host()
      ->GetStoragePartition()
      ->RevokeNetworkForNoncesInNetworkContext(
          {{*network_restrictions_id, std::move(allowlisted_patterns)}},
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
