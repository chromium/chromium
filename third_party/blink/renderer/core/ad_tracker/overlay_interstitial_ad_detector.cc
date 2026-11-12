// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/overlay_interstitial_ad_detector.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
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
  LayoutView* layout_view = element->GetDocument().GetLayoutView();
  LayoutObject* object = element->GetLayoutObject();

  DCHECK_NE(object, layout_view);

  LayoutObject* candidate = nullptr;
  for (; object != layout_view; object = object->Container()) {
    DCHECK(object);
    candidate = object;
  }

  DCHECK(candidate);

  // 'style' is now the ComputedStyle for the object whose position depends
  // on the document.
  const ComputedStyle& style = candidate->Style();
  if (style.GetPosition() == EPosition::kFixed ||
      style.HasStickyConstrainedPosition()) {
    return true;
  }

  if (style.GetPosition() == EPosition::kAbsolute) {
    return !layout_view->Style().ScrollsOverflow();
  }

  return false;
}

// Returns whether an overlay ad appearing now qualifies as a prestitial ad:
// it's within `kPrestitialWindow` of the first contentful paint, the page
// hasn't been hidden since navigation, and the user hasn't interacted with the
// page yet, i.e. there's no sticky user activation and the user hasn't
// scrolled.
bool IsPrestitialAd(LocalFrame* outermost_main_frame) {
  Document* document = outermost_main_frame->GetDocument();

  base::TimeTicks first_contentful_paint =
      PaintTiming::From(*document).FirstContentfulPaint();
  DCHECK(!first_contentful_paint.is_null());
  if (base::TimeTicks::Now() - first_contentful_paint >
      OverlayInterstitialAdDetector::kPrestitialWindow) {
    return false;
  }

  // Rendering is throttled while the page is hidden, so the paint timings of a
  // page that has been hidden no longer reflect what the user saw.
  InteractiveDetector* interactive_detector =
      InteractiveDetector::From(*document);
  if (interactive_detector->PageWasBackgroundedSinceEvent(
          document->Loader()->GetTiming().NavigationStart())) {
    return false;
  }

  if (outermost_main_frame->HasStickyUserActivation()) {
    return false;
  }

  return !interactive_detector->GetFirstScrollTimestamp().has_value();
}

}  // namespace

