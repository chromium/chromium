// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {

// PrerenderNavigationThrottle applies restrictions to prerendering navigation
// on the main frame. Specifically this cancels prerendering in the following
// cases.
//
// - Cross-origin prerendering
// - Cross-origin redirection during prerendering
// - Cross-origin navigation from a prerendered page
class PrerenderNavigationThrottle : public NavigationThrottle {
 public:
  ~PrerenderNavigationThrottle() override;

  static std::unique_ptr<PrerenderNavigationThrottle> MaybeCreateThrottleFor(
      NavigationHandle* navigation_handle);

  // NavigationThrottle
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;

 private:
  explicit PrerenderNavigationThrottle(NavigationHandle* navigation_handle);

  ThrottleCheckResult WillStartOrRedirectRequest(bool is_redirection);

  bool is_same_site_cross_origin_prerender_ = false;
  bool same_site_cross_origin_prerender_did_redirect_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NAVIGATION_THROTTLE_H_
