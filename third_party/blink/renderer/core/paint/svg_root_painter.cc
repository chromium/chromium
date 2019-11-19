// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_root_painter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"

namespace blink {

IntRect SVGRootPainter::PixelSnappedSize(
    const PhysicalOffset& paint_offset) const {
  return PixelSnappedIntRect(
      PhysicalRect(paint_offset, layout_svg_root_.Size()));
}

AffineTransform SVGRootPainter::TransformToPixelSnappedBorderBox(
    const PhysicalOffset& paint_offset) const {
  const IntRect snapped_size = PixelSnappedSize(paint_offset);
  AffineTransform paint_offset_to_border_box =
      AffineTransform::Translation(snapped_size.X(), snapped_size.Y());
  LayoutSize size = layout_svg_root_.Size();
  if (!size.IsEmpty()) {
    paint_offset_to_border_box.Scale(
        snapped_size.Width() / size.Width().ToFloat(),
        snapped_size.Height() / size.Height().ToFloat());
  }
  paint_offset_to_border_box.Multiply(
      layout_svg_root_.LocalToBorderBoxTransform());
  return paint_offset_to_border_box;
}

void SVGRootPainter::PaintReplaced(const PaintInfo& paint_info,
                                   const PhysicalOffset& paint_offset) {
  // An empty viewport disables rendering.
  if (PixelSnappedSize(paint_offset).IsEmpty())
    return;

  // An empty viewBox also disables rendering.
  // (http://www.w3.org/TR/SVG/coords.html#ViewBoxAttribute)
  SVGSVGElement* svg = ToSVGSVGElement(layout_svg_root_.GetNode());
  DCHECK(svg);
  if (svg->HasEmptyViewBox())
    return;

  ScopedSVGPaintState paint_state(layout_svg_root_, paint_info);
  if (paint_state.GetPaintInfo().phase == PaintPhase::kForeground &&
      !paint_state.ApplyClipMaskAndFilterIfNecessary())
    return;

  BoxPainter(layout_svg_root_).PaintChildren(paint_state.GetPaintInfo());

  PaintTiming& timing = PaintTiming::From(
      layout_svg_root_.GetNode()->GetDocument().TopDocument());
  timing.MarkFirstContentfulPaint();
}

}  // namespace blink
