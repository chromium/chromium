// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_navigation_throttle.h"

#include "content/browser/prerender/prerender_host.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "third_party/blink/public/common/features.h"

namespace content {

PrerenderNavigationThrottle::~PrerenderNavigationThrottle() = default;

// static
std::unique_ptr<PrerenderNavigationThrottle>
PrerenderNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  if (frame_tree_node->IsMainFrame() &&
      frame_tree_node->frame_tree()->is_prerendering()) {
    DCHECK(blink::features::IsPrerender2Enabled());
    return base::WrapUnique(new PrerenderNavigationThrottle(navigation_handle));
  }
  return nullptr;
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

  // Get the prerender host of the prerendering page.
  PrerenderHostRegistry* prerender_host_registry =
      frame_tree_node->current_frame_host()
          ->delegate()
          ->GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      prerender_host_registry->FindNonReservedHostById(
          frame_tree_node->frame_tree_node_id());
  if (!prerender_host) {
    // If there is no host, we are already reserved for activation. Just let the
    // navigation proceed, since abandoning prerendering now might break the
    // activation navigation and cancelling the request while continuing the
    // activation will break compatibility. We also cannot defer because the
    // activation machinery waits for the navigation to commit before
    // activating.
    // TODO(https://crbug.com/1198395): Somehow handle this, probably by
    // deferring after support is added to activate while the main frame is
    // still being navigated; or else cancelling prerendering.
    DCHECK(prerender_host_registry->FindReservedHostById(
        frame_tree_node->frame_tree_node_id()));
    return PROCEED;
  }

  // Navigation after the initial prerendering navigation are disallowed.
  absl::optional<int64_t> initial_navigation_id =
      prerender_host->GetInitialNavigationId();
  if (!initial_navigation_id.has_value()) {
    // If the PrerenderHost has no initial navigation ID yet, this must be the
    // initial one, so set it here. This throttle is responsible for setting it
    // since the PrerenderHost obtains the NavigationRequest, which has the ID,
    // only after the navigation throttles run.
    prerender_host->SetInitialNavigationId(
        navigation_request->GetNavigationId());
  } else if (*initial_navigation_id != navigation_request->GetNavigationId()) {
    // If this is not the initial prerendering navigation, cancel the navigation
    // and cancel prerendering. Same document navigation is exceptionally
    // allowed but we do nothing here as throttles don't run against the same
    // document navigation. It should just work.
    prerender_host_registry->AbandonHost(
        frame_tree_node->frame_tree_node_id(),
        PrerenderHost::FinalStatus::kMainFrameNavigation);
    return CANCEL;
  }

  // Allow only HTTP(S) schemes.
  // https://jeremyroman.github.io/alternate-loading-modes/#no-bad-navs
  GURL prerendering_url = navigation_handle()->GetURL();
  if (!prerendering_url.SchemeIsHTTPOrHTTPS()) {
    prerender_host_registry->AbandonHost(
        frame_tree_node->frame_tree_node_id(),
        is_redirection ? PrerenderHost::FinalStatus::kInvalidSchemeRedirect
                       : PrerenderHost::FinalStatus::kInvalidSchemeNavigation);
    return CANCEL;
  }

  // Cancel prerendering if this is cross-origin prerendering, cross-origin
  // redirection during prerendering, or cross-origin navigation from a
  // prerendered page.
  url::Origin prerendering_origin = url::Origin::Create(prerendering_url);
  if (prerendering_origin != prerender_host->initiator_origin()) {
    prerender_host_registry->AbandonHost(
        frame_tree_node->frame_tree_node_id(),
        is_redirection ? PrerenderHost::FinalStatus::kCrossOriginRedirect
                       : PrerenderHost::FinalStatus::kCrossOriginNavigation);
    // TODO(https://crbug.com/1176120): Fallback to NoStatePrefetch.
    return CANCEL;
  }

  return PROCEED;
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillProcessResponse() {
  // Disallow downloads during prerendering and cancel the prerender.
  if (navigation_handle()->IsDownload()) {
    auto* navigation_request = NavigationRequest::From(navigation_handle());
    FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
    DCHECK(frame_tree_node->frame_tree()->is_prerendering());

    PrerenderHostRegistry* prerender_host_registry =
        frame_tree_node->current_frame_host()
            ->delegate()
            ->GetPrerenderHostRegistry();

    prerender_host_registry->AbandonHost(frame_tree_node->frame_tree_node_id(),
                                         PrerenderHost::FinalStatus::kDownload);
    return CANCEL;
  }
  return PROCEED;
}

}  // namespace content
