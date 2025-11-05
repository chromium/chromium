// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_viewport_observer.h"

#include <map>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "url/origin.h"

namespace content {
namespace {

using FrameVisibility = blink::mojom::FrameVisibility;

net::SchemefulSite GetSchemefulSiteFromPossiblyOpaqueOrigin(
    const url::Origin& origin) {
  const url::SchemeHostPort& scheme_host_port =
      origin.GetTupleOrPrecursorTupleIfOpaque();
  if (!scheme_host_port.IsValid()) {
    return net::SchemefulSite();
  }

  return net::SchemefulSite(url::Origin::CreateFromNormalizedTuple(
      scheme_host_port.scheme(), scheme_host_port.host(),
      scheme_host_port.port()));
}

// If this navigation will result in an error page, or it already has, then
// we need to track the pre-navigation site of the frame. If a fenced frame
// attempts to navigate itself to an error page on purpose, we'd need to track
// the site that was in the frame before the error page (which has an
// opaque origin, so we can't construct its net::SchemefulSite directly).
// We're choosing the pre-navigation site instead of the initiator site because
// the initiator might be the fenced frame's embedder, rather than the document
// that is already in the frame.
bool ShouldTrackPreviousSiteForNavigation(NavigationHandle* navigation_handle) {
  if (navigation_handle->GetNetErrorCode() != net::Error::OK) {
    return true;
  }

  // For extra safety, check the error page status of committed navigations.
  if (navigation_handle->HasCommitted() && navigation_handle->IsErrorPage()) {
    return true;
  }

  return false;
}

// Determines the site that a frame will be tracked under for viewport metrics.
//
// IMPORTANT: This will not always return the same site that the navigation
// intends to commit or has committed. In cases like error pages we need to take
// special precautions to ensure the site that was previously committed in the
// frame was tracked instead.
net::SchemefulSite GetSiteForViewportMetricsTracking(
    const FencedFrameViewportMonitor::FencedFrameVisibilityInfo& info,
    NavigationHandle* navigation_handle) {
  if (ShouldTrackPreviousSiteForNavigation(navigation_handle)) {
    const url::Origin& last_successful_origin =
        static_cast<NavigationRequest*>(navigation_handle)
            ->frame_tree_node()
            ->last_successful_origin();

    return GetSchemefulSiteFromPossiblyOpaqueOrigin(last_successful_origin);
  }

  const url::Origin& origin_to_track =
      navigation_handle->GetRenderFrameHost()->GetLastCommittedOrigin();

  // If the origin whose site we're planning to track is opaque, then we want
  // the site of the precursor origin instead, to determine the actual eTLD+1
  // that the navigation was intended for, independent of the origin's
  // actual opaqueness. The origin may not have a precursor, which will result
  // in returning an opaque SchemefulSite. This isn't ideal, but it's the only
  // option if we don't have enough information to proceed.
  return GetSchemefulSiteFromPossiblyOpaqueOrigin(origin_to_track);
}

}  // namespace

FencedFrameViewportObserver::FencedFrameViewportObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

FencedFrameViewportObserver::~FencedFrameViewportObserver() = default;

void FencedFrameViewportObserver::FrameDeleted(
    FrameTreeNodeId frame_tree_node_id) {
  // This is sketchy but safe to do, as the FrameTreeNode being destroyed still
  // technically exists at this point.
  FrameTreeNode* node = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!node || !node->IsFencedFrameRoot()) {
    return;
  }

  auto* monitor = PageUserData<FencedFrameViewportMonitor>::GetOrCreateForPage(
      node->GetParentOrOuterDocument()->GetOutermostMainFrame()->GetPage());
  if (monitor) {
    monitor->FrameDeleted(frame_tree_node_id);
  }
}

void FencedFrameViewportObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!(navigation_handle->IsInFencedFrameTree() &&
        navigation_handle->IsInMainFrame())) {
    return;
  }

  // If the navigation never committed (download, HTTP 204, etc), then there's
  // no site information to update. Same goes for same-document navigations.
  // Error page commits are allowed to proceed.
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  auto* monitor = PageUserData<FencedFrameViewportMonitor>::GetOrCreateForPage(
      navigation_handle->GetRenderFrameHost()
          ->GetOutermostMainFrame()
          ->GetPage());
  if (monitor) {
    monitor->DidFinishNavigation(navigation_handle);
  }
}

void FencedFrameViewportObserver::OnFrameVisibilityChanged(
    RenderFrameHost* rfh,
    FrameVisibility visibility) {
  if (!rfh->IsFencedFrameRoot()) {
    return;
  }
  auto* monitor = PageUserData<FencedFrameViewportMonitor>::GetOrCreateForPage(
      rfh->GetOutermostMainFrame()->GetPage());
  if (monitor) {
    auto* rfhi = static_cast<RenderFrameHostImpl*>(rfh);
    monitor->OnFrameVisibilityChanged(rfhi->GetFrameTreeNodeId(), visibility);
  }
}

PAGE_USER_DATA_KEY_IMPL(FencedFrameViewportMonitor);

// Ideally, we'd have a CHECK in place here that we only construct this object
// for the primary page. However, there are cases where the primary page isn't
// actually qualified as such yet, like prerendering (see Page::IsPrimary() for
// more info). For now, rely on the caller providing the page associated with
// the outermost main frame, which should result in the desired behavior.
FencedFrameViewportMonitor::FencedFrameViewportMonitor(Page& page)
    : PageUserData<FencedFrameViewportMonitor>(page) {}

FencedFrameViewportMonitor::~FencedFrameViewportMonitor() {
  LogUmaMetrics();
}

void FencedFrameViewportMonitor::FrameDeleted(
    FrameTreeNodeId frame_tree_node_id) {
  auto iter = visibility_infos_.find(frame_tree_node_id);
  if (iter == visibility_infos_.end()) {
    return;
  }

  // If this frame was in the viewport, then try to decrement the count for the
  // current site.
  if (iter->second.current_visibility == FrameVisibility::kRenderedInViewport) {
    DecrementFencedFrameViewportCountForSite(iter->second.site);
  }
  visibility_infos_.erase(iter);
}

void FencedFrameViewportMonitor::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // Get the FencedFrameVisibilityInfo for this frame, or default-construct one
  // if it doesn't exist, which may happen if the frame hasn't navigated or
  // entered the viewport yet.
  auto& info = visibility_infos_[navigation_handle->GetFrameTreeNodeId()];

  net::SchemefulSite site_to_track =
      GetSiteForViewportMetricsTracking(info, navigation_handle);

  // The frame has navigated to a new site while in the viewport. We need to
  // update the counts for both the old and new site. If the frame has navigated
  // while outside the viewport, or the navigation is same-site, there's no
  // new metrics to log.
  if (info.current_visibility == FrameVisibility::kRenderedInViewport &&
      site_to_track != info.site) {
    IncrementFencedFrameViewportCountForSite(site_to_track);
    DecrementFencedFrameViewportCountForSite(info.site);
  }

  info.site = site_to_track;
}

void FencedFrameViewportMonitor::OnFrameVisibilityChanged(
    FrameTreeNodeId frame_tree_node_id,
    FrameVisibility visibility) {
  // Get the FencedFrameVisibilityInfo for this frame, or default-construct one
  // if it doesn't exist, which may happen if the frame hasn't navigated or
  // entered the viewport yet.
  auto& info = visibility_infos_[frame_tree_node_id];

  // If the frame is entering the viewport, increment the count for the last
  // successful site. If the frame is leaving the viewport, decrement the
  // count instead.
  if (info.current_visibility != FrameVisibility::kRenderedInViewport &&
      visibility == FrameVisibility::kRenderedInViewport) {
    IncrementFencedFrameViewportCountForSite(info.site);
  } else if (info.current_visibility == FrameVisibility::kRenderedInViewport &&
             visibility != FrameVisibility::kRenderedInViewport) {
    DecrementFencedFrameViewportCountForSite(info.site);
  }

  info.current_visibility = visibility;
}

