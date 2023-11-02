// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_model_object_painter.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_model_object.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

bool SVGModelObjectPainter::CanUseCullRect(const ComputedStyle& style) {
  // We do not apply cull rect optimizations across transforms for two reasons:
  //   1) Performance: We can optimize transform changes by not repainting.
  //   2) Complexity: Difficulty updating clips when ancestor transforms change.
  // For these reasons, we do not cull painting if there is a transform.
  if (style.HasTransform())
    return false;
  // If the filter "moves pixels" we may require input from outside the cull
  // rect.
  if (style.HasFilter() && style.Filter().HasFilterThatMovesPixels())
    return false;
  return true;
}

void SVGModelObjectPainter::RecordHitTestData(const LayoutObject& svg_object,
                                              const PaintInfo& paint_info) {
  DCHECK(svg_object.IsSVGChild());
  DCHECK(paint_info.phase == PaintPhase::kForeground);
  // Hit test display items are only needed for compositing. This flag is used
  // for for printing and drag images which do not need hit testing.
  if (paint_info.ShouldOmitCompositingInfo())
    return;

  paint_info.context.GetPaintController().RecordHitTestData(
      svg_object,
      gfx::ToEnclosingRect(svg_object.VisualRectInLocalSVGCoordinates()),
      svg_object.EffectiveAllowedTouchAction(),
      svg_object.InsideBlockingWheelEventHandler());
}

void SVGModelObjectPainter::RecordRegionCaptureData(
    const LayoutObject& svg_object,
    const PaintInfo& paint_info) {
  DCHECK(svg_object.IsSVGChild());
  const Element* element = DynamicTo<Element>(svg_object.GetNode());
  if (element) {
    const RegionCaptureCropId* crop_id = element->GetRegionCaptureCropId();
    if (crop_id) {
      paint_info.context.GetPaintController().RecordRegionCaptureData(
          svg_object, *crop_id,
          gfx::ToEnclosingRect(svg_object.VisualRectInLocalSVGCoordinates()));
    }
  }
}

void SVGModelObjectPainter::PaintOutline(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground)
    return;
  if (layout_svg_model_object_.StyleRef().Visibility() != EVisibility::kVisible)
    return;
  if (!layout_svg_model_object_.StyleRef().HasOutline())
    return;

  PaintInfo outline_paint_info(paint_info);
  outline_paint_info.phase = PaintPhase::kSelfOutlineOnly;
  auto visual_rect = layout_svg_model_object_.VisualRectInLocalSVGCoordinates();
  ObjectPainter(layout_svg_model_object_)
      .PaintOutline(outline_paint_info,
                    PhysicalOffset::FromPointFRound(visual_rect.origin()));
}

}  // namespace blink
