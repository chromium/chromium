// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/display_ad_element_monitor.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"

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

void DisplayAdElementMonitor::OnElementRemoved() {
  if (!started_) {
    return;
  }

  if (element_->InActiveDocument() && !last_reported_rect_.IsEmpty()) {
    gfx::Rect empty_rect;
    element_->GetDocument()
        .GetFrame()
        ->Client()
        ->OnMainFrameImageAdRectangleChanged(element_->GetDomNodeId(),
                                             empty_rect);
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

  const LocalFrame& local_root_main_frame = frame->LocalFrameRoot();

  gfx::Rect rect_to_report;
  if (LayoutObject* r = element_->GetLayoutObject()) {
    // Get the element's bounding box relative to the main frame's viewport.
    gfx::Rect rect_in_viewport =
        r->AbsoluteBoundingBoxRect(kTraverseDocumentBoundaries);

    // Exclude image ads that are invisible or too small (e.g. tracking pixels).
    if (rect_in_viewport.width() > 1 && rect_in_viewport.height() > 1) {
      if (!ad_use_counter_recorded_) {
        // Currently, only image element is supported.
        CHECK(IsA<HTMLImageElement>(*element_));

        UseCounter::Count(element_->GetDocument(), WebFeature::kImageAd);

        ad_use_counter_recorded_ = true;
      }

      // Maps the rectangle from its coordinates within the viewport's
      // coordinate system to the document's coordinate system.
      rect_to_report =
          rect_in_viewport +
          local_root_main_frame.View()->LayoutViewport()->ScrollOffsetInt();
    }
  }

  if (last_reported_rect_ != rect_to_report) {
    local_root_main_frame.Client()->OnMainFrameImageAdRectangleChanged(
        element_->GetDomNodeId(), rect_to_report);
    last_reported_rect_ = rect_to_report;
  }
}

void DisplayAdElementMonitor::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
}

}  // namespace blink
