// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/display_ad_element_monitor.h"

#include <cstdlib>

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

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

bool MeetsLargeStickyAdGeometry(const gfx::Rect& main_frame_viewport,
                                const gfx::Rect& ad_visible_rect) {
  constexpr double kLargeAdSizeToViewportSizeThreshold = 0.3;
  bool is_large = (ad_visible_rect.size().Area64() >
                   main_frame_viewport.size().Area64() *
                       kLargeAdSizeToViewportSizeThreshold);

  bool is_at_bottom = ad_visible_rect.Contains(
      main_frame_viewport.width() / 2, main_frame_viewport.height() * 9 / 10);

  return is_large && is_at_bottom;
}

// Determines whether the ad element or its containing block chain is static
// w.r.t. the viewport.
//
// In-flow (static) elements flow with the document and cannot be sticky on
// their own. Identifying them allows filtering out false positives caused by
// layout shifts (e.g., scroll anchoring adjusting the scroll offset when
// content expands above the viewport, while keeping the ad stationary in the
// viewport).
bool IsOutermostContainerStatic(Element* element) {
  DCHECK(element);

  LayoutView* layout_view = element->GetDocument().GetLayoutView();
  LayoutObject* object = element->GetLayoutObject();

  // LayoutView is only created for Document, so an Element's LayoutObject can
  // never be the LayoutView.
  DCHECK_NE(object, layout_view);

  // Traverse the containing block chain to find the outermost container
  // directly under LayoutView. Because the element is attached and in a clean
  // post-paint state, Container() is guaranteed to reach LayoutView.
  LayoutObject* candidate = nullptr;
  for (; object != layout_view; object = object->Container()) {
    DCHECK(object);
    candidate = object;
  }

  DCHECK(candidate);

  // 'candidate' is the outermost object whose position depends on the
  // document (e.g., <html> for in-flow content, or a fixed/absolute element).
  const ComputedStyle& style = candidate->StyleRef();
  return style.GetPosition() == EPosition::kStatic;
}

}  // namespace

DisplayAdElementMonitor::DisplayAdElementMonitor(Element* element,
                                                 AdProvenance ad_provenance)
    : element_(element), ad_provenance_(std::move(ad_provenance)) {
  DCHECK(element_);
  probe::UpdateAdRelatedState(*element, ad_provenance_);

  EnsureStarted();
}

void DisplayAdElementMonitor::UpdateToVideoAd() {
  if (!is_video_ad_) {
    is_video_ad_ = true;
  }
  MaybeRecordVideoAdUseCounter();

  if (is_sticky_ad_ && !is_sticky_video_ad_) {
    UpdateToStickyVideoAd();
  }
}

void DisplayAdElementMonitor::MaybeRecordVideoAdUseCounter() {
  if (!did_record_video_ad_use_counter_ && is_video_ad_ &&
      overlay_visibility_ == OverlayVisibility::kVisible &&
      !last_reported_rect_.IsEmpty()) {
    did_record_video_ad_use_counter_ = true;
    if (LocalFrame* frame = element_->GetDocument().GetFrame()) {
      UseCounter::Count(frame->LocalFrameRoot().GetDocument(),
                        WebFeature::kVideoAdDetected);
    }
  }
}

void DisplayAdElementMonitor::EnsureStarted() {
  if (started_ || !ShouldMonitorElement(element_.Get())) {
    return;
  }

  started_ = true;
  element_->GetDocument().View()->RegisterForLifecycleNotifications(this);
}

void DisplayAdElementMonitor::OnElementRemoved() {
  if (!started_) {
    return;
  }

  if (element_->InActiveDocument() && !last_reported_rect_.IsEmpty()) {
    gfx::Rect empty_rect;
    element_->GetDocument()
        .GetFrame()
        ->LocalFrameRoot()
        .Client()
        ->OnMainFrameAdRectangleChanged(element_->GetDomNodeId(), empty_rect);
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

  // We use this lifecycle update to check the "Highlight ads" settings, which
  // are toggled via the internals page and DevTools. If the combined state
  // changes, we trigger a repaint. While less precise than relying on direct
  // toggling events, reading this local state provides better robustness
  // against race conditions and missed updates.
  bool should_highlight =
      frame->GetPage()->GetSettings().GetHighlightAds() ||
      frame->GetPage()->GetSettings().GetInspectorHighlightAds();
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
    gfx::Rect rect_in_viewport = r->AbsoluteBoundingBoxRect(
        {MapCoordinatesMode::kTraverseDocumentBoundaries});

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

      // `CalculateStickyAdState` may re-check visibility using an unthrottled
      // hit-test, which gives a more accurate response; use this result if
      // available.
      if (overlay_visibility_ == OverlayVisibility::kVisible) {
        OverlayVisibility sticky_ad_visibility =
            CalculateStickyAdState(local_root_main_frame, rect_in_viewport);
        if (sticky_ad_visibility != OverlayVisibility::kSkipped) {
          overlay_visibility_ = sticky_ad_visibility;
        }
      }

      if (overlay_visibility_ == OverlayVisibility::kVisible) {
        // Maps the rectangle from its coordinates within the viewport's
        // coordinate system to the document's coordinate system.
        rect_to_report = rect_in_viewport + local_root_main_frame.View()
                                                ->LayoutViewport()
                                                ->PixelSnappedScrollOffset();
      }
    }
  }

  if (last_reported_rect_ != rect_to_report) {
    local_root_main_frame.Client()->OnMainFrameAdRectangleChanged(
        element_->GetDomNodeId(), rect_to_report);
    last_reported_rect_ = rect_to_report;
  }

  MaybeRecordVideoAdUseCounter();
}

