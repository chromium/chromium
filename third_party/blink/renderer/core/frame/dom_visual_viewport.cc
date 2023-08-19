/*
 * Copyright (C) 2016 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/dom_visual_viewport.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"

namespace blink {

DOMVisualViewport::DOMVisualViewport(LocalDOMWindow* window)
    : window_(window) {}

DOMVisualViewport::~DOMVisualViewport() = default;

void DOMVisualViewport::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  EventTarget::Trace(visitor);
}

const AtomicString& DOMVisualViewport::InterfaceName() const {
  return event_target_names::kVisualViewport;
}

ExecutionContext* DOMVisualViewport::GetExecutionContext() const {
  return window_->GetExecutionContext();
}

float DOMVisualViewport::offsetLeft() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame || !frame->IsOutermostMainFrame())
    return 0;

  if (Page* page = frame->GetPage())
    return page->GetVisualViewport().OffsetLeft();

  return 0;
}

float DOMVisualViewport::offsetTop() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame || !frame->IsOutermostMainFrame())
    return 0;

  if (Page* page = frame->GetPage())
    return page->GetVisualViewport().OffsetTop();

  return 0;
}

float DOMVisualViewport::pageLeft() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame)
    return 0;

  Page* page = frame->GetPage();
  if (!page)
    return 0;

  LocalFrameView* view = frame->View();
  if (!view || !view->LayoutViewport())
    return 0;

  frame->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  float viewport_x = view->LayoutViewport()->GetScrollOffset().x();

  if (frame->IsMainFrame() && page->GetVisualViewport().IsActiveViewport())
    viewport_x += page->GetVisualViewport().GetScrollOffset().x();

  return AdjustForAbsoluteZoom::AdjustScroll(viewport_x,
                                             frame->PageZoomFactor());
}

float DOMVisualViewport::pageTop() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame)
    return 0;

  Page* page = frame->GetPage();
  if (!page)
    return 0;

  LocalFrameView* view = frame->View();
  if (!view || !view->LayoutViewport())
    return 0;

  frame->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  float viewport_y = view->LayoutViewport()->GetScrollOffset().y();

  if (frame->IsMainFrame() && page->GetVisualViewport().IsActiveViewport())
    viewport_y += page->GetVisualViewport().GetScrollOffset().y();

  return AdjustForAbsoluteZoom::AdjustScroll(viewport_y,
                                             frame->PageZoomFactor());
}

double DOMVisualViewport::width() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame)
    return 0;

  if (!frame->IsOutermostMainFrame()) {
    // Update layout to ensure scrollbars are up-to-date.
    frame->GetDocument()->UpdateStyleAndLayout(
        DocumentUpdateReason::kJavaScript);
    auto* scrollable_area = frame->View()->LayoutViewport();
    float width =
        scrollable_area->VisibleContentRect(kExcludeScrollbars).width();
    return AdjustForAbsoluteZoom::AdjustInt(ClampTo<int>(ceilf(width)),
                                            frame->PageZoomFactor());
  }

  if (Page* page = frame->GetPage())
    return page->GetVisualViewport().Width();

  return 0;
}

double DOMVisualViewport::height() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame)
    return 0;

  if (!frame->IsOutermostMainFrame()) {
    // Update layout to ensure scrollbars are up-to-date.
    frame->GetDocument()->UpdateStyleAndLayout(
        DocumentUpdateReason::kJavaScript);
    auto* scrollable_area = frame->View()->LayoutViewport();
    float height =
        scrollable_area->VisibleContentRect(kExcludeScrollbars).height();
    return AdjustForAbsoluteZoom::AdjustInt(ClampTo<int>(ceilf(height)),
                                            frame->PageZoomFactor());
  }

  if (Page* page = frame->GetPage())
    return page->GetVisualViewport().Height();

  return 0;
}

double DOMVisualViewport::scale() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame)
    return 0;

  if (!frame->IsOutermostMainFrame())
    return 1;

  if (Page* page = window_->GetFrame()->GetPage())
    return page->GetVisualViewport().ScaleForVisualViewport();

  return 0;
}

absl::optional<HeapVector<Member<DOMRect>>> DOMVisualViewport::segments()
    const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame || !frame->IsOutermostMainFrame())
    return absl::nullopt;

  WebVector<gfx::Rect> web_segments =
      frame->GetWidgetForLocalRoot()->WindowSegments();

  // If there is a single segment, return null as authors should use other
  // properties on VisualViewport to determine the size.
  if (web_segments.size() <= 1)
    return absl::nullopt;

  // The rect passed to us from content is in DIP, relative to the main
  // frame/widget. This doesn't take the page's zoom factor into account so we
  // must scale by the inverse of the page zoom in order to get correct client
  // coordinates.
  // WindowToViewportScalar is the device scale factor, and PageZoomFactor is
  // the combination of the device scale factor and the zoom percent of the
  // page.
  HeapVector<Member<DOMRect>> viewport_segments;
  const float dips_to_blink =
      frame->GetWidgetForLocalRoot()->DIPsToBlinkSpace(1.0f);
  const float page_zoom_factor = frame->PageZoomFactor();
  const float scale_factor = dips_to_blink / page_zoom_factor;
  for (auto const& web_segment : web_segments) {
    gfx::QuadF quad((gfx::RectF(web_segment)));
    quad.Scale(scale_factor);
    viewport_segments.push_back(DOMRect::FromRectF(quad.BoundingBox()));
  }

  return viewport_segments;
}

}  // namespace blink
