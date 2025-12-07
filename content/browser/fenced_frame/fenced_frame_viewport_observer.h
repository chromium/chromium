// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_VIEWPORT_OBSERVER_H_
#define CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_VIEWPORT_OBSERVER_H_

#include <map>
#include <unordered_set>

#include "base/types/optional_ref.h"
#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"

namespace content {

// This class tracks how many same-site fenced frames are visible in the
// viewport at once per primary main frame by updating the relevant metrics in
// the primary Page's FencedFrameViewportMonitor. Bound to the lifetime of the
// WebContents. Intended to monitor cases where multiple same-site fenced frames
// may collude to exfiltrate information via mouse click; for more info, see
// https://github.com/WICG/fenced-frame/blob/master/explainer/fenced_frames_with_local_unpartitioned_data_access.md#click-privacy-considerations
class FencedFrameViewportObserver : public WebContentsObserver {
 public:
  explicit FencedFrameViewportObserver(WebContents* web_contents);
  ~FencedFrameViewportObserver() override;

  FencedFrameViewportObserver(const FencedFrameViewportObserver&) = delete;
  FencedFrameViewportObserver(FencedFrameViewportObserver&&) = delete;
  FencedFrameViewportObserver& operator=(const FencedFrameViewportObserver&) =
      delete;
  FencedFrameViewportObserver& operator=(FencedFrameViewportObserver&&) =
      delete;

  // WebContentsObserver implementation.
  void FrameDeleted(FrameTreeNodeId frame_tree_node_id) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void OnFrameVisibilityChanged(
      RenderFrameHost* rfh,
      blink::mojom::FrameVisibility visibility) override;
};

// Monitors the number of same-site fenced frames that are rendered in the
// viewport, and logs relevant UMA metrics. Scoped to the lifetime of the
// primary Page. Metrics will be logged when the primary Page enters
// BackForwardCache or is destroyed (destroying this object as well).
class FencedFrameViewportMonitor
    : public PageUserData<FencedFrameViewportMonitor> {
 public:
  // Helper to hold visibility info for each fenced frame in the
  // `visibility_infos_` map below.
  struct FencedFrameVisibilityInfo {
    net::SchemefulSite site;
    blink::mojom::FrameVisibility current_visibility =
        blink::mojom::FrameVisibility::kNotRendered;
  };

  ~FencedFrameViewportMonitor() override;

  // These methods correspond to the `WebContentsObserver` methods of the same
  // name, but are called only by `FencedFrameViewportObserver` when the
  // corresponding method is invoked for a fenced frame root.
  void FrameDeleted(FrameTreeNodeId frame_tree_node_id);
  void DidFinishNavigation(NavigationHandle* navigation_handle);
  void OnFrameVisibilityChanged(FrameTreeNodeId frame_tree_node_id,
                                blink::mojom::FrameVisibility visibility);

  // Called immediately before sending a commit for the primary main frame, or
  // when the frame tree is being torn down. Either of these actions will cause
  // fenced frames to leave the viewport, so we need to compute a snapshot of
  // how many same-site fenced frames were in the viewport beforehand, which
  // will ensure accurate UMA metrics.
  void ComputeSameSiteFencedFrameMaximumBeforePrimaryPageChange();

  // Logs UMA metrics and resets this object's local state before entering
  // BackForwardCache, which ensures that it's restored into the proper state
  // after being activated again.
  void OnPrimaryPageEnteringBFCache();

 private:
  explicit FencedFrameViewportMonitor(Page& page);

  friend PageUserData<FencedFrameViewportMonitor>;
  PAGE_USER_DATA_KEY_DECL();

  // Called when this object is destroyed or enters BackForwardCache.
  void LogUmaMetrics();

  // Increments or decrements the count of fenced frames navigated to `site`
  // that are visible in the viewport.
  void IncrementFencedFrameViewportCountForSite(const net::SchemefulSite& site);
  void DecrementFencedFrameViewportCountForSite(const net::SchemefulSite& site);

  // Relevant site + visibility info for each fenced frame, identified by its
  // FrameTreeNode.
  std::map<FrameTreeNodeId, FencedFrameVisibilityInfo> visibility_infos_;

  // Total count of fenced frames in the viewport, for each site.
  std::map<net::SchemefulSite, int> fenced_frames_in_viewport_per_site_;

  // Tracks the maximum number of same-site fenced frames that were rendered in
  // the viewport over the lifetime of this page load. Logged via UMA when this
  // object is destroyed or enters BackForwardCache.
  int max_same_site_fenced_frames_in_viewport_count_ = 0;

  // Tracks the maximum number of same-site fenced frames that were rendered in
  // the viewport at the moment the primary main frame unloads.
  int max_same_site_fenced_frames_in_viewport_at_unload_count_ = 0;

  // There are multiple code paths that may try to unload all the fenced frames
  // on the page, and they may or may not overlap. This flag ensures there's
  // only one computation per primary Page, done as early as possible.
  bool has_computed_unload_count_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_VIEWPORT_OBSERVER_H_
