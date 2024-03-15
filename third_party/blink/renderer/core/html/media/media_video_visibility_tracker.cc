// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_video_visibility_tracker.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

// Do a rect-based penetrating list hit test, with a callback that is executed
// at each node.
HitTestResult HitTestForOcclusionRatio(
    const HTMLVideoElement& video_element,
    const PhysicalRect& hit_rect,
    std::optional<HitTestRequest::HitNodeCb> hit_node_cb) {
  LocalFrame* frame = video_element.GetDocument().GetFrame();
  DCHECK(!frame->View()->NeedsLayout());
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kIgnorePointerEventsNone | HitTestRequest::kReadOnly |
      HitTestRequest::kIgnoreClipping |
      HitTestRequest::kIgnoreZeroOpacityObjects |
      HitTestRequest::kHitTestVisualOverflow | HitTestRequest::kListBased |
      HitTestRequest::kPenetratingList | HitTestRequest::kAvoidCache;
  HitTestLocation location(hit_rect);
  return frame->GetEventHandler().HitTestResultAtLocation(
      location, hit_type, video_element.GetLayoutObject(), true, hit_node_cb);
}

float ComputeArea(const PhysicalRect& rect) {
  return static_cast<float>(ToRoundedSize(rect.size).Area64());
}

bool HasEnoughVisibleAreaRemaining(float accumulated_area,
                                   const PhysicalRect& intersection_rect,
                                   float visibility_threshold) {
  return accumulated_area / ComputeArea(intersection_rect) <
         (1 - visibility_threshold);
}

float ComputeIntersectionArea(const Vector<PhysicalRect>& occluding_rects,
                              const PhysicalRect& target_rect,
                              float video_element_area) {
  float intersection_area = 0.0;

  for (const auto& rect : occluding_rects) {
    if (!target_rect.Intersects(rect)) {
      continue;
    }

    PhysicalRect intersecting_rect = target_rect;
    intersecting_rect.Intersect(rect);
    intersection_area += ComputeArea(intersecting_rect);

    if (intersection_area >= video_element_area) {
      return video_element_area;
    }
  }

  return intersection_area;
}

}  // anonymous namespace

MediaVideoVisibilityTracker::MediaVideoVisibilityTracker(
    HTMLVideoElement& video,
    float visibility_threshold,
    ReportVisibilityCb report_visibility_cb,
    base::TimeDelta hit_test_interval)
    : video_element_(video),
      visibility_threshold_(visibility_threshold),
      report_visibility_cb_(std::move(report_visibility_cb)),
      hit_test_interval_(hit_test_interval) {
  DCHECK(report_visibility_cb_);
  DCHECK(visibility_threshold_ > 0.0 && visibility_threshold_ <= 1.0)
      << "Invalid threshold: " << visibility_threshold_;
  DCHECK_GE(hit_test_interval_, kMinimumAllowedHitTestInterval);
}

MediaVideoVisibilityTracker::~MediaVideoVisibilityTracker() {
  DCHECK(!tracker_attached_to_document_);
}

void MediaVideoVisibilityTracker::Attach() {
  const auto& video_element = VideoElement();
  auto* document = &video_element.GetDocument();
  auto* document_view = document->View();

  if (tracker_attached_to_document_) {
    DCHECK_EQ(tracker_attached_to_document_, document);
    return;
  }

  if (!video_element.isConnected() || !document_view) {
    return;
  }

  document_view->RegisterForLifecycleNotifications(this);

  tracker_attached_to_document_ = document;
}

void MediaVideoVisibilityTracker::Detach() {
  if (!tracker_attached_to_document_) {
    return;
  }

  if (auto* view = tracker_attached_to_document_->View()) {
    view->UnregisterFromLifecycleNotifications(this);
  }

  tracker_attached_to_document_ = nullptr;
}

void MediaVideoVisibilityTracker::UpdateVisibilityTrackerState() {
  if (VideoElement().GetExecutionContext() && !VideoElement().paused()) {
    Attach();
  } else {
    Detach();
  }
}

void MediaVideoVisibilityTracker::ElementDidMoveToNewDocument() {
  Detach();
}

