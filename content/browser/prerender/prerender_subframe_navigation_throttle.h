// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_SUBFRAME_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_SUBFRAME_NAVIGATION_THROTTLE_H_

#include "base/scoped_observation.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// PrerenderSubframeNavigationThrottle defers cross-origin subframe loading
// during the main frame is in a prerendered state.
class PrerenderSubframeNavigationThrottle : public NavigationThrottle,
                                            public PrerenderHost::Observer {
 public:
  ~PrerenderSubframeNavigationThrottle() override;

  static std::unique_ptr<PrerenderSubframeNavigationThrottle>
  MaybeCreateThrottleFor(NavigationHandle* navigation_handle);

 private:
  explicit PrerenderSubframeNavigationThrottle(
      NavigationHandle* navigation_handle);

  // NavigationThrottle
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

  // PrerenderHost::Observer
  void OnActivated() override;
  void OnHostDestroyed() override;

  ThrottleCheckResult WillStartOrRedirectRequest();

  base::ScopedObservation<PrerenderHost, PrerenderHost::Observer> observation_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_SUBFRAME_NAVIGATION_THROTTLE_H_
