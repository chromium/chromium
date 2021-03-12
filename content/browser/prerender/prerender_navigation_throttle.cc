// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_navigation_throttle.h"

#include "content/browser/prerender/prerender_host.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/storage_partition_impl.h"
#include "third_party/blink/public/common/features.h"

namespace content {

PrerenderNavigationThrottle::~PrerenderNavigationThrottle() = default;

// static
std::unique_ptr<PrerenderNavigationThrottle>
PrerenderNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  if (!blink::features::IsPrerender2Enabled() ||
      !frame_tree_node->IsMainFrame() ||
      !frame_tree_node->frame_tree()->is_prerendering()) {
    return nullptr;
  }

  return base::WrapUnique(new PrerenderNavigationThrottle(navigation_handle));
}

const char* PrerenderNavigationThrottle::GetNameForLogging() {
  return "PrerenderNavigationThrottle";
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest(/*is_redirection=*/false);
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest(/*is_redirection=*/true);
}

PrerenderNavigationThrottle::PrerenderNavigationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillStartOrRedirectRequest(bool is_redirection) {
  DCHECK(blink::features::IsPrerender2Enabled());

  // Take the root frame tree node of the prerendering page.
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  DCHECK(frame_tree_node->IsMainFrame());
  DCHECK(frame_tree_node->frame_tree()->is_prerendering());

  // Take the prerender host of the prerendering page.
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      frame_tree_node->current_frame_host()->GetStoragePartition());
  PrerenderHostRegistry* prerender_host_registry =
      storage_partition_impl->GetPrerenderHostRegistry();
  const PrerenderHost* prerender_host = prerender_host_registry->FindHostById(
      frame_tree_node->frame_tree_node_id());
  DCHECK(prerender_host);

  // TODO(https://crbug.com/1176132): Cancel if the request is not for http(s).

  // Cancel prerendering if this is cross-origin prerendering, cross-origin
  // redirection during prerendering, or cross-origin navigation from a
  // prerendered page.
  GURL prerendering_url = navigation_handle()->GetURL();
  url::Origin prerendering_origin = url::Origin::Create(prerendering_url);
  if (prerendering_origin != prerender_host->initiator_origin()) {
    // Asynchronously abandon the prerender host so that the navigation request
    // and the render frame tree indirectly owned by the prerender host can
    // outlive the current callstack.
    prerender_host_registry->AbandonHostAsync(
        frame_tree_node->frame_tree_node_id(),
        is_redirection ? PrerenderHost::FinalStatus::kCrossOriginRedirect
                       : PrerenderHost::FinalStatus::kCrossOriginNavigation);
    // TODO(https://crbug.com/1176120): Fallback to NoStatePrefetch.
    return CANCEL;
  }

  return PROCEED;
}

}  // namespace content