ListBasedHitTestBehavior MediaVideoVisibilityTracker::ComputeOcclusion(
    const Node& node) {
  if (node == VideoElement()) {
    return kStopHitTesting;
  }

  // Ignore nodes with a containing shadow root of type
  // ShadowRootType::kUserAgent (e.g Video Controls).
  if (node.IsInShadowTree() && node.ContainingShadowRoot() &&
      node.ContainingShadowRoot()->IsUserAgent()) {
    return kContinueHitTesting;
  }

  // Ignore nodes that are not opaque. We are only interested on evaluating
  // nodes that visually occlude the video, as seen by the user.
  if (!node.GetLayoutObject()->HasNonZeroEffectiveOpacity()) {
    return kContinueHitTesting;
  }

  // Only account for the intersection of |node_rect| with |intersection_rect_|.
  PhysicalRect node_rect = node.BoundingBox();
  node_rect.Intersect(intersection_rect_);

  // Accumulate the area covered by the current node.
  accumulated_area_ += ComputeArea(node_rect);

  // Subtract the area of the |node_rect| intersection with the existing
  // occluding rects, if any.
  float intersection_area =
      ComputeIntersectionArea(occluding_rects_, node_rect, video_element_area_);
  accumulated_area_ -= intersection_area;
  occluding_rects_.push_back(node_rect);

  if (HasEnoughVisibleAreaRemaining(accumulated_area_, intersection_rect_,
                                    visibility_threshold_)) {
    return kContinueHitTesting;
  }

  return kStopHitTesting;
}

bool MediaVideoVisibilityTracker::MeetsVisibilityThreshold(
    const PhysicalRect& rect) {
  HitTestResult result(HitTestForOcclusionRatio(
      VideoElement(), rect,
      WTF::BindRepeating(&MediaVideoVisibilityTracker::ComputeOcclusion,
                         WrapPersistent(this))));

  return HasEnoughVisibleAreaRemaining(accumulated_area_, intersection_rect_,
                                       visibility_threshold_)
             ? true
             : false;
}

void MediaVideoVisibilityTracker::OnIntersectionChanged() {
  // Reset the various member variables used by |ComputeOcclusion()|.
  accumulated_area_ = 0.0;
  occluding_rects_.clear();
  video_element_area_ = 0.0;

  LayoutBox* box = To<LayoutBox>(VideoElement().GetLayoutObject());
  PhysicalRect bounds(box->PhysicalBorderBoxRect());
  video_element_area_ = ComputeArea(bounds);
  auto intersection_ratio =
      ComputeArea(intersection_rect_) / video_element_area_;

  auto* layout = VideoElement().GetLayoutObject();
  // Return early if the area of the video that intersects with the view is
  // below |visibility_threshold_|.
  if (!layout || intersection_ratio < visibility_threshold_) {
    report_visibility_cb_.Run(false);
    return;
  }

  accumulated_area_ = ComputeArea(bounds) - ComputeArea(intersection_rect_);

  if (MeetsVisibilityThreshold(intersection_rect_)) {
    report_visibility_cb_.Run(true);
    return;
  }

  report_visibility_cb_.Run(false);
}

void MediaVideoVisibilityTracker::DidFinishLifecycleUpdate(
    const LocalFrameView& local_frame_view) {
  if ((base::TimeTicks::Now() - last_hit_test_timestamp_ <
       hit_test_interval_)) {
    return;
  }
  last_hit_test_timestamp_ = base::TimeTicks::Now();

  intersection_rect_ = PhysicalRect();

  if (!VideoElement().GetLayoutObject()) {
    return;
  }

  LayoutBox* box = To<LayoutBox>(VideoElement().GetLayoutObject());
  gfx::RectF bounds(box->AbsoluteBoundingBoxRectF());

  gfx::Rect viewport_in_root_frame = ToEnclosingRect(
      local_frame_view.GetFrame().GetPage()->GetVisualViewport().VisibleRect());
  gfx::RectF absolute_viewport(
      local_frame_view.ConvertFromRootFrame(viewport_in_root_frame));
  intersection_rect_ = PhysicalRect::FastAndLossyFromRectF(
      IntersectRects(absolute_viewport, bounds));

  OnIntersectionChanged();
}

void MediaVideoVisibilityTracker::Trace(Visitor* visitor) const {
  visitor->Trace(video_element_);
  visitor->Trace(tracker_attached_to_document_);
}

}  // namespace blink
