// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/sticky_ad_detector.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

#include <cstdlib>

namespace blink {

namespace {

constexpr base::TimeDelta kFireInterval = base::TimeDelta::FromSeconds(1);
constexpr double kLargeAdSizeToViewportSizeThreshold = 0.3;

// An sticky element should have a non-default position w.r.t. the viewport. The
// main page should also be scrollable.
bool IsStickyAdCandidate(Element* element) {
  if (!element->IsAdRelated())
    return false;

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
  return style->GetPosition() != EPosition::kStatic;
}

}  // namespace

void StickyAdDetector::MaybeFireDetection(LocalFrame* main_frame) {
  DCHECK(main_frame);
  DCHECK(main_frame->IsMainFrame());
  if (done_detection_)
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
  if (last_detection_time_.has_value() &&
      base::FeatureList::IsEnabled(
          features::kFrequencyCappingForLargeStickyAdDetection) &&
      current_time < last_detection_time_.value() + kFireInterval) {
    return;
  }

  TRACE_EVENT0("blink,benchmark", "StickyAdDetector::MaybeFireDetection");

  IntSize main_frame_size = main_frame->GetMainFrameViewportSize();

  // Hit test the bottom center of the viewport.
  HitTestLocation location(DoublePoint(main_frame_size.Width() / 2.0,
                                       main_frame_size.Height() * 9.0 / 10));

  HitTestResult result;
  main_frame->ContentLayoutObject()->HitTestNoLifecycleUpdate(location, result);

  last_detection_time_ = current_time;

  Element* element = result.InnerElement();
  if (!element)
    return;

  DOMNodeId element_id = DOMNodeIds::IdForNode(element);

  if (element_id == candidate_id_) {
    // If the main frame scrolling position has changed by a distance greater
    // than the height of the candidate, and the candidate is still at the
    // bottom center, then we record the use counter.
    if (std::abs(candidate_start_main_frame_scroll_offset_ -
                 main_frame->GetMainFrameScrollOffset().Y()) >
        candidate_height_) {
      OnLargeStickyAdDetected(main_frame);
    }
    return;
  }

  // The hit testing returns an element different from the current candidate,
  // and the main frame scroll offset hasn't changed much. In this case we
  // we don't consider the candidate to be a sticky ad, because it may have
  // been dismissed along with scrolling (e.g. parallax/scroller ad), or may
  // have dismissed itself soon after its appearance.
  candidate_id_ = kInvalidDOMNodeId;

  if (!element->GetLayoutObject())
    return;

  IntRect overlay_rect = element->GetLayoutObject()->AbsoluteBoundingBoxRect();

  bool is_large =
      (overlay_rect.Size().Area() >
       main_frame_size.Area() * kLargeAdSizeToViewportSizeThreshold);

  bool is_main_page_scrollable =
      element->GetDocument().GetLayoutView()->HasScrollableOverflowY();

  if (is_large && is_main_page_scrollable && IsStickyAdCandidate(element)) {
    candidate_id_ = element_id;
    candidate_height_ = overlay_rect.Size().Height();
    candidate_start_main_frame_scroll_offset_ =
        main_frame->GetMainFrameScrollOffset().Y();
  }
}

void StickyAdDetector::OnLargeStickyAdDetected(LocalFrame* main_frame) {
  main_frame->Client()->OnLargeStickyAdDetected();
  UseCounter::Count(main_frame->GetDocument(), WebFeature::kLargeStickyAd);
  done_detection_ = true;
}

}  // namespace blink