void OverlayInterstitialAdDetector::MaybeFireDetection(
    LocalFrame* outermost_main_frame) {
  DCHECK(outermost_main_frame);
  DCHECK(outermost_main_frame->IsOutermostMainFrame());
  if (popup_ad_detected_) {
    return;
  }

  DCHECK(outermost_main_frame->GetDocument());
  DCHECK(outermost_main_frame->ContentLayoutObject());

  // Skip any measurement before the FCP.
  if (PaintTiming::From(*outermost_main_frame->GetDocument())
          .FirstContentfulPaint()
          .is_null()) {
    return;
  }

  base::Time current_time = base::Time::Now();
  if (started_detection_ &&
      base::FeatureList::IsEnabled(
          features::kFrequencyCappingForOverlayPopupDetection) &&
      current_time < last_detection_time_ + kFireInterval) {
    return;
  }

  TRACE_EVENT0("blink,benchmark",
               "OverlayInterstitialAdDetector::MaybeFireDetection");

  bool is_first_detection = !started_detection_;
  started_detection_ = true;
  last_detection_time_ = current_time;

  gfx::Size outermost_main_frame_size =
      outermost_main_frame->View()
          ->LayoutViewport()
          ->VisibleContentRect(kExcludeScrollbars)
          .size();

  if (!is_first_detection &&
      outermost_main_frame_size != last_detection_outermost_main_frame_size_) {
    // Reset the candidate when the the viewport size has changed. Changing
    // the viewport size could influence the layout and may trick the detector
    // into believing that an element appeared and was dismissed, but what
    // could have happened is that the element no longer covers the center,
    // but still exists (e.g. a sticky ad at the top).
    candidate_id_ = kInvalidDOMNodeId;

    // Also mark the element now at the viewport center unqualified. We don't
    // want to consider an overlay as a popup if it wasn't counted before and
    // only satisfies the conditions later due to viewport size change.
    viewport_size_changed_ = true;
  }
  last_detection_outermost_main_frame_size_ = outermost_main_frame_size;

  // We want to explicitly prevent mid-roll ads from being categorized as
  // pop-ups. Skip the detection if we are in the middle of a video play.
  if (outermost_main_frame->View()->HasDominantVideoElement()) {
    return;
  }

  // HitTestNoLifecycleUpdate requires the lifecycle reached kPrePaintClean
  // (enforced by PaintLayer::HitTestLayer).
  if (outermost_main_frame->GetDocument()->Lifecycle().GetState() <
      DocumentLifecycle::kPrePaintClean) {
    return;
  }

  HitTestLocation location(
      gfx::PointF(outermost_main_frame_size.width() / 2.0,
                  outermost_main_frame_size.height() / 2.0));
  HitTestResult result;
  outermost_main_frame->ContentLayoutObject()->HitTestNoLifecycleUpdate(
      location, result);

  Element* element = result.InnerElement();
  if (!element) {
    return;
  }

  DOMNodeId element_id = element->GetDomNodeId();

  if (viewport_size_changed_) {
    viewport_size_changed_ = false;
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
      OnPopupDetected(
          outermost_main_frame, candidate_is_ad_, candidate_is_prestitial_,
          candidate_is_ad_
              ? DynamicTo<Element>(DOMNodeIds::NodeForId(candidate_id_))
              : nullptr);
    }

    if (popup_ad_detected_) {
      return;
    }

    last_unqualified_element_id_ = candidate_id_;
    candidate_id_ = kInvalidDOMNodeId;
    candidate_is_ad_ = false;
    candidate_is_prestitial_ = false;
  }

  if (element_id == last_unqualified_element_id_) {
    return;
  }

  if (!is_new_element) {
    // Potentially update the ad status of the candidate from non-ad to ad.
    // Ad tagging could occur after the initial painting (e.g. at loading time),
    // and we are making the best effort to catch it.
    if (element->IsAdRelated()) {
      candidate_is_ad_ = true;
    }

    return;
  }

  if (!element->GetLayoutObject()) {
    return;
  }

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
    // The prestitial conditions are about the state of the page when the
    // overlay appears, so evaluate them now rather than at dismissal time.
    bool is_prestitial = IsPrestitialAd(outermost_main_frame);

    // If main page is not scrollable, immediately determinine the overlay
    // to be a popup. There's is no need to check any state at the dismissal
    // time.
    if (!outermost_main_frame->GetDocument()
             ->GetLayoutView()
             ->HasScrollableOverflowY()) {
      OnPopupDetected(outermost_main_frame, is_ad, is_prestitial, element);
    }

    if (popup_ad_detected_) {
      return;
    }

    candidate_id_ = element_id;
    candidate_is_ad_ = is_ad;
    candidate_is_prestitial_ = is_prestitial;
    candidate_start_outermost_main_frame_scroll_position_ =
        outermost_main_frame->GetOutermostMainFrameScrollPosition().y();
  } else {
    last_unqualified_element_id_ = element_id;
  }
}

void OverlayInterstitialAdDetector::OnPopupDetected(
    LocalFrame* outermost_main_frame,
    bool is_ad,
    bool is_prestitial_ad,
    Element* ad_element) {
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

    if (is_prestitial_ad) {
      UseCounter::Count(outermost_main_frame->GetDocument(),
                        WebFeature::kOverlayPrestitialAd);
    }

    // Tag the element as a popup ad, and as a prestitial ad if applicable, for
    // the ad highlight label. The popup tag must come first: a prestitial is a
    // kind of popup, and Element::SetIsPrestitialAd() checks that.
    if (ad_element) {
      ad_element->SetIsOverlayPopupAd();
      if (is_prestitial_ad) {
        ad_element->SetIsPrestitialAd();
      }
    }
  }
}

}  // namespace blink
