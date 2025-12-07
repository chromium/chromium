// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NETWORK_RESTRICTIONS_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_RENDERER_HOST_NETWORK_RESTRICTIONS_COMMIT_DEFERRING_CONDITION_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/commit_deferring_condition.h"

namespace content {

class NavigationRequest;

// A CommitDeferringCondition that applies network restrictions based on
// PolicyContainerPolicies and the network_restrictions_id_ of the navigation.
// It defers commit until the network restrictions have been applied (i.e., the
// callback from RevokeNetworkForNoncesInNetworkContext is invoked).
class NetworkRestrictionsCommitDeferringCondition
    : public CommitDeferringCondition {
 public:
  static std::unique_ptr<CommitDeferringCondition> MaybeCreate(
      NavigationRequest& navigation_request);

  explicit NetworkRestrictionsCommitDeferringCondition(
      NavigationRequest& navigation_request);
  ~NetworkRestrictionsCommitDeferringCondition() override;

  // CommitDeferringCondition:
  Result WillCommitNavigation(base::OnceClosure resume) override;
  const char* TraceEventName() const override;

 private:
  void OnRevokeComplete(base::OnceClosure resume);

  base::WeakPtrFactory<NetworkRestrictionsCommitDeferringCondition>
      weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NETWORK_RESTRICTIONS_COMMIT_DEFERRING_CONDITION_H_
