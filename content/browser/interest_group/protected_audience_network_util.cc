// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/protected_audience_network_util.h"

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace content {

std::optional<std::string> GetUserAgentOverrideForProtectedAudience(
    FrameTreeNode* frame_tree_node) {
  if (frame_tree_node == nullptr) {
    return std::nullopt;
  }
  if (!base::FeatureList::IsEnabled(
          features::kFledgeEnableUserAgentOverrides)) {
    return std::nullopt;
  }
  if (!frame_tree_node->navigator()
           .GetDelegate()
           ->ShouldOverrideUserAgentForRendererInitiatedNavigation()) {
    return std::nullopt;
  }
  std::string maybe_user_agent =
      frame_tree_node->navigator()
          .GetDelegate()
          ->GetUserAgentOverride(frame_tree_node->frame_tree())
          .ua_string_override;
  if (maybe_user_agent.empty()) {
    return std::nullopt;
  }
  return std::move(maybe_user_agent);
}

std::optional<std::string> GetUserAgentOverrideForProtectedAudience(
    FrameTreeNodeId frame_tree_node_id) {
  // This typically only happens in unit tests.
  if (!frame_tree_node_id) {
    return std::nullopt;
  }
  return GetUserAgentOverrideForProtectedAudience(
      FrameTreeNode::GloballyFindByID(frame_tree_node_id));
}

void SetUpDevtoolsForRequest(FrameTreeNode* frame_tree_node,
                             network::ResourceRequest& request) {
  request.throttling_profile_id =
      frame_tree_node->current_frame_host()->devtools_frame_token();

  bool network_instrumentation_enabled = false;
  devtools_instrumentation::ApplyAuctionNetworkRequestOverrides(
      frame_tree_node, &request, &network_instrumentation_enabled);
  if (network_instrumentation_enabled) {
    request.enable_load_timing = true;
    if (request.trusted_params.has_value()) {
      request.trusted_params->devtools_observer =
          NetworkServiceDevToolsObserver::MakeSelfOwned(frame_tree_node);
    }
  }
}

network::mojom::ClientSecurityStatePtr
CreateClientSecurityStateForProtectedAudience(
    network::mojom::IPAddressSpace ip_address_space) {
  auto client_security_state = network::mojom::ClientSecurityState::New();
  client_security_state->ip_address_space = ip_address_space;
  client_security_state->is_web_secure_context = true;
  client_security_state->private_network_request_policy =
      network::mojom::PrivateNetworkRequestPolicy::kBlock;
  return client_security_state;
}

}  // namespace content
