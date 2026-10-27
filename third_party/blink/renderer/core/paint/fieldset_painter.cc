// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fieldset_painter.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/border_shape_utils.h"
#include "third_party/blink/renderer/core/paint/box_background_paint_context.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/contoured_border_geometry.h"
#include "third_party/blink/renderer/core/paint/fieldset_paint_info.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

FieldsetPaintInfo FieldsetPainter::CreateFieldsetPaintInfo() const {
  const PhysicalFragmentLink* legend = nullptr;
  if (!fieldset_.Children().empty()) {
    const auto& first_child = fieldset_.Children().front();
    if (first_child->IsRenderedLegend()) {
      legend = &first_child;
    }
  }

  const WritingDirectionMode writing_direction =
      fieldset_.Style().GetWritingDirection();

  PhysicalRect legend_border_box;
  if (legend) {
    // Remove the position:relative offset of the legend. See:
    // https://html.spec.whatwg.org/C/#the-fieldset-and-legend-elements
    //
    // We can't use the used position-insets directly, we need to select the
    // dominant side first.
    const LogicalOffset relative_offset =
        To<PhysicalBoxFragment>(**legend)
            .UsedPositionInsets()
            .ConvertToLogical(writing_direction)
            .StartOffset();
    const PhysicalBoxStrut relative_insets =
        BoxStrut(relative_offset.inline_offset, -relative_offset.inline_offset,
                 relative_offset.block_offset, -relative_offset.block_offset)
            .ConvertToPhysical(writing_direction);
    legend_border_box = PhysicalRect(
        legend->Offset() - relative_insets.Offset(), (*legend)->Size());
  }
  return FieldsetPaintInfo(fieldset_.Size(), fieldset_.Borders(),
                           legend_border_box, writing_direction);
}

// Paint the fieldset (background, other decorations, and) border, with the
// cutout hole for the legend.
void FieldsetPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const BoxDecorationData& box_decoration_data) {
  DCHECK(box_decoration_data.ShouldPaint());

  const ComputedStyle& style = fieldset_.Style();
  FieldsetPaintInfo fieldset_paint_info = CreateFieldsetPaintInfo();
  PhysicalRect contracted_rect(paint_rect);
  contracted_rect.Contract(fieldset_paint_info.border_outsets);

  BoxFragmentPainter fragment_painter(fieldset_);
  if (box_decoration_data.ShouldPaintShadow()) {
    std::optional<BorderShapeReferenceRects> border_shape_rects =
        ComputeBorderShapeReferenceRects(contracted_rect, fieldset_.Style(),
                                         *fieldset_.GetLayoutObject());
    fragment_painter.PaintNormalBoxShadow(paint_info, contracted_rect, style,
                                          border_shape_rects);
  }

  GraphicsContext& graphics_context = paint_info.context;
  GraphicsContextStateSaver state_saver(graphics_context, false);
  bool needs_end_layer = false;
  if (BleedAvoidanceIsClipping(
          box_decoration_data.GetBackgroundBleedAvoidance())) {
    state_saver.Save();

    ContouredRect border = ContouredBorderGeometry::PixelSnappedContouredBorder(
        style, contracted_rect, fieldset_.SidesToInclude());
    graphics_context.ClipContouredRect(border);

    if (box_decoration_data.GetBackgroundBleedAvoidance() ==
        kBackgroundBleedClipLayer) {
      graphics_context.BeginLayer();
      needs_end_layer = true;
    }
  }

  if (box_decoration_data.ShouldPaintBackground()) {
    // TODO(eae): Switch to LayoutNG version of BoxBackgroundPaintContext.
    BoxBackgroundPaintContext bg_paint_context(
        *static_cast<const LayoutBoxModelObject*>(fieldset_.GetLayoutObject()));
    fragment_painter.PaintFillLayers(
        paint_info, box_decoration_data.BackgroundColor(),
        style.BackgroundLayers(), contracted_rect, bg_paint_context);
  }
  if (box_decoration_data.ShouldPaintShadow()) {
    const LayoutObject* layout_object = fieldset_.GetLayoutObject();
    std::optional<BorderShapeReferenceRects> border_shape_rects =
        ComputeBorderShapeReferenceRects(contracted_rect, fieldset_.Style(),
                                         *layout_object);
    fragment_painter.PaintInsetBoxShadowWithBorderRect(
        paint_info, contracted_rect, fieldset_.Style(), border_shape_rects);
  }
  if (box_decoration_data.ShouldPaintBorder()) {
    // Create a clipping region around the legend and paint the border as
    // normal.
    PhysicalRect legend_cutout_rect = fieldset_paint_info.legend_cutout_rect;
    legend_cutout_rect.Move(paint_rect.offset);
    graphics_context.ClipOut(ToPixelSnappedRect(legend_cutout_rect));

    const LayoutObject* layout_object = fieldset_.GetLayoutObject();
    Node* node = layout_object->GeneratingNode();
    std::optional<BorderShapeReferenceRects> border_shape_rects =
        ComputeBorderShapeReferenceRects(contracted_rect, fieldset_.Style(),
                                         *layout_object);
    fragment_painter.PaintBorder(
        *fieldset_.GetLayoutObject(), layout_object->GetDocument(), node,
        paint_info, contracted_rect, fieldset_.Style(),
        box_decoration_data.GetBackgroundBleedAvoidance(),
        fieldset_.SidesToInclude(),
        border_shape_rects ? &*border_shape_rects : nullptr);
  }

  if (needs_end_layer)
    graphics_context.EndLayer();
}

void FieldsetPainter::PaintMask(const PaintInfo& paint_info,
                                const PhysicalOffset& paint_offset) {
  const LayoutObject& layout_object = *fieldset_.GetLayoutObject();
  BoxFragmentPainter ng_box_painter(fieldset_);
  DrawingRecorder recorder(paint_info.context, layout_object, paint_info.phase,
                           ng_box_painter.VisualRect(paint_offset));
  PhysicalRect paint_rect(paint_offset, fieldset_.Size());
  paint_rect.Contract(CreateFieldsetPaintInfo().border_outsets);
  // TODO(eae): Switch to LayoutNG version of BoxBackgroundPaintContext.
  BoxBackgroundPaintContext bg_paint_context(
      static_cast<const LayoutBoxModelObject&>(layout_object));
  ng_box_painter.PaintMaskImages(paint_info, paint_rect, layout_object,
                                 bg_paint_context, fieldset_.SidesToInclude());
}

}  // namespace blink
