// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {

class PrerenderHost;
enum class PrerenderFinalStatus;

// PrerenderNavigationThrottle applies restrictions to prerendering navigation
// on the main frame. Specifically this cancels prerendering in the following
// cases.
//
// - Cross-origin prerendering
// - Cross-origin redirection during prerendering
// - Cross-origin navigation from a prerendered page
class PrerenderNavigationThrottle : public NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(NavigationThrottleRegistry& registry);

  ~PrerenderNavigationThrottle() override;

  // NavigationThrottle
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;

 private:
  explicit PrerenderNavigationThrottle(NavigationThrottleRegistry& registry);

  ThrottleCheckResult WillStartOrRedirectRequest(bool is_redirection);

  // Returns true if this throttle is for prerender initial navigation.
  bool IsInitialNavigation() const;

  // Cancels prerendering hosting this navigation with `final_status`.
  void CancelPrerendering(PrerenderFinalStatus final_status);

  const base::WeakPtr<PrerenderHost> prerender_host_ = nullptr;

  bool is_same_site_cross_origin_prerender_ = false;
  bool same_site_cross_origin_prerender_did_redirect_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NAVIGATION_THROTTLE_H_
