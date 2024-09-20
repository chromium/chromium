// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PARTITIONED_POPINS_PARTITIONED_POPINS_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_PARTITIONED_POPINS_PARTITIONED_POPINS_NAVIGATION_THROTTLE_H_

#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// This throttle should be attached to all top-frame navigations for partitioned
// popins to handle blocks and headers.
// See https://explainers-by-googlers.github.io/partitioned-popins/
class CONTENT_EXPORT PartitionedPopinsNavigationThrottle
    : public NavigationThrottle {
 public:
  static std::unique_ptr<PartitionedPopinsNavigationThrottle>
  MaybeCreateThrottleFor(NavigationHandle* navigation_handle);

  // NavigationThrottle
  const char* GetNameForLogging() override;
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override;
  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override;

 private:
  explicit PartitionedPopinsNavigationThrottle(
      NavigationHandle* navigation_handle);

  // Whether the `Popin-Policy` response header blocks access.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  bool DoesPopinPolicyBlockResponse();
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PARTITIONED_POPINS_PARTITIONED_POPINS_NAVIGATION_THROTTLE_H_
