// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_subframe_navigation_throttle.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/prerender/prerender_navigation_utils.h"
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
    NavigationHandle* nav_handle)
    : NavigationThrottle(nav_handle),
      prerender_root_ftn_id_(NavigationRequest::From(nav_handle)
                                 ->frame_tree_node()
                                 ->frame_tree()
                                 ->root()
                                 ->frame_tree_node_id()) {}

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

NavigationThrottle::ThrottleCheckResult
PrerenderSubframeNavigationThrottle::WillProcessResponse() {
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  absl::optional<PrerenderHost::FinalStatus> cancel_reason;

  if (!frame_tree_node->frame_tree()->is_prerendering())
    return NavigationThrottle::PROCEED;

  // Disallow downloads during prerendering and cancel the prerender.
  if (navigation_handle()->IsDownload()) {
    cancel_reason = PrerenderHost::FinalStatus::kDownload;
  } else if (prerender_navigation_utils::IsDisallowedHttpResponseCode(
                 navigation_request->commit_params().http_response_code)) {
    cancel_reason = PrerenderHost::FinalStatus::kNavigationBadHttpStatus;
  }

  if (cancel_reason.has_value()) {
    PrerenderHostRegistry* prerender_host_registry =
        frame_tree_node->current_frame_host()
            ->delegate()
            ->GetPrerenderHostRegistry();

    prerender_host_registry->CancelHost(
        frame_tree_node->frame_tree()->root()->frame_tree_node_id(),
        cancel_reason.value());
    return CANCEL;
  }

  // Defer cross-origin subframe navigation until page activation. The check is
  // added here, because this is the first place that the throttle can properly
  // check for cross-origin using GetOriginToCommit(). See comments in
  // WillStartOrRedirectRequest() for more details.
  RenderFrameHostImpl* rfhi = frame_tree_node->frame_tree()->GetMainFrame();
  const url::Origin& main_origin = rfhi->GetLastCommittedOrigin();
  if (!main_origin.IsSameOriginWith(navigation_request->GetOriginToCommit())) {
    DeferCrossOriginSubframeNavigation(*frame_tree_node);
    return NavigationThrottle::DEFER;
  }

  return PROCEED;
}

void PrerenderSubframeNavigationThrottle::OnActivated() {
  DCHECK(!NavigationRequest::From(navigation_handle())
              ->frame_tree_node()
              ->frame_tree()
              ->is_prerendering());
  // OnActivated() is called right before activation navigation commit which is
  // a little early. We want to resume the subframe navigation after the
  // PageBroadcast ActivatePrerenderedPage IPC is sent, to
  // guarantee that the new document starts in the non-prerendered state and
  // does not get a prerenderingchange event.
  //
  // Listen to the WebContents to wait for the activation navigation to finish
  // before resuming the subframe navigation.
  Observe(navigation_handle()->GetWebContents());
}

// Use DidFinishNavigation() rather than PrimaryPageChanged() in order to
// Resume() after the PageBroadcast Activate IPC is sent, which happens a
// little after PrimaryPageChanged() and before DidFinishNavigation(). This
// guarantees the new document starts in non-prerendered state.
void PrerenderSubframeNavigationThrottle::DidFinishNavigation(
    NavigationHandle* nav_handle) {
  // Ignore finished navigations that are not the activation navigation for the
  // prerendering frame tree that this subframe navigation started in.
  auto* finished_navigation = NavigationRequest::From(nav_handle);
  if (finished_navigation->prerender_frame_tree_node_id() !=
      prerender_root_ftn_id_)
    return;

  // The activation is finished. There is no need to listen to the WebContents
  // anymore.
  Observe(nullptr);

  // If the finished navigation did not commit, do not Resume(). We expect that
  // the prerendered page and therefore the subframe navigation will eventually
  // be cancelled.
  if (!finished_navigation->HasCommitted())
    return;

  // Resume the subframe navigation.
  if (!is_deferred_)
    return;
  is_deferred_ = false;
  Resume();
  // Resume() may have deleted `this`.
}

void PrerenderSubframeNavigationThrottle::DeferCrossOriginSubframeNavigation(
    const FrameTreeNode& frame_tree_node) {
  // Look up the PrerenderHost.
  DCHECK(!frame_tree_node.IsMainFrame());
  PrerenderHostRegistry* registry = frame_tree_node.current_frame_host()
                                        ->delegate()
                                        ->GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_root_ftn_id_);
  DCHECK(prerender_host);

  // Defer cross-origin subframe navigations during prerendering.
  // Will resume the navigation upon activation.
  DCHECK(!observation_.IsObserving());
  observation_.Observe(prerender_host);
  DCHECK(observation_.IsObservingSource(prerender_host));
  is_deferred_ = true;
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

  // Defer cross-origin subframe navigation until page activation.
  // Using url::Origin::Create() to check same-origin might not be
  // completely accurate for cases such as sandboxed iframes, which have a
  // different origin from the main frame even when the URL is same-origin.
  // There is another check in WillProcessResponse to fix this issue.
  // In WillProcessResponse, GetOriginToCommit is used to identify the
  // accurate Origin.
  // Note: about:blank and about:srcdoc also might not result in an appropriate
  // origin if we create the origin from the URL, but those cases won't go
  // through the NavigationThrottle, so it's not a problem here
  RenderFrameHostImpl* rfhi = frame_tree_node->frame_tree()->GetMainFrame();
  const url::Origin& main_origin = rfhi->GetLastCommittedOrigin();
  if (!main_origin.IsSameOriginWith(navigation_handle()->GetURL())) {
    DeferCrossOriginSubframeNavigation(*frame_tree_node);
    return NavigationThrottle::DEFER;
  }

  return NavigationThrottle::PROCEED;
}

}  // namespace content
