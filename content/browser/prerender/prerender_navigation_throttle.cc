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

  // Get the prerender host registry.
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      frame_tree_node->current_frame_host()->GetStoragePartition());
  PrerenderHostRegistry* prerender_host_registry =
      storage_partition_impl->GetPrerenderHostRegistry();

  // Disallow navigation from a prerendering page and cancel prerendering.
  RenderFrameHostImpl* initiator_render_frame_host_impl =
      navigation_request->GetInitiatorFrameToken().has_value()
          ? RenderFrameHostImpl::FromFrameToken(
                navigation_request->GetInitiatorProcessID(),
                navigation_request->GetInitiatorFrameToken().value())
          : nullptr;
  if (initiator_render_frame_host_impl &&
      initiator_render_frame_host_impl->frame_tree()->is_prerendering()) {
    prerender_host_registry->AbandonHostAsync(
        frame_tree_node->frame_tree_node_id(),
        PrerenderHost::FinalStatus::kMainFrameNavigation);
    // TODO(https://crbug.com/1194414): Handle the case the prerendering page
    // is reserved for activation, and AbandonHostAsync() could not do nothing
    // here.
    return CANCEL;
  }

  // Allow only HTTP(S) schemes.
  // https://jeremyroman.github.io/alternate-loading-modes/#no-bad-navs
  GURL prerendering_url = navigation_handle()->GetURL();
  if (!prerendering_url.SchemeIsHTTPOrHTTPS()) {
    prerender_host_registry->AbandonHostAsync(
        frame_tree_node->frame_tree_node_id(),
        is_redirection ? PrerenderHost::FinalStatus::kInvalidSchemeRedirect
                       : PrerenderHost::FinalStatus::kInvalidSchemeNavigation);
    return CANCEL;
  }

  // Take the prerender host of the prerendering page.
  const PrerenderHost* prerender_host = prerender_host_registry->FindHostById(
      frame_tree_node->frame_tree_node_id());
  DCHECK(prerender_host);

  // Cancel prerendering if this is cross-origin prerendering, cross-origin
  // redirection during prerendering, or cross-origin navigation from a
  // prerendered page.
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
