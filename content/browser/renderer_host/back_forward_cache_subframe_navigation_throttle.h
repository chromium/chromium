// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_SUBFRAME_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_SUBFRAME_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

// BackForwardCacheSubframeNavigationThrottle defers subframe navigation before
// commit when the page that the subframe is in is BFCached. For now, this is
// only for WillCommitWithoutUrlLoader and WillStartRequest since currently, we
// allow no-URLLoader navigations that haven't yet reached the "pending commit"
// stage and navigations that use URLLoader that haven't sent network requests.
// We disallow any navigations in all other stages when the frame is in
// BFCache. This throttle resumes the deferred navigation when a page which
// contains the subframe is restored from the BFCache. Find more details in
// https://crbug.com/1511153.
class BackForwardCacheSubframeNavigationThrottle : public NavigationThrottle,
                                                   public WebContentsObserver {
 public:
  ~BackForwardCacheSubframeNavigationThrottle() override;

  CONTENT_EXPORT static std::unique_ptr<
      BackForwardCacheSubframeNavigationThrottle>
  MaybeCreateThrottleFor(NavigationHandle* navigation_handle);

 private:
  explicit BackForwardCacheSubframeNavigationThrottle(
      NavigationHandle* navigation_handle);

  // NavigationThrottle
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillFailRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  ThrottleCheckResult WillCommitWithoutUrlLoader() override;

  // WebContentsObserver
  void RenderFrameHostStateChanged(
      RenderFrameHost* render_frame_host,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) override;

  // Defer subframe navigation which hasn't reached "pending commit" stage nor
  // sent a network request if the frame is in BackForwardCache.
  NavigationThrottle::ThrottleCheckResult DeferNavigationIfNeeded();

  // Check that navigations that need URLLoader in bfcached page don't hit the
  // stages after sending network requests.
  void ConfirmNavigationIsNotInBFCachedFrame();

  bool is_deferred_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_SUBFRAME_NAVIGATION_THROTTLE_H_
