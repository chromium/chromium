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
  // Disallow downloads during prerendering and cancel the prerender.
  if (navigation_handle()->IsDownload() &&
      frame_tree_node->frame_tree()->is_prerendering()) {
    PrerenderHostRegistry* prerender_host_registry =
        frame_tree_node->current_frame_host()
            ->delegate()
            ->GetPrerenderHostRegistry();

    prerender_host_registry->CancelHost(
        frame_tree_node->frame_tree()->root()->frame_tree_node_id(),
        PrerenderHost::FinalStatus::kDownload);
    return CANCEL;
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

  // If the finished navigation did not commit, do not Resume(). We expect that
  // the prerendered page and therefore the subframe navigation will eventually
  // be cancelled.
  if (!finished_navigation->HasCommitted())
    return;

  // The activation is finished. There is no need to listen to the WebContents
  // anymore.
  Observe(nullptr);

  // Resume the subframe navigation.
  if (!is_deferred_)
    return;
  is_deferred_ = false;
  Resume();
  // Resume() may have deleted `this`.
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

  // Proceed for same-origin subframe navigation.
  // TODO(https://crbug.com/1229027): url::Origin::Create() might not be
  // completely accurate for cases like sandboxed flags (while about:blank and
  // srcdoc are OK because NavigationThrottles do not run for those). We may
  // also need to defer at response time by using GetOriginToCommit().
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
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_root_ftn_id_);
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
