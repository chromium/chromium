// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_root_painter.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/svg_foreign_object_painter.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"

namespace blink {

gfx::Rect SVGRootPainter::PixelSnappedSize(
    const PhysicalOffset& paint_offset) const {
  return ToPixelSnappedRect(
      PhysicalRect(paint_offset, layout_svg_root_.Size()));
}

AffineTransform SVGRootPainter::TransformToPixelSnappedBorderBox(
    const PhysicalOffset& paint_offset) const {
  const gfx::Rect snapped_size = PixelSnappedSize(paint_offset);
  AffineTransform paint_offset_to_border_box =
      AffineTransform::Translation(snapped_size.x(), snapped_size.y());
  LayoutSize size = layout_svg_root_.Size();
  if (!size.IsEmpty()) {
    paint_offset_to_border_box.Scale(
        snapped_size.width() / size.Width().ToFloat(),
        snapped_size.height() / size.Height().ToFloat());
  }
  paint_offset_to_border_box.PreConcat(
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
  auto* svg = To<SVGSVGElement>(layout_svg_root_.GetNode());
  DCHECK(svg);
  if (svg->HasEmptyViewBox())
    return;

  ScopedSVGPaintState paint_state(layout_svg_root_, paint_info);

  if (paint_info.DescendantPaintingBlocked()) {
    return;
  }

  PaintInfo child_info(paint_info);
  for (LayoutObject* child = layout_svg_root_.FirstChild(); child;
       child = child->NextSibling()) {
    if (auto* foreign_object = DynamicTo<LayoutNGSVGForeignObject>(child)) {
      SVGForeignObjectPainter(*foreign_object).PaintLayer(paint_info);
    } else {
      child->Paint(child_info);
    }
  }
}

}  // namespace blink
