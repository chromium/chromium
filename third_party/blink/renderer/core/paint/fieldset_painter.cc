// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fieldset_painter.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/relative_utils.h"
#include "third_party/blink/renderer/core/paint/box_background_paint_context.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/fieldset_paint_info.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

FieldsetPaintInfo FieldsetPainter::CreateFieldsetPaintInfo() const {
  const PhysicalFragmentLink* legend = nullptr;
  if (!fieldset_.Children().empty()) {
    const auto& first_child = fieldset_.Children().front();
    if (first_child->IsRenderedLegend())
      legend = &first_child;
  }
  const PhysicalSize fieldset_size(fieldset_.Size());
  const auto& fragment = fieldset_;
  PhysicalBoxStrut fieldset_borders = fragment.Borders();
  const ComputedStyle& style = fieldset_.Style();
  PhysicalRect legend_border_box;
  if (legend) {
    legend_border_box.size = (*legend)->Size();
    // Unapply relative position of the legend.
    // Note that legend->Offset() is the offset after applying
    // position:relative, but the fieldset border painting needs to avoid
    // the legend position with static position.
    //
    // See https://html.spec.whatwg.org/C/#the-fieldset-and-legend-elements
    // > * If the element has a rendered legend, then the border is expected to
    // >   not be painted behind the rectangle defined as follows, using the
    // >   writing mode of the fieldset: ...
    // >    ... at its static position (ignoring transforms), ...
    //
    // The following logic produces wrong results for block direction offsets.
    // However we don't need them.
    const WritingDirectionMode writing_direction = style.GetWritingDirection();
    const LogicalSize logical_fieldset_content_size =
        (fieldset_size -
         PhysicalSize(fieldset_borders.HorizontalSum(),
                      fieldset_borders.VerticalSum()) -
         PhysicalSize(fragment.Padding().HorizontalSum(),
                      fragment.Padding().VerticalSum()))
            .ConvertToLogical(writing_direction.GetWritingMode());
    LogicalOffset relative_offset = ComputeRelativeOffset(
        (*legend)->Style(), writing_direction, logical_fieldset_content_size);
    LogicalOffset legend_logical_offset =
        legend->Offset().ConvertToLogical(writing_direction, fieldset_size,
                                          (*legend)->Size()) -
        relative_offset;
    legend_border_box.offset = legend_logical_offset.ConvertToPhysical(
        writing_direction, fieldset_size, legend_border_box.size);
  }
  return FieldsetPaintInfo(style, fieldset_size, fieldset_borders,
                           legend_border_box);
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
    fragment_painter.PaintNormalBoxShadow(paint_info, contracted_rect, style);
  }

  GraphicsContext& graphics_context = paint_info.context;
  GraphicsContextStateSaver state_saver(graphics_context, false);
  bool needs_end_layer = false;
  if (BleedAvoidanceIsClipping(
          box_decoration_data.GetBackgroundBleedAvoidance())) {
    state_saver.Save();
    FloatRoundedRect border = RoundedBorderGeometry::PixelSnappedRoundedBorder(
        style, contracted_rect, fieldset_.SidesToInclude());
    graphics_context.ClipRoundedRect(border);

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
    fragment_painter.PaintInsetBoxShadowWithBorderRect(
        paint_info, contracted_rect, fieldset_.Style());
  }
  if (box_decoration_data.ShouldPaintBorder()) {
    // Create a clipping region around the legend and paint the border as
    // normal.
    PhysicalRect legend_cutout_rect = fieldset_paint_info.legend_cutout_rect;
    legend_cutout_rect.Move(paint_rect.offset);
    graphics_context.ClipOut(ToPixelSnappedRect(legend_cutout_rect));

    const LayoutObject* layout_object = fieldset_.GetLayoutObject();
    Node* node = layout_object->GeneratingNode();
    fragment_painter.PaintBorder(
        *fieldset_.GetLayoutObject(), layout_object->GetDocument(), node,
        paint_info, contracted_rect, fieldset_.Style(),
        box_decoration_data.GetBackgroundBleedAvoidance(),
        fieldset_.SidesToInclude());
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
