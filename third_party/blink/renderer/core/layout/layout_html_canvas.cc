/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/html_canvas_painter.h"

namespace blink {

LayoutHTMLCanvas::LayoutHTMLCanvas(HTMLCanvasElement* element)
    : LayoutReplaced(element, PhysicalSize(element->Size())) {
  View()->GetFrameView()->SetIsVisuallyNonEmpty();
}

void LayoutHTMLCanvas::PaintReplaced(const PaintInfo& paint_info,
                                     const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  if (ChildPaintBlockedByDisplayLock()) {
    return;
  }
  HTMLCanvasPainter(*this).PaintReplaced(paint_info, paint_offset);
}

void LayoutHTMLCanvas::CanvasSizeChanged() {
  NOT_DESTROYED();
  gfx::Size canvas_size = To<HTMLCanvasElement>(GetNode())->Size();
  PhysicalSize zoomed_size = PhysicalSize(canvas_size);
  zoomed_size.Scale(StyleRef().EffectiveZoom());

  if (zoomed_size == IntrinsicSize())
    return;

  SetIntrinsicSize(zoomed_size);

  if (!Parent())
    return;

  SetIntrinsicLogicalWidthsDirty();
  SetNeedsLayout(layout_invalidation_reason::kSizeChanged);
}

bool LayoutHTMLCanvas::DrawsBackgroundOntoContentLayer() const {
  auto* canvas = To<HTMLCanvasElement>(GetNode());
  if (canvas->SurfaceLayerBridge())
    return false;
  CanvasRenderingContext* context = canvas->RenderingContext();
  if (!context || !context->IsComposited() || !context->CcLayer())
    return false;
  if (StyleRef().HasBoxDecorations() || StyleRef().HasBackgroundImage())
    return false;
  // If there is no background, there is nothing to support.
  if (!StyleRef().HasBackground())
    return true;
  // Simple background that is contained within the contents rect.
  return ReplacedContentRect().Contains(
      PhysicalBackgroundRect(kBackgroundPaintedExtent));
}

void LayoutHTMLCanvas::InvalidatePaint(
    const PaintInvalidatorContext& context) const {
  NOT_DESTROYED();
  auto* element = To<HTMLCanvasElement>(GetNode());
  if (element->IsDirty())
    element->DoDeferredPaintInvalidation();

  LayoutReplaced::InvalidatePaint(context);
}

void LayoutHTMLCanvas::StyleDidChange(StyleDifference diff,
                                      const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutReplaced::StyleDidChange(diff, old_style);
  To<HTMLCanvasElement>(GetNode())->StyleDidChange(old_style, StyleRef());
}

void LayoutHTMLCanvas::WillBeDestroyed() {
  NOT_DESTROYED();
  LayoutReplaced::WillBeDestroyed();
  To<HTMLCanvasElement>(GetNode())->LayoutObjectDestroyed();
}

void LayoutHTMLCanvas::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
  LayoutReplaced::Trace(visitor);
}

bool LayoutHTMLCanvas::IsChildAllowed(LayoutObject* child,
                                      const ComputedStyle& style) const {
  NOT_DESTROYED();
  return IsA<Element>(GetNode()) && !child->IsText() &&
         To<HTMLCanvasElement>(GetNode())->HasPlacedElements() &&
         RuntimeEnabledFeatures::CanvasPlaceElementEnabled();
}

}  // namespace blink
