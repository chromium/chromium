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
    : NavigationThrottle(navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  FrameTreeNode* ftn = navigation_request->frame_tree_node();
  PrerenderHostRegistry* prerender_host_registry =
      ftn->current_frame_host()->delegate()->GetPrerenderHostRegistry();
  int ftn_id = ftn->frame_tree_node_id();
  PrerenderHost* prerender_host =
      prerender_host_registry->FindNonReservedHostById(ftn_id);
  if (!prerender_host) {
    prerender_host = prerender_host_registry->FindReservedHostById(ftn_id);
  }
  DCHECK(prerender_host);

  // This throttle is responsible for setting the initial navigation id on the
  // PrerenderHost, since the PrerenderHost obtains the NavigationRequest,
  // which has the ID, only after the navigation throttles run.
  if (prerender_host->GetInitialNavigationId().has_value()) {
    // If the host already has an initial navigation id, this throttle
    // will later cancel the navigation in Will*Request(). Just do nothing
    // until then.
  } else {
    prerender_host->SetInitialNavigationId(
        navigation_handle->GetNavigationId());
  }
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillStartOrRedirectRequest(bool is_redirection) {
  DCHECK(blink::features::IsPrerender2Enabled());

  // Take the root frame tree node of the prerendering page.
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  DCHECK(frame_tree_node->IsMainFrame());
  DCHECK(frame_tree_node->frame_tree()->is_prerendering());

  // Get the prerender host of the prerendering page. It might be a reserved
  // host if activation already started.
  PrerenderHostRegistry* prerender_host_registry =
      frame_tree_node->current_frame_host()
          ->delegate()
          ->GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      prerender_host_registry->FindNonReservedHostById(
          frame_tree_node->frame_tree_node_id());
  bool activation_started = false;
  if (!prerender_host) {
    prerender_host = prerender_host_registry->FindReservedHostById(
        frame_tree_node->frame_tree_node_id());
    activation_started = true;
  }
  DCHECK(prerender_host);

  // `activation_started` determines whether prerendering can be cancelled.
  // If activation already started, we cannot safely cancel prerendering, but
  // we still block the navigation to preserve the restrictions that this
  // throttle is intended to impose.
  // TODO(https://crbug.com/1198395): Cancel prerendering even after
  // activation started when support is added to do so.

  // Navigations after the initial prerendering navigation are disallowed.
  if (*prerender_host->GetInitialNavigationId() !=
      navigation_request->GetNavigationId()) {
    if (!activation_started) {
      prerender_host_registry->CancelHost(
          frame_tree_node->frame_tree_node_id(),
          PrerenderHost::FinalStatus::kMainFrameNavigation);
    }
    return CANCEL;
  }

  // Allow only HTTP(S) schemes.
  // https://jeremyroman.github.io/alternate-loading-modes/#no-bad-navs
  GURL prerendering_url = navigation_handle()->GetURL();
  if (!prerendering_url.SchemeIsHTTPOrHTTPS()) {
    if (!activation_started) {
      prerender_host_registry->CancelHost(
          frame_tree_node->frame_tree_node_id(),
          is_redirection
              ? PrerenderHost::FinalStatus::kInvalidSchemeRedirect
              : PrerenderHost::FinalStatus::kInvalidSchemeNavigation);
    }
    return CANCEL;
  }

  // Cancel prerendering if this is cross-origin prerendering, cross-origin
  // redirection during prerendering, or cross-origin navigation from a
  // prerendered page.
  // TODO(https://crbug.com/1176120): Fallback to NoStatePrefetch.
  url::Origin prerendering_origin = url::Origin::Create(prerendering_url);
  if (prerendering_origin != prerender_host->initiator_origin()) {
    if (!activation_started) {
      prerender_host_registry->CancelHost(
          frame_tree_node->frame_tree_node_id(),
          is_redirection ? PrerenderHost::FinalStatus::kCrossOriginRedirect
                         : PrerenderHost::FinalStatus::kCrossOriginNavigation);
    }
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

    prerender_host_registry->CancelHost(frame_tree_node->frame_tree_node_id(),
                                        PrerenderHost::FinalStatus::kDownload);
    return CANCEL;
  }
  return PROCEED;
}

}  // namespace content
