// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/display_ad_element_monitor.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"

namespace blink {

namespace {

// Determines if a given element is eligible for ad monitoring.
bool ShouldMonitorElement(Element* element) {
  DCHECK(element);

  LocalFrame* frame = element->GetDocument().GetFrame();
  if (!frame) {
    return false;
  }

  if (!frame->View()) {
    return false;
  }

  // We only monitor the "root" ad element. If the element lives in an ad-tagged
  // iframe, we can skip it to avoid redundant monitoring.
  if (frame->IsAdFrame()) {
    return false;
  }

  // Restrict monitoring to elements within the outermost main frame's local
  // subtree.
  const LocalFrame& local_root_main_frame = frame->LocalFrameRoot();
  if (!local_root_main_frame.IsOutermostMainFrame()) {
    return false;
  }

  return true;
}

}  // namespace

DisplayAdElementMonitor::DisplayAdElementMonitor(Element* element)
    : element_(element) {
  DCHECK(element_);
  EnsureStarted();
}

void DisplayAdElementMonitor::EnsureStarted() {
  if (started_ || !ShouldMonitorElement(element_.Get())) {
    return;
  }

  started_ = true;
  element_->GetDocument().View()->RegisterForLifecycleNotifications(this);
}

void DisplayAdElementMonitor::OnElementRemovedOrUntagged() {
  if (!started_) {
    return;
  }

  if (element_->InActiveDocument() && !last_reported_rect_.IsEmpty()) {
    gfx::Rect empty_rect;
    element_->GetDocument().GetFrame()->Client()->OnMainFrameAdRectangleChanged(
        element_->GetDomNodeId(), empty_rect);
    last_reported_rect_ = empty_rect;
  }

  if (element_->GetDocument().View()) {
    element_->GetDocument().View()->UnregisterFromLifecycleNotifications(this);
  }
  started_ = false;
}

void DisplayAdElementMonitor::DidFinishLifecycleUpdate(
    const LocalFrameView& local_frame_view) {
  // Re-check eligibility. This is a safeguard against race conditions where the
  // element's state might have changed after the observer was attached.
  if (!ShouldMonitorElement(element_.Get())) {
    return;
  }

  LocalFrame* frame = element_->GetDocument().GetFrame();
  DCHECK(frame);

  // We use this lifecycle update as an opportunity to poll the "Highlight ads"
  // setting (toggled by DevTools). If it has changed, we trigger a repaint.
  // This polling approach is less precise than relying on direct events, but
  // it's more robust against potential race conditions or missed state updates.
  bool should_highlight = frame->GetPage()->GetSettings().GetHighlightAds();
  if (should_highlight != should_highlight_) {
    should_highlight_ = should_highlight;
    if (auto* layout_object = element_->GetLayoutObject()) {
      layout_object->SetShouldDoFullPaintInvalidation();
    }
  }

  const LocalFrame& local_root_main_frame = frame->LocalFrameRoot();

  if (PaintTiming::From(*(local_root_main_frame.GetDocument()))
          .FirstContentfulPaint()
          .is_null()) {
    return;
  }

  gfx::Rect rect_to_report;
  if (LayoutObject* r = element_->GetLayoutObject()) {
    // Get the element's bounding box relative to the main frame's viewport.
    gfx::Rect rect_in_viewport =
        r->AbsoluteBoundingBoxRect(kTraverseDocumentBoundaries);

    // Exclude ads that are invisible or too small (e.g. tracking pixels).
    if (rect_in_viewport.width() > 1 && rect_in_viewport.height() > 1) {
      OverlayVisibility overlay_visibility =
          CheckOverlayVisibility(local_root_main_frame, rect_in_viewport);

      // If the visibility check was skipped due to throttling, use the previous
      // result. Otherwise, update our status.
      if (overlay_visibility != OverlayVisibility::kSkipped) {
        overlay_visibility_ = overlay_visibility;
      }

      CHECK_NE(overlay_visibility_, OverlayVisibility::kSkipped);

      if (overlay_visibility_ == OverlayVisibility::kVisible) {
        // Maps the rectangle from its coordinates within the viewport's
        // coordinate system to the document's coordinate system.
        rect_to_report =
            rect_in_viewport +
            local_root_main_frame.View()->LayoutViewport()->ScrollOffsetInt();
      }
    }
  }

  if (last_reported_rect_ != rect_to_report) {
    local_root_main_frame.Client()->OnMainFrameAdRectangleChanged(
        element_->GetDomNodeId(), rect_to_report);
    last_reported_rect_ = rect_to_report;
  }
}

DisplayAdElementMonitor::OverlayVisibility
DisplayAdElementMonitor::CheckOverlayVisibility(
    const LocalFrame& main_frame,
    const gfx::Rect& rect_in_viewport) {
  DCHECK(element_->GetLayoutObject());

  constexpr base::TimeDelta kFireInterval = base::Seconds(1);

  base::TimeTicks now = base::TimeTicks::Now();

  if (!last_overlay_check_time_.is_null() &&
      now < last_overlay_check_time_ + kFireInterval) {
    return OverlayVisibility::kSkipped;
  }

  last_overlay_check_time_ = now;

  gfx::Rect viewport =
      gfx::Rect(gfx::Point(), main_frame.GetOutermostMainFrameSize());

  // For performance reasons, we only check for overlay visibility for elements
  // within the viewport.
  gfx::Rect intersection_rect = IntersectRects(rect_in_viewport, viewport);
  if (intersection_rect.IsEmpty()) {
    return OverlayVisibility::kSkipped;
  }

  // Hit-tests at the center of `intersection_rect` to see if the element is
  // visible to the user.
  gfx::Point intersection_rect_center =
      gfx::Point(intersection_rect.x() + intersection_rect.width() / 2,
                 intersection_rect.y() + intersection_rect.height() / 2);

  HitTestLocation location(intersection_rect_center);

  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kReadOnly | HitTestRequest::kAllowChildFrameContent |
      HitTestRequest::kIgnoreZeroOpacityObjects |
      HitTestRequest::kHitTestVisualOverflow;

  HitTestRequest request(hit_type, /*stop_node=*/element_->GetLayoutObject());
  HitTestResult result(request, location);

  main_frame.ContentLayoutObject()->HitTestNoLifecycleUpdate(location, result);

  Node* inner_node = result.InnerNode();

  if (!inner_node || inner_node->GetDomNodeId() != element_->GetDomNodeId()) {
    return OverlayVisibility::kInvisible;
  }

  return OverlayVisibility::kVisible;
}

void DisplayAdElementMonitor::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  ElementRareDataField::Trace(visitor);
}

}  // namespace blink
