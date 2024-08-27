// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_subframe_navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "url/origin.h"

namespace content {

// static
std::unique_ptr<PrerenderSubframeNavigationThrottle>
PrerenderSubframeNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  if (frame_tree_node->IsMainFrame() ||
      !frame_tree_node->frame_tree().is_prerendering()) {
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
                                 .root()
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
  if (!frame_tree_node->frame_tree().is_prerendering())
    return NavigationThrottle::PROCEED;

  // TODO(crbug.com/40222993): Delay until activation instead of cancellation.
  if (navigation_handle()->IsDownload()) {
    // Disallow downloads during prerendering and cancel the prerender.
    PrerenderHostRegistry* prerender_host_registry =
        frame_tree_node->current_frame_host()
            ->delegate()
            ->GetPrerenderHostRegistry();
    prerender_host_registry->CancelHost(
        frame_tree_node->frame_tree().root()->frame_tree_node_id(),
        PrerenderFinalStatus::kDownload);
    return CANCEL;
  }

  // Don't run cross-origin subframe navigation check for non-renderable
  // contents like 204/205 as their GetOriginToCommit() is invalid. In this
  // case, we can safely proceed with navigation without deferring it.
  if (!navigation_request->response_should_be_rendered())
    return PROCEED;

  // Defer cross-origin subframe navigation until page activation. The check is
  // added here, because this is the first place that the throttle can properly
  // check for cross-origin using GetOriginToCommit(). See comments in
  // WillStartOrRedirectRequest() for more details.
  RenderFrameHostImpl* rfhi = frame_tree_node->frame_tree().GetMainFrame();
  const url::Origin& main_origin = rfhi->GetLastCommittedOrigin();
  if (!main_origin.IsSameOriginWith(
          navigation_request->GetOriginToCommit().value())) {
    return DeferOrCancelCrossOriginSubframeNavigation(*frame_tree_node);
  }

  return PROCEED;
}

void PrerenderSubframeNavigationThrottle::OnActivated() {
  CHECK(!NavigationRequest::From(navigation_handle())
             ->frame_tree_node()
             ->frame_tree()
             .is_prerendering());
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

NavigationThrottle::ThrottleCheckResult
PrerenderSubframeNavigationThrottle::WillCommitWithoutUrlLoader() {
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  if (navigation_request->GetUrlInfo().is_sandboxed) {
    FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
    // Although main frames can be in sandboxed SiteInfo's, we don't encounter
    // that here since this throttle check should never occur for a mainframe.
    CHECK(!frame_tree_node->IsMainFrame());
    return DeferOrCancelCrossOriginSubframeNavigation(*frame_tree_node);
  }

  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
PrerenderSubframeNavigationThrottle::DeferOrCancelCrossOriginSubframeNavigation(
    const FrameTreeNode& frame_tree_node) {
  CHECK(frame_tree_node.frame_tree().is_prerendering());
  CHECK(!frame_tree_node.IsMainFrame());

  // Look up the PrerenderHost.
  PrerenderHostRegistry* registry = frame_tree_node.current_frame_host()
                                        ->delegate()
                                        ->GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_root_ftn_id_);
  if (!prerender_host) {
    // The PrerenderHostRegistry removed the PrerenderHost and scheduled to
    // destroy it asynchronously.
    return NavigationThrottle::CANCEL;
  }

  // Defer cross-origin subframe navigations during prerendering.
  // Will resume the navigation upon activation.
  CHECK(!observation_.IsObserving());
  observation_.Observe(prerender_host);
  CHECK(observation_.IsObservingSource(prerender_host));
  is_deferred_ = true;
  return NavigationThrottle::DEFER;
}

void PrerenderSubframeNavigationThrottle::OnHostDestroyed(
    PrerenderFinalStatus final_status) {
  observation_.Reset();
}

NavigationThrottle::ThrottleCheckResult
PrerenderSubframeNavigationThrottle::WillStartOrRedirectRequest() {
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  CHECK(!frame_tree_node->IsMainFrame());

  // Proceed if the page isn't in the prerendering state.
  if (!frame_tree_node->frame_tree().is_prerendering())
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
  RenderFrameHostImpl* rfhi = frame_tree_node->frame_tree().GetMainFrame();
  const url::Origin& main_origin = rfhi->GetLastCommittedOrigin();
  if (!main_origin.IsSameOriginWith(navigation_handle()->GetURL()))
    return DeferOrCancelCrossOriginSubframeNavigation(*frame_tree_node);

  return NavigationThrottle::PROCEED;
}

}  // namespace content
