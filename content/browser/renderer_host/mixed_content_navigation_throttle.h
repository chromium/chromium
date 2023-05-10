// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MIXED_CONTENT_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MIXED_CONTENT_NAVIGATION_THROTTLE_H_

#include <set>

#include "base/gtest_prod_util.h"
#include "content/browser/renderer_host/mixed_content_checker.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// Responsible for browser-process-side mixed content security checks. It checks
// only for frame-level resource loads (aka navigation loads). Sub-resources
// fetches are checked in the renderer process by `blink::MixedContentChecker`.
// Changes to this class might need to be reflected on its renderer counterpart.
//
// This class handles frame-level resource loads that have certificate errors as
// well as mixed content. (Resources with certificate errors can be seen as a
// type of mixed content.) This can happen when a user has previously bypassed a
// certificate error for the same host as the resource.
//
// Current mixed content W3C draft that drives this implementation:
// https://w3c.github.io/webappsec-mixed-content/
class MixedContentNavigationThrottle : public NavigationThrottle {
 public:
  static std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
      NavigationHandle* navigation_handle);

  MixedContentNavigationThrottle(NavigationHandle* navigation_handle);

  MixedContentNavigationThrottle(const MixedContentNavigationThrottle&) =
      delete;
  MixedContentNavigationThrottle& operator=(
      const MixedContentNavigationThrottle&) = delete;

  ~MixedContentNavigationThrottle() override;

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  // Checks if the request has a certificate error that should adjust the page's
  // security UI, and does so if applicable.
  void MaybeHandleCertificateError();

  // Checks whether to block navigation loads and reports to renderer.
  MixedContentChecker checker_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MIXED_CONTENT_NAVIGATION_THROTTLE_H_
