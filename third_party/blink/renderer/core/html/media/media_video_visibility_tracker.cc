// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_video_visibility_tracker.h"

#include "base/metrics/histogram_macros.h"
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
#include "ui/gfx/geometry/skia_conversions.h"

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

bool HasEnoughVisibleAreaRemaining(float occluded_area,
                                   const PhysicalRect& video_element_rect,
                                   float visibility_threshold) {
  return occluded_area / ComputeArea(video_element_rect) <
         (1 - visibility_threshold);
}

float ComputeOccludingArea(const Vector<SkIRect>& occluding_rects,
                           float video_element_area) {
  float occluding_area = 0.0;

  std::vector<SkIRect> sk_rects;
  for (const auto rect : occluding_rects) {
    sk_rects.push_back(rect);
  }

  SkRegion region;
  bool compute_area = region.setRects(sk_rects.data(), occluding_rects.size());

  if (!compute_area) {
    return occluding_area;
  }

  for (SkRegion::Iterator it(region); !it.done(); it.next()) {
    auto occluding_rect = it.rect();
    occluding_area +=
        ComputeArea(PhysicalRect(gfx::SkIRectToRect(occluding_rect)));

    if (occluding_area >= video_element_area) {
      return video_element_area;
    }
  }

  return occluding_area;
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
  const auto& video_element = VideoElement();
  if (video_element.GetWebMediaPlayer() &&
      video_element.GetExecutionContext() && !video_element.paused()) {
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

  // Add the current occluding node rect to `occluding_rects_` and compute the
  // total occluded area.
  occluding_rects_.push_back(gfx::RectToSkIRect(ToPixelSnappedRect(node_rect)));
  occluded_area_ =
      ComputeOccludingArea(occluding_rects_, ComputeArea(video_element_rect_));

  if (HasEnoughVisibleAreaRemaining(occluded_area_, video_element_rect_,
                                    visibility_threshold_)) {
    return kContinueHitTesting;
  }

  return kStopHitTesting;
}

bool MediaVideoVisibilityTracker::MeetsVisibilityThreshold(
    const PhysicalRect& rect) {
  {
    // Record the total time spent computing occlusion.
    SCOPED_UMA_HISTOGRAM_TIMER(
        "Media.MediaVideoVisibilityTracker.ComputeOcclusion.TotalDuration");

    HitTestResult result(HitTestForOcclusionRatio(
        VideoElement(), rect,
        WTF::BindRepeating(&MediaVideoVisibilityTracker::ComputeOcclusion,
                           WrapPersistent(this))));
  }

  return HasEnoughVisibleAreaRemaining(occluded_area_, video_element_rect_,
                                       visibility_threshold_)
             ? true
             : false;
}

void MediaVideoVisibilityTracker::OnIntersectionChanged() {
  LayoutBox* box = To<LayoutBox>(VideoElement().GetLayoutObject());
  PhysicalRect bounds(box->PhysicalBorderBoxRect());
  auto intersection_ratio =
      ComputeArea(intersection_rect_) / ComputeArea(bounds);

  auto* layout = VideoElement().GetLayoutObject();
  // Return early if the area of the video that intersects with the view is
  // below |visibility_threshold_|.
  if (!layout || intersection_ratio < visibility_threshold_) {
    report_visibility_cb_.Run(false);
    return;
  }

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

  // Reset the various member variables used by `ComputeOcclusion()`.
  occluded_area_ = 0.0;
  occluding_rects_.clear();
  intersection_rect_ = PhysicalRect();
  video_element_rect_ = PhysicalRect();

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

  video_element_rect_ = PhysicalRect::FastAndLossyFromRectF(bounds);

  // Compute the VideoElement area that is occluded by the viewport, if any.
  SkRegion region;
  region.setRect(gfx::RectToSkIRect(gfx::ToRoundedRect(bounds)));
  if (region.op(gfx::RectToSkIRect(gfx::ToRoundedRect(absolute_viewport)),
                SkRegion::kDifference_Op)) {
    for (SkRegion::Iterator it(region); !it.done(); it.next()) {
      auto occluding_rect = it.rect();
      occluding_rects_.push_back(occluding_rect);
      it.next();
    }
  }

  OnIntersectionChanged();
}

void MediaVideoVisibilityTracker::Trace(Visitor* visitor) const {
  visitor->Trace(video_element_);
  visitor->Trace(tracker_attached_to_document_);
}

}  // namespace blink
