// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_SUBFRAME_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_SUBFRAME_NAVIGATION_THROTTLE_H_

#include "base/scoped_observation.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class FrameTreeNode;

// PrerenderSubframeNavigationThrottle defers cross-origin subframe loading
// during the main frame is in a prerendered state unless the main frame
// has `Supports-Loading-Mode: prerender-cross-origin-frames` header.
class PrerenderSubframeNavigationThrottle : public NavigationThrottle,
                                            public PrerenderHost::Observer,
                                            public WebContentsObserver {
 public:
  ~PrerenderSubframeNavigationThrottle() override;

  static void MaybeCreateAndAdd(NavigationThrottleRegistry& registry);

 private:
  explicit PrerenderSubframeNavigationThrottle(
          NavigationThrottleRegistry& registry);

  // NavigationThrottle
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  ThrottleCheckResult WillCommitWithoutUrlLoader() override;

  // PrerenderHost::Observer
  void OnActivated() override;
  void OnHostDestroyed(PrerenderFinalStatus final_status) override;

  // WebContentsObserver:
  void DidFinishNavigation(NavigationHandle* nav_handle) override;

  ThrottleCheckResult WillStartOrRedirectRequest();

  ThrottleCheckResult DecidePolicyForCrossOriginSubframeNavigation(
      const FrameTreeNode& frame_tree_node);

  bool is_deferred_ = false;
  const FrameTreeNodeId prerender_root_ftn_id_;
  base::ScopedObservation<PrerenderHost, PrerenderHost::Observer> observation_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_SUBFRAME_NAVIGATION_THROTTLE_H_
