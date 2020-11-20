// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/overlay_interstitial_ad_detector.h"

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

namespace blink {

namespace {

static bool g_frequency_capping_enabled = true;

constexpr base::TimeDelta kFireInterval = base::TimeDelta::FromSeconds(1);
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
  if (style->HasViewportConstrainedPosition() ||
      style->HasStickyConstrainedPosition()) {
    return true;
  }

  if (style->GetPosition() == EPosition::kAbsolute)
    return !object->StyleRef().ScrollsOverflow();

  return false;
}

}  // namespace

void OverlayInterstitialAdDetector::MaybeFireDetection(LocalFrame* main_frame) {
  DCHECK(main_frame);
  DCHECK(main_frame->IsMainFrame());
  if (popup_ad_detected_)
    return;

  DCHECK(main_frame->GetDocument());
  DCHECK(main_frame->ContentLayoutObject());

  // Skip any measurement before the FCP.
  if (PaintTiming::From(*main_frame->GetDocument())
          .FirstContentfulPaint()
          .is_null()) {
    return;
  }

  base::Time current_time = base::Time::Now();
  if (started_detection_ && g_frequency_capping_enabled &&
      current_time < last_detection_time_ + kFireInterval)
    return;

  TRACE_EVENT0("blink,benchmark",
               "OverlayInterstitialAdDetector::MaybeFireDetection");

  started_detection_ = true;
  last_detection_time_ = current_time;

  IntSize main_frame_size = main_frame->GetMainFrameViewportSize();

  if (main_frame_size != last_detection_main_frame_size_) {
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

    last_detection_main_frame_size_ = main_frame_size;
  }

  // We want to explicitly prevent mid-roll ads from being categorized as
  // pop-ups. Skip the detection if we are in the middle of a video play.
  if (main_frame->View()->HasDominantVideoElement())
    return;

  HitTestLocation location(DoublePoint(main_frame_size.Width() / 2.0,
                                       main_frame_size.Height() / 2.0));
  HitTestResult result;
  main_frame->ContentLayoutObject()->HitTestNoLifecycleUpdate(location, result);

  Element* element = result.InnerElement();
  if (!element)
    return;

  DOMNodeId element_id = DOMNodeIds::IdForNode(element);

  // Skip considering the overlay for a pop-up candidate if we haven't seen or
  // have just seen the first meaningful paint, or if the viewport size has just
  // changed. If we have just seen the first meaningful paint, however, we
  // would consider future overlays for pop-up candidates.
  if (!content_has_been_stable_) {
    if (!PaintTiming::From(*main_frame->GetDocument())
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
    // If the main frame scrolling offset hasn't changed since the candidate's
    // appearance, we consider it to be a overlay interstitial; otherwise, we
    // skip that candidate because it could be a parallax/scroller ad.
    if (main_frame->GetMainFrameScrollOffset().Y() ==
        candidate_start_main_frame_scroll_offset_) {
      OnPopupDetected(main_frame, candidate_is_ad_);
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

  IntRect overlay_rect = element->GetLayoutObject()->AbsoluteBoundingBoxRect();

  bool is_large =
      (overlay_rect.Size().Area() >
       main_frame_size.Area() * kLargeAdSizeToViewportSizeThreshold);

  bool has_gesture = LocalFrame::HasTransientUserActivation(main_frame);
  bool is_ad = element->IsAdRelated();

  if (!has_gesture && is_large && (!popup_detected_ || is_ad) &&
      IsOverlayCandidate(element)) {
    // If main page is not scrollable, immediately determinine the overlay
    // to be a popup. There's is no need to check any state at the dismissal
    // time.
    if (!main_frame->GetDocument()->GetLayoutView()->HasScrollableOverflowY()) {
      OnPopupDetected(main_frame, is_ad);
    }

    if (popup_ad_detected_)
      return;

    candidate_id_ = element_id;
    candidate_is_ad_ = is_ad;
    candidate_start_main_frame_scroll_offset_ =
        main_frame->GetMainFrameScrollOffset().Y();
  } else {
    last_unqualified_element_id_ = element_id;
  }
}

// static
void OverlayInterstitialAdDetector::DisableFrequencyCappingForTesting() {
  g_frequency_capping_enabled = false;
}

void OverlayInterstitialAdDetector::OnPopupDetected(LocalFrame* main_frame,
                                                    bool is_ad) {
  if (!popup_detected_) {
    UseCounter::Count(main_frame->GetDocument(), WebFeature::kOverlayPopup);
    popup_detected_ = true;
  }

  if (is_ad) {
    DCHECK(!popup_ad_detected_);
    UseCounter::Count(main_frame->GetDocument(), WebFeature::kOverlayPopupAd);
    popup_ad_detected_ = true;
  }
}

}  // namespace blink
