// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_SUBFRAME_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_SUBFRAME_NAVIGATION_THROTTLE_H_

#include "base/scoped_observation.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

class FrameTreeNode;

// PrerenderSubframeNavigationThrottle defers cross-origin subframe loading
// during the main frame is in a prerendered state.
class PrerenderSubframeNavigationThrottle : public NavigationThrottle,
                                            public PrerenderHost::Observer,
                                            public WebContentsObserver {
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
  ThrottleCheckResult WillProcessResponse() override;

  // PrerenderHost::Observer
  void OnActivated() override;
  void OnHostDestroyed() override;

  // WebContentsObserver:
  void DidFinishNavigation(NavigationHandle* nav_handle) override;

  ThrottleCheckResult WillStartOrRedirectRequest();

  // Called when this throttle defers a navigation. Observes the PrerenderHost
  // so that the throttle can resume the navigation upon activation.
  void DeferCrossOriginSubframeNavigation(const FrameTreeNode& frame_tree_node);

  bool is_deferred_ = false;
  const int prerender_root_ftn_id_;
  base::ScopedObservation<PrerenderHost, PrerenderHost::Observer> observation_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_SUBFRAME_NAVIGATION_THROTTLE_H_
