// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_drag_security_info.h"

#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/drop_data.h"

namespace content {

namespace {

// True when `a` and `b` belong to different pages shown in the same view: an
// inner WebContents and its embedder (or a sibling), or an MPArch guest page.
bool IsDragBetweenGuestAndOtherPage(RenderWidgetHostImpl* a,
                                    RenderWidgetHostImpl* b) {
  if (WebContentsImpl::FromRenderWidgetHostImpl(a) !=
      WebContentsImpl::FromRenderWidgetHostImpl(b)) {
    return true;
  }
  FrameTree* a_tree = a->frame_tree();
  FrameTree* b_tree = b->frame_tree();
  return a_tree && b_tree && a_tree != b_tree &&
         (a_tree->is_guest() || b_tree->is_guest());
}

}  // namespace

WebContentsViewDragSecurityInfo::WebContentsViewDragSecurityInfo() = default;
WebContentsViewDragSecurityInfo::~WebContentsViewDragSecurityInfo() = default;

void WebContentsViewDragSecurityInfo::OnDragInitiated(
    RenderWidgetHostImpl* source_rwh,
    const DropData& drop_data) {
  did_initiate_ = true;
  source_rwh_ = source_rwh->GetWeakPtr();
  site_instance_group_id_ = source_rwh->GetSiteInstanceGroup()->GetId();
  image_accessible_from_frame_ = drop_data.file_contents_image_accessible;
}

void WebContentsViewDragSecurityInfo::OnDragEnded() {
  did_initiate_ = false;
  source_rwh_ = nullptr;
  site_instance_group_id_ = SiteInstanceGroupId();
  image_accessible_from_frame_ = true;
}

bool WebContentsViewDragSecurityInfo::IsImageAccessibleFromFrame() const {
  // `is_initiated_` is false when the drag started outside of the browser or
  // from a different top-level WebContents. The drag is allowed if that is the
  // case.
  if (!did_initiate_) {
    return true;
  }

  return image_accessible_from_frame_;
}

// The browser-side check for https://crbug.com/59081 to block drags between
// cross-origin frames within the same page. Otherwise, a malicious attacker
// could abuse drag interactions to leak information across origins without
// explicit user intent.
bool WebContentsViewDragSecurityInfo::IsValidDragTarget(
    RenderWidgetHostImpl* target_rwh) const {
  // `is_initiated_` is false when the drag started outside of the browser or
  // from a different top-level WebContents. The drag is allowed if that is the
  // case.
  if (!did_initiate_) {
    return true;
  }

  // Guests route drags through their embedder's view, so a drag between two
  // pages can arrive here with `did_initiate_` set. Treat it like a drag
  // between two tabs.
  RenderWidgetHostImpl* source_rwh = source_rwh_.get();
  if (source_rwh && IsDragBetweenGuestAndOtherPage(source_rwh, target_rwh)) {
    return true;
  }

  // For site isolation, it is desirable to avoid having the renderer
  // perform the check unless it already has access to the starting
  // document's origin. If the SiteInstanceGroups match, then the process
  // allocation policy decided that it is OK for the source and target
  // frames to live in the same renderer process. Furthermore, having matching
  // SiteInstanceGroups means that either (1) the source and target frame are
  // part of the same blink::Page, or (2) that they are in the same Browsing
  // Context Group and the drag would cross tab boundaries (the latter of which
  // can't happen here since `is_initiated_` is true). Allow this drag to
  // the renderer. Blink will perform an additional check against
  // `blink::DragController::drag_initiator_` to decide whether or not to
  // allow the drag operation. This can be done in the renderer, as the
  // browser-side checks only have local tree fragment (potentially with
  // multiple origins) granularity at best, but a drag operation eventually
  // targets one single frame in that local tree fragment.
  return target_rwh->GetSiteInstanceGroup()->GetId() == site_instance_group_id_;
}

}  // namespace content
