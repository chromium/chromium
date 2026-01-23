// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NETWORK_RESTRICTIONS_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_NETWORK_RESTRICTIONS_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {

class NavigationRequest;

// A NavigationThrottle that applies network restrictions based on
// PolicyContainerPolicies and the network_restrictions_id_ of the navigation.
// It defers commit until the network restrictions have been applied (i.e., the
// callback from RevokeNetworkForNoncesInNetworkContext is invoked).
class NetworkRestrictionsNavigationThrottle : public NavigationThrottle {
 public:
  enum class NetworkRestrictionsResult {
    kProceed,
    kDefer,
  };

  static void MaybeCreateAndAdd(NavigationThrottleRegistry& registry);

  static NetworkRestrictionsResult MaybeApplyNetworkRestrictions(
      NavigationRequest& navigation_request,
      base::OnceClosure on_complete);

  explicit NetworkRestrictionsNavigationThrottle(
      NavigationThrottleRegistry& registry);
  ~NetworkRestrictionsNavigationThrottle() override;

  // NavigationThrottle:
  ThrottleCheckResult WillProcessResponse() override;
  ThrottleCheckResult WillCommitWithoutUrlLoader() override;
  const char* GetNameForLogging() override;

 private:
  void OnRevokeComplete();

  base::WeakPtrFactory<NetworkRestrictionsNavigationThrottle> weak_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NETWORK_RESTRICTIONS_NAVIGATION_THROTTLE_H_
