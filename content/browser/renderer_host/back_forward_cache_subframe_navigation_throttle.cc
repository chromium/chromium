// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_subframe_navigation_throttle.h"

#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"

namespace content {

// static
std::unique_ptr<BackForwardCacheSubframeNavigationThrottle>
BackForwardCacheSubframeNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* navigation_handle) {
  CHECK(navigation_handle);
  // While the NavigationRequest is ongoing (and the throttles are already
  // registered),  the page might move into BFCache, so we check BFCache status
  // later in order to defer navigations in those cases.
  if (!navigation_handle->IsInMainFrame()) {
    return base::WrapUnique(
        new BackForwardCacheSubframeNavigationThrottle(navigation_handle));
  }
  return nullptr;
}

BackForwardCacheSubframeNavigationThrottle::
    BackForwardCacheSubframeNavigationThrottle(NavigationHandle* nav_handle)
    : NavigationThrottle(nav_handle),
      WebContentsObserver(nav_handle->GetWebContents()) {}

BackForwardCacheSubframeNavigationThrottle::
    ~BackForwardCacheSubframeNavigationThrottle() = default;

const char* BackForwardCacheSubframeNavigationThrottle::GetNameForLogging() {
  return "BackForwardCacheSubframeNavigationThrottle";
}

NavigationThrottle::ThrottleCheckResult
BackForwardCacheSubframeNavigationThrottle::DeferNavigationIfNeeded() {
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  // Defer this navigation if the frame is in BackForwardCache. Otherwise, we
  // enable it to proceed.
  if (frame_tree_node->current_frame_host()->GetLifecycleState() ==
      RenderFrameHost::LifecycleState::kInBackForwardCache) {
    is_deferred_ = true;
    return NavigationThrottle::DEFER;
  }
  return NavigationThrottle::PROCEED;
}

void BackForwardCacheSubframeNavigationThrottle::
    ConfirmNavigationIsNotInBFCachedFrame() {
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  // We don't bfcache pages with subframe navigations that have sent network
  // requests, so it's impossible for subframe navigations in bfcached pages to
  // reach `WillRedirectRequest`, `WillProcessResponse`, and `WillFailRequest`
  // while bfcached.
  CHECK_NE(frame_tree_node->current_frame_host()->GetLifecycleState(),
           RenderFrameHost::LifecycleState::kInBackForwardCache);
}

NavigationThrottle::ThrottleCheckResult
BackForwardCacheSubframeNavigationThrottle::WillStartRequest() {
  return DeferNavigationIfNeeded();
}

NavigationThrottle::ThrottleCheckResult
BackForwardCacheSubframeNavigationThrottle::WillRedirectRequest() {
  ConfirmNavigationIsNotInBFCachedFrame();
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
BackForwardCacheSubframeNavigationThrottle::WillFailRequest() {
  ConfirmNavigationIsNotInBFCachedFrame();
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
BackForwardCacheSubframeNavigationThrottle::WillProcessResponse() {
  ConfirmNavigationIsNotInBFCachedFrame();
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
BackForwardCacheSubframeNavigationThrottle::WillCommitWithoutUrlLoader() {
  return DeferNavigationIfNeeded();
}

void BackForwardCacheSubframeNavigationThrottle::RenderFrameHostStateChanged(
    RenderFrameHost* render_frame_host,
    RenderFrameHost::LifecycleState old_state,
    RenderFrameHost::LifecycleState new_state) {
  CHECK(render_frame_host);
  // Resume the deferred navigation when the `render_frame_host` is activated
  // from BackForwardCache. We check the frame tree node so that we won't resume
  // unrelated navigations.
  if (is_deferred_ &&
      NavigationRequest::From(navigation_handle())->frame_tree_node() ==
          static_cast<RenderFrameHostImpl*>(render_frame_host)
              ->frame_tree_node() &&
      old_state == RenderFrameHost::LifecycleState::kInBackForwardCache &&
      new_state == RenderFrameHost::LifecycleState::kActive) {
    Resume();
  }
}

}  // namespace content
