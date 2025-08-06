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

namespace blink {

DisplayAdElementMonitor::DisplayAdElementMonitor(Element* element)
    : element_(element) {
  DCHECK(element_);
  EnsureStarted();
}

void DisplayAdElementMonitor::EnsureStarted() {
  if (started_ || !element_->GetDocument().View()) {
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
  // Scope to the outermost frame to avoid counting image ads that are (likely)
  // already in ad iframes.
  LocalFrame* frame = element_->GetDocument().GetFrame();
  if (!frame || !frame->View() || !frame->IsOutermostMainFrame()) {
    return;
  }

  gfx::Rect rect_to_report;
  if (LayoutObject* r = element_->GetLayoutObject()) {
    gfx::Rect rect_in_viewport = r->AbsoluteBoundingBoxRect();

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
          rect_in_viewport + frame->View()->LayoutViewport()->ScrollOffsetInt();
    }
  }

  if (last_reported_rect_ != rect_to_report) {
    frame->Client()->OnMainFrameImageAdRectangleChanged(
        element_->GetDomNodeId(), rect_to_report);
    last_reported_rect_ = rect_to_report;
  }
}

void DisplayAdElementMonitor::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
}

}  // namespace blink
