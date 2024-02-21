// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/overlay_interstitial_ad_detector.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

namespace blink {

namespace {

constexpr base::TimeDelta kFireInterval = base::Seconds(1);
constexpr double kLargeAdSizeToViewportSizeThreshold = 0.1;

// An overlay interstitial element shouldn't move with scrolling and should be
// able to overlap with other contents. So, either:
// 1) one of its container ancestors (including itself) has fixed position.
// 2) <body> or <html> has style="overflow:hidden", and among its container
// ancestors (including itself), the 2nd to the top (where the top should always
// be the <body>) has absolute position.
bool IsOverlayCandidate(Element* element) {
  const ComputedStyle* style = nullptr;
  LayoutView* layout_view = element->GetDocument().GetLayoutView();
  LayoutObject* object = element->GetLayoutObject();

  DCHECK_NE(object, layout_view);

  for (; object != layout_view; object = object->Container()) {
    DCHECK(object);
    style = object->Style();
  }

  DCHECK(style);

  // 'style' is now the ComputedStyle for the object whose position depends
  // on the document.
  if (style->GetPosition() == EPosition::kFixed ||
      style->HasStickyConstrainedPosition()) {
    return true;
  }

  if (style->GetPosition() == EPosition::kAbsolute)
    return !object->StyleRef().ScrollsOverflow();

  return false;
}

}  // namespace

void OverlayInterstitialAdDetector::MaybeFireDetection(
    LocalFrame* outermost_main_frame) {
  DCHECK(outermost_main_frame);
  DCHECK(outermost_main_frame->IsOutermostMainFrame());
  if (popup_ad_detected_)
    return;

  DCHECK(outermost_main_frame->GetDocument());
  DCHECK(outermost_main_frame->ContentLayoutObject());

  // Skip any measurement before the FCP.
  if (PaintTiming::From(*outermost_main_frame->GetDocument())
          .FirstContentfulPaintIgnoringSoftNavigations()
          .is_null()) {
    return;
  }

  base::Time current_time = base::Time::Now();
  if (started_detection_ &&
      base::FeatureList::IsEnabled(
          features::kFrequencyCappingForOverlayPopupDetection) &&
      current_time < last_detection_time_ + kFireInterval)
    return;

  TRACE_EVENT0("blink,benchmark",
               "OverlayInterstitialAdDetector::MaybeFireDetection");

  started_detection_ = true;
  last_detection_time_ = current_time;

  gfx::Size outermost_main_frame_size = outermost_main_frame->View()
                                            ->LayoutViewport()
                                            ->VisibleContentRect()
                                            .size();

  if (outermost_main_frame_size != last_detection_outermost_main_frame_size_) {
    // Reset the candidate when the the viewport size has changed. Changing
    // the viewport size could influence the layout and may trick the detector
    // into believing that an element appeared and was dismissed, but what
    // could have happened is that the element no longer covers the center,
    // but still exists (e.g. a sticky ad at the top).
    candidate_id_ = kInvalidDOMNodeId;

    // Reset |content_has_been_stable_| to so that the current hit-test element
    // will be marked unqualified. We don't want to consider an overlay as a
    // popup if it wasn't counted before and only satisfies the conditions later
    // due to viewport size change.
    content_has_been_stable_ = false;

    last_detection_outermost_main_frame_size_ = outermost_main_frame_size;
  }

  // We want to explicitly prevent mid-roll ads from being categorized as
  // pop-ups. Skip the detection if we are in the middle of a video play.
  if (outermost_main_frame->View()->HasDominantVideoElement())
    return;

  HitTestLocation location(
      gfx::PointF(outermost_main_frame_size.width() / 2.0,
                  outermost_main_frame_size.height() / 2.0));
  HitTestResult result;
  outermost_main_frame->ContentLayoutObject()->HitTestNoLifecycleUpdate(
      location, result);

  Element* element = result.InnerElement();
  if (!element)
    return;

  DOMNodeId element_id = element->GetDomNodeId();

  // Skip considering the overlay for a pop-up candidate if we haven't seen or
  // have just seen the first meaningful paint, or if the viewport size has just
  // changed. If we have just seen the first meaningful paint, however, we
  // would consider future overlays for pop-up candidates.
  if (!content_has_been_stable_) {
    if (!PaintTiming::From(*outermost_main_frame->GetDocument())
             .FirstMeaningfulPaint()
             .is_null()) {
      content_has_been_stable_ = true;
    }

    last_unqualified_element_id_ = element_id;
    return;
  }

  bool is_new_element = (element_id != candidate_id_);

  // The popup candidate has just been dismissed.
  if (is_new_element && candidate_id_ != kInvalidDOMNodeId) {
    // If the main frame scrolling position hasn't changed since the candidate's
    // appearance, we consider it to be a overlay interstitial; otherwise, we
    // skip that candidate because it could be a parallax/scroller ad.
    if (outermost_main_frame->GetOutermostMainFrameScrollPosition().y() ==
        candidate_start_outermost_main_frame_scroll_position_) {
      OnPopupDetected(outermost_main_frame, candidate_is_ad_);
    }

    if (popup_ad_detected_)
      return;

    last_unqualified_element_id_ = candidate_id_;
    candidate_id_ = kInvalidDOMNodeId;
    candidate_is_ad_ = false;
  }

  if (element_id == last_unqualified_element_id_)
    return;

  if (!is_new_element) {
    // Potentially update the ad status of the candidate from non-ad to ad.
    // Ad tagging could occur after the initial painting (e.g. at loading time),
    // and we are making the best effort to catch it.
    if (element->IsAdRelated())
      candidate_is_ad_ = true;

    return;
  }

  if (!element->GetLayoutObject())
    return;

  gfx::Rect overlay_rect =
      element->GetLayoutObject()->AbsoluteBoundingBoxRect();

  bool is_large =
      (overlay_rect.size().Area64() > outermost_main_frame_size.Area64() *
                                          kLargeAdSizeToViewportSizeThreshold);

  bool has_gesture =
      LocalFrame::HasTransientUserActivation(outermost_main_frame);
  bool is_ad = element->IsAdRelated();

  if (!has_gesture && is_large && (!popup_detected_ || is_ad) &&
      IsOverlayCandidate(element)) {
    // If main page is not scrollable, immediately determinine the overlay
    // to be a popup. There's is no need to check any state at the dismissal
    // time.
    if (!outermost_main_frame->GetDocument()
             ->GetLayoutView()
             ->HasScrollableOverflowY()) {
      OnPopupDetected(outermost_main_frame, is_ad);
    }

    if (popup_ad_detected_)
      return;

    candidate_id_ = element_id;
    candidate_is_ad_ = is_ad;
    candidate_start_outermost_main_frame_scroll_position_ =
        outermost_main_frame->GetOutermostMainFrameScrollPosition().y();
  } else {
    last_unqualified_element_id_ = element_id;
  }
}

void OverlayInterstitialAdDetector::OnPopupDetected(
    LocalFrame* outermost_main_frame,
    bool is_ad) {
  if (!popup_detected_) {
    UseCounter::Count(outermost_main_frame->GetDocument(),
                      WebFeature::kOverlayPopup);
    popup_detected_ = true;
  }

  if (is_ad) {
    DCHECK(!popup_ad_detected_);
    outermost_main_frame->Client()->OnOverlayPopupAdDetected();
    UseCounter::Count(outermost_main_frame->GetDocument(),
                      WebFeature::kOverlayPopupAd);
    popup_ad_detected_ = true;
  }
}

}  // namespace blink
