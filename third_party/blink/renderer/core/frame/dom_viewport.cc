// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/dom_viewport.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"

namespace blink {

DOMViewport::DOMViewport(LocalDOMWindow* window) : window_(window) {}

DOMViewport::~DOMViewport() = default;

void DOMViewport::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  ScriptWrappable::Trace(visitor);
}

std::optional<HeapVector<Member<DOMRect>>> DOMViewport::segments() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame || !frame->IsOutermostMainFrame()) {
    return std::nullopt;
  }

  UseCounter::Count(frame->GetDocument(), WebFeature::kFoldableAPIs);

  WebVector<gfx::Rect> web_segments =
      frame->GetWidgetForLocalRoot()->ViewportSegments();

  // If there is a single segment, return null as authors should use other
  // properties on VisualViewport to determine the size.
  if (web_segments.size() <= 1) {
    return std::nullopt;
  }

  // The rect passed to us from content is in DIP, relative to the main
  // frame/widget. This doesn't take the page's zoom factor into account so we
  // must scale by the inverse of the page zoom in order to get correct client
  // coordinates.
  // WindowToViewportScalar is the device scale factor, and LayoutZoomFactor is
  // the combination of the device scale factor and the zoom percent of the
  // page.
  HeapVector<Member<DOMRect>> viewport_segments;
  const float dips_to_blink =
      frame->GetWidgetForLocalRoot()->DIPsToBlinkSpace(1.0f);
  const float zoom_factor = frame->LayoutZoomFactor();
  const float scale_factor = dips_to_blink / zoom_factor;
  for (auto const& web_segment : web_segments) {
    gfx::QuadF quad((gfx::RectF(web_segment)));
    quad.Scale(scale_factor);
    viewport_segments.push_back(DOMRect::FromRectF(quad.BoundingBox()));
  }

  return viewport_segments;
}

}  // namespace blink