void FencedFrameViewportMonitor::
    ComputeSameSiteFencedFrameMaximumBeforePrimaryPageChange() {
  if (has_computed_unload_count_) {
    return;
  }

  auto max_iter =
      std::max_element(fenced_frames_in_viewport_per_site_.begin(),
                       fenced_frames_in_viewport_per_site_.end(),
                       [](const std::pair<net::SchemefulSite, int>& a,
                          const std::pair<net::SchemefulSite, int>& b) {
                         return a.second < b.second;
                       });
  if (max_iter != fenced_frames_in_viewport_per_site_.end()) {
    max_same_site_fenced_frames_in_viewport_at_unload_count_ = max_iter->second;
  }

  has_computed_unload_count_ = true;
}

void FencedFrameViewportMonitor::OnPrimaryPageEnteringBFCache() {
  // Normally, we'd log UMA metrics when this object is destroyed, such as when
  // the primary main frame changes after a navigation, or the WebContents is
  // torn down. However, when a Page enters BackForwardCache, it's not
  // destroyed, even though the primary main frame is changing. So, we need to
  // log UMA metrics now.
  LogUmaMetrics();

  // Now that we've logged the previous round of metrics, we need to initialize
  // the counters to the correct values for when this page is restored from
  // BackForwardCache. When the page is restored, we'll already know the max
  // number of same-site fenced frames in the viewport: it's the same as when
  // the page entered BackForwardCache in the first place! NOTE: We don't need
  // to re-initialize any of the per-frame data structures here, because no
  // notifications fire that update individual frame state when entering or
  // leaving BackForwardCache.
  max_same_site_fenced_frames_in_viewport_count_ =
      max_same_site_fenced_frames_in_viewport_at_unload_count_;

  // Also, allow the unload count to be computed again when the restored page
  // eventually unloads or re-enters BackForwardCache.
  has_computed_unload_count_ = false;
}

void FencedFrameViewportMonitor::LogUmaMetrics() {
  // If we're not tracking any fenced frames, there's no reason to log any
  // metrics. We also log metrics before the Page enters BackForwardCache, so no
  // need to log again if we've already entered.
  if (fenced_frames_in_viewport_per_site_.empty() ||
      static_cast<RenderFrameHostImpl*>(&(page().GetMainDocument()))
          ->IsInBackForwardCache()) {
    return;
  }

  base::UmaHistogramExactLinear(
      blink::kMaxSameSiteFencedFramesInViewportPerPageLoad,
      max_same_site_fenced_frames_in_viewport_count_, 101);

  base::UmaHistogramExactLinear(
      blink::kMaxSameSiteFencedFramesInViewportAtUnload,
      max_same_site_fenced_frames_in_viewport_at_unload_count_, 101);
}

void FencedFrameViewportMonitor::IncrementFencedFrameViewportCountForSite(
    const net::SchemefulSite& site) {
  // If this line performs an insertion, site_count should be zero-initialized.
  int& site_count = fenced_frames_in_viewport_per_site_[site];
  site_count += 1;

  // If a site's count increases, check if it exceeds the max same-site count
  // we've seen for this primary page load.
  if (site_count > max_same_site_fenced_frames_in_viewport_count_) {
    max_same_site_fenced_frames_in_viewport_count_ = site_count;
  }
}

void FencedFrameViewportMonitor::DecrementFencedFrameViewportCountForSite(
    const net::SchemefulSite& site) {
  // If this line performs an insertion, site_count should be zero-initialized.
  int& site_count = fenced_frames_in_viewport_per_site_[site];

  CHECK_GE(site_count, 0);
  if (site_count > 0) {
    site_count -= 1;
  }
}

}  // namespace content
