// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_UNTRUSTED_WEB_UI_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_UNTRUSTED_WEB_UI_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {

// Throttles unexpected renderer-initiated navigations targeting frames
// that are currently displaying chrome-untrusted:// WebUI content (e.g. via
// window.opener or cross-origin child iframes).
class UntrustedWebUINavigationThrottle final : public NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(NavigationThrottleRegistry& registry);

  explicit UntrustedWebUINavigationThrottle(
      NavigationThrottleRegistry& registry);
  UntrustedWebUINavigationThrottle(const UntrustedWebUINavigationThrottle&) =
      delete;
  UntrustedWebUINavigationThrottle& operator=(
      const UntrustedWebUINavigationThrottle&) = delete;
  ~UntrustedWebUINavigationThrottle() override;

  // NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillCommitWithoutUrlLoader() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult ProcessNavigation();
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_UNTRUSTED_WEB_UI_NAVIGATION_THROTTLE_H_
