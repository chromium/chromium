// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_SUBFRAME_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_SUBFRAME_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

// BackForwardCacheSubframeNavigationThrottle defers subframe navigation at
// every possible stage of navigation before commit when the page that the
// subframe is in is BFCached. For now, this is only for
// WillCommitWithoutUrlLoader since currently, we allow no-url-loader
// navigations that haven't yet reached the "pending commit" stage, but disallow
// any navigations with URLLoaders. This throttle resumes the deferred
// navigation when a page which contains the subframe is restored from the
// BFCache. Find more details in https://crbug.com/1511153 See the design doc:
// https://docs.google.com/document/d/1XLOQuHjCVmBfXAJhgrASkVyrHAK_DGmcYPyrTnOrSPM/edit?usp=sharing&resourcekey=0-uNz75Ux7INdCLhj2FWVILg
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
  ThrottleCheckResult WillCommitWithoutUrlLoader() override;

  // WebContentsObserver
  void RenderFrameHostStateChanged(
      RenderFrameHost* render_frame_host,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) override;

  bool is_deferred_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_SUBFRAME_NAVIGATION_THROTTLE_H_
