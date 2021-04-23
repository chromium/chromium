// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_subframe_navigation_throttle.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {

// static
std::unique_ptr<PrerenderSubframeNavigationThrottle>
PrerenderSubframeNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  if (!blink::features::IsPrerender2Enabled() ||
      frame_tree_node->IsMainFrame() ||
      !frame_tree_node->frame_tree()->is_prerendering()) {
    return nullptr;
  }

  return base::WrapUnique(
      new PrerenderSubframeNavigationThrottle(navigation_handle));
}

PrerenderSubframeNavigationThrottle::PrerenderSubframeNavigationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

PrerenderSubframeNavigationThrottle::~PrerenderSubframeNavigationThrottle() =
    default;

const char* PrerenderSubframeNavigationThrottle::GetNameForLogging() {
  return "PrerenderSubframeNavigationThrottle";
}

NavigationThrottle::ThrottleCheckResult
PrerenderSubframeNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest();
}

NavigationThrottle::ThrottleCheckResult
PrerenderSubframeNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest();
}

void PrerenderSubframeNavigationThrottle::OnActivated() {
  DCHECK(!NavigationRequest::From(navigation_handle())
              ->frame_tree_node()
              ->frame_tree()
              ->is_prerendering());
  if (is_deferred_) {
    is_deferred_ = false;
    // May delete `this`.
    Resume();
  }
}

void PrerenderSubframeNavigationThrottle::OnHostDestroyed() {
  observation_.Reset();
}

NavigationThrottle::ThrottleCheckResult
PrerenderSubframeNavigationThrottle::WillStartOrRedirectRequest() {
  DCHECK(blink::features::IsPrerender2Enabled());

  auto* navigation_request = NavigationRequest::From(navigation_handle());
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  DCHECK(!frame_tree_node->IsMainFrame());

  // Proceed if the page isn't in the prerendering state.
  if (!frame_tree_node->frame_tree()->is_prerendering())
    return NavigationThrottle::PROCEED;

  // Proceed for the same origin subframe navigation.
  RenderFrameHostImpl* rfhi = frame_tree_node->frame_tree()->GetMainFrame();
  const url::Origin& main_origin = rfhi->GetLastCommittedOrigin();
  if (main_origin.IsSameOriginWith(
          url::Origin::Create(navigation_handle()->GetURL()))) {
    return NavigationThrottle::PROCEED;
  }

  // Look up the PrerenderHost.
  PrerenderHostRegistry* registry = frame_tree_node->current_frame_host()
                                        ->delegate()
                                        ->GetPrerenderHostRegistry();
  int id = frame_tree_node->frame_tree()->GetMainFrame()->GetFrameTreeNodeId();
  PrerenderHost* prerender_host = registry->FindNonReservedHostById(id);
  if (!prerender_host) {
    // The host might be reserved already for activation. In either case, we
    // defer until activation for simplicity.
    prerender_host = registry->FindReservedHostById(id);
  }
  DCHECK(prerender_host);

  // Defer cross-origin subframe navigations during prerendering.
  // Will resume the navigation upon activation.
  if (!observation_.IsObserving())
    observation_.Observe(prerender_host);
  DCHECK(observation_.IsObservingSource(prerender_host));
  is_deferred_ = true;
  return NavigationThrottle::DEFER;
}

}  // namespace content
