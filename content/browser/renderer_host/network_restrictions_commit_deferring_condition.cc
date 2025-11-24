// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/network_restrictions_commit_deferring_condition.h"

#include "base/functional/bind.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/features.h"

namespace content {

NetworkRestrictionsCommitDeferringCondition::
    NetworkRestrictionsCommitDeferringCondition(
        NavigationRequest& navigation_request)
    : CommitDeferringCondition(navigation_request) {}

NetworkRestrictionsCommitDeferringCondition::
    ~NetworkRestrictionsCommitDeferringCondition() {}

const char* NetworkRestrictionsCommitDeferringCondition::TraceEventName()
    const {
  return "NetworkRestrictionsCommitDeferringCondition";
}

// static
std::unique_ptr<CommitDeferringCondition>
NetworkRestrictionsCommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request) {
  if (navigation_request.IsSameDocument()) {
    return nullptr;
  }

  // If this navigation is either canceled or failed, no need for network
  // restrictions to be applied.
  if (navigation_request.state() == NavigationRequest::CANCELING ||
      navigation_request.state() == NavigationRequest::WILL_FAIL_REQUEST ||
      navigation_request.state() == NavigationRequest::DID_COMMIT_ERROR_PAGE) {
    return nullptr;
  }

  if (!base::FeatureList::IsEnabled(network::features::kConnectionAllowlists)) {
    return nullptr;
  }

  // The feature currently does not impact fenced frames.
  // TODO(crbug.com/447954811): Revisit this if the feature needs to be enabled
  // and fenced frames need to be supported.
  if (navigation_request.frame_tree_node()->IsInFencedFrameTree()) {
    return nullptr;
  }

  navigation_request.set_network_restrictions_id(
      base::UnguessableToken::Create());
  return std::make_unique<NetworkRestrictionsCommitDeferringCondition>(
      navigation_request);
}
CommitDeferringCondition::Result
NetworkRestrictionsCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  NavigationRequest* navigation_request =
      NavigationRequest::From(&GetNavigationHandle());
  const auto& policy_container_policies =
      navigation_request->GetPolicyContainerPolicies();

  auto network_restrictions_id = navigation_request->network_restrictions_id();
  if (!network_restrictions_id.has_value()) {
    return Result::kProceed;
  }

  if (!policy_container_policies.connection_allowlists.enforced) {
    return Result::kProceed;
  }

  std::set<std::string> allowlisted_patterns;
  for (const auto& pattern_string :
       policy_container_policies.connection_allowlists.enforced->allowlist) {
    allowlisted_patterns.insert(pattern_string);
  }

  // Defer the commit until the network restrictions have been applied.
  navigation_request->frame_tree_node()
      ->current_frame_host()
      ->GetStoragePartition()
      ->RevokeNetworkForNoncesInNetworkContext(
          {{*network_restrictions_id, std::move(allowlisted_patterns)}},
          base::BindOnce(
              &NetworkRestrictionsCommitDeferringCondition::OnRevokeComplete,
              weak_factory_.GetWeakPtr(), std::move(resume)));

  return Result::kDefer;
}

void NetworkRestrictionsCommitDeferringCondition::OnRevokeComplete(
    base::OnceClosure resume) {
  std::move(resume).Run();
}

}  // namespace content
