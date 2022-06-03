// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PORTAL_PORTAL_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_PORTAL_PORTAL_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/auto_reset.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// Applies restrictions to portal navigation.
//
// Unless cross-origin portals are enabled,, restricts navigation within a
// portal main frame to only the origin of its host. This allows a more limited
// testing mode of the portals feature, in which third-party (cross-origin)
// content cannot be loaded.
class CONTENT_EXPORT PortalNavigationThrottle : public NavigationThrottle {
 public:
  static std::unique_ptr<PortalNavigationThrottle> MaybeCreateThrottleFor(
      NavigationHandle* navigation_handle);

  ~PortalNavigationThrottle() override;

  // NavigationThrottle
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

 private:
  PortalNavigationThrottle(NavigationHandle* navigation_handle);

  ThrottleCheckResult WillStartOrRedirectRequest();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PORTAL_PORTAL_NAVIGATION_THROTTLE_H_