DisplayAdElementMonitor::OverlayVisibility
DisplayAdElementMonitor::CheckOverlayVisibility(
    const LocalFrame& main_frame,
    const gfx::Rect& rect_in_viewport,
    bool ignore_throttling) {
  DCHECK(element_->GetLayoutObject());

  constexpr base::TimeDelta kFireInterval = base::Seconds(1);

  base::TimeTicks now = base::TimeTicks::Now();

  if (!ignore_throttling && !last_overlay_check_time_.is_null() &&
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

DisplayAdElementMonitor::OverlayVisibility
DisplayAdElementMonitor::CalculateStickyAdState(
    const LocalFrame& local_root_main_frame,
    const gfx::Rect& rect_in_viewport) {
  if (is_sticky_ad_) {
    return OverlayVisibility::kSkipped;
  }

  gfx::Rect main_frame_viewport = gfx::Rect(
      gfx::Point(), local_root_main_frame.GetOutermostMainFrameSize());

  gfx::Rect ad_visible_rect =
      IntersectRects(rect_in_viewport, main_frame_viewport);

  // If the ad isn't within the viewport, skip the measurement.
  if (ad_visible_rect.IsEmpty()) {
    return OverlayVisibility::kSkipped;
  }

  int current_scroll_position =
      local_root_main_frame.GetOutermostMainFrameScrollPosition().y();
  int current_ad_y_position_in_viewport = rect_in_viewport.y();

  if (!sticky_ad_measurement_) {
    sticky_ad_measurement_ = {current_scroll_position,
                              current_ad_y_position_in_viewport,
                              rect_in_viewport.height()};
  } else {
    // Allow a tolerance (20% of the ad's height) to handle JS-driven sticky ads
    // that are not pixel-perfectly sticky but quickly reposition themselves to
    // visually snap to the same position.
    int tolerance = sticky_ad_measurement_->ad_height / 5;
    int y_difference =
        std::abs(sticky_ad_measurement_->ad_y_position_in_viewport -
                 current_ad_y_position_in_viewport);

    if (y_difference > tolerance) {
      // The ad has moved beyond the tolerance. Discard the previous anchor and
      // establish a new one.
      sticky_ad_measurement_ = {current_scroll_position,
                                current_ad_y_position_in_viewport,
                                rect_in_viewport.height()};
    } else {
      // If the scroll position changes substantially (by more than the ad's
      // height) and the current y-position in the viewport hasn't changed
      // much (within tolerance), consider this a sticky ad.
      //
      // Requiring a scroll distance greater than the ad's height prevents
      // transient sticky ads from being inadvertently categorized (e.g.,
      // parallax or scroller ads that will soon dismiss after a short scroll).
      if (std::abs(current_scroll_position -
                   sticky_ad_measurement_->viewport_scroll_position) >
          sticky_ad_measurement_->ad_height) {
        // Avoid declaring in-flow elements sticky when layout shifts (e.g.,
        // scroll anchoring) alter scroll offset without moving the element.
        if (IsOutermostContainerStatic(element_.Get())) {
          sticky_ad_measurement_.reset();
          return OverlayVisibility::kSkipped;
        }

        // Verify the ad is currently visible before declaring it sticky. This
        // handles edge cases like fast scrolling past a parallax ad before the
        // frequency-capped visibility check updates the state.
        OverlayVisibility hit_test_result = CheckOverlayVisibility(
            local_root_main_frame, ad_visible_rect, /*ignore_throttling=*/true);
        if (hit_test_result != OverlayVisibility::kVisible) {
          sticky_ad_measurement_.reset();
          return hit_test_result;
        }

        UpdateToStickyAd(local_root_main_frame, main_frame_viewport,
                         ad_visible_rect);
        return hit_test_result;
      }
    }
  }

  return OverlayVisibility::kSkipped;
}

void DisplayAdElementMonitor::UpdateToStickyAd(
    const LocalFrame& local_root_main_frame,
    const gfx::Rect& main_frame_viewport,
    const gfx::Rect& ad_visible_rect) {
  CHECK(!is_sticky_ad_);

  is_sticky_ad_ = true;

  // Determine if the newly sticky ad matches any sub-types (e.g., "large
  // sticky ad", "sticky video ad") to record metrics and send notifications.
  //
  // For large sticky ads, geometry checks are done only once upon initial
  // stickiness detection. We accept potential false negatives (e.g., if the ad
  // resizes to become large later) in favor of simplicity and performance.
  // For sticky video ads, detection is recorded either here (if already tagged
  // as a video ad) or when the ad transitions to a video ad later via
  // `UpdateToVideoAd`.
  if (MeetsLargeStickyAdGeometry(main_frame_viewport, ad_visible_rect)) {
    UseCounter::Count(local_root_main_frame.GetDocument(),
                      WebFeature::kLargeStickyAd);

    local_root_main_frame.Client()->OnLargeStickyAdDetected();
  }

  if (is_video_ad_ && !is_sticky_video_ad_) {
    UpdateToStickyVideoAd();
  }
}

void DisplayAdElementMonitor::UpdateToStickyVideoAd() {
  CHECK(is_sticky_ad_);
  CHECK(is_video_ad_);
  CHECK(!is_sticky_video_ad_);

  is_sticky_video_ad_ = true;

  if (LocalFrame* frame = element_->GetDocument().GetFrame()) {
    UseCounter::Count(frame->LocalFrameRoot().GetDocument(),
                      WebFeature::kStickyVideoAdDetected);
  }
}

void DisplayAdElementMonitor::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  NodeRareDataField::Trace(visitor);
}

}  // namespace blink
