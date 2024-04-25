// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_root_painter.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/svg_foreign_object_painter.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

bool ShouldApplySnappingScaleAdjustment(const LayoutSVGRoot& layout_svg_root) {
  // If the RuntimeEnabledFeatures flag isn't set then apply scale adjustment.
  if (!RuntimeEnabledFeatures::SvgNoPixelSnappingScaleAdjustmentEnabled()) {
    return true;
  }
  // Apply scale adjustment if the SVG root is the document root - i.e it is
  // not an inline SVG.
  return layout_svg_root.IsDocumentElement();
}

}  // namespace

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
  if (ShouldApplySnappingScaleAdjustment(layout_svg_root_)) {
    PhysicalSize size = layout_svg_root_.Size();
    if (!size.IsEmpty()) {
      paint_offset_to_border_box.Scale(
          snapped_size.width() / size.width.ToFloat(),
          snapped_size.height() / size.height.ToFloat());
    }
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

  if (paint_info.DescendantPaintingBlocked()) {
    return;
  }

  for (LayoutObject* child = layout_svg_root_.FirstChild(); child;
       child = child->NextSibling()) {
    if (auto* foreign_object = DynamicTo<LayoutSVGForeignObject>(child)) {
      SVGForeignObjectPainter(*foreign_object).PaintLayer(paint_info);
    } else {
      child->Paint(paint_info);
    }
  }
}

}  // namespace blink
