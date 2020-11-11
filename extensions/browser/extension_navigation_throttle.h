// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_THROTTLE_H_
#define EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_THROTTLE_H_

#include "base/macros.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}

namespace extensions {

// This class allows the extensions subsystem to have control over navigations
// and optionally cancel/block them. This is a UI thread class.
class ExtensionNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit ExtensionNavigationThrottle(
      content::NavigationHandle* navigation_handle);
  ~ExtensionNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  // Shared throttle handler.
  ThrottleCheckResult WillStartOrRedirectRequest();

  DISALLOW_COPY_AND_ASSIGN(ExtensionNavigationThrottle);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_THROTTLE_H_
