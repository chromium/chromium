// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_fieldset_painter.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/fieldset_paint_info.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void NGFieldsetPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  const NGLink* legend = nullptr;
  if (!fieldset_.Children().empty()) {
    const auto& first_child = fieldset_.Children().front();
    if (first_child->IsRenderedLegend())
      legend = &first_child;
  }

  // Paint the fieldset (background, other decorations, and) border, with the
  // cutout hole for the legend.
  PaintFieldsetDecorationBackground(legend, paint_info, paint_offset);

  // Proceed to painting the legend. According to the spec, it should be done as
  // part of the border phase.
  if (legend)
    PaintLegend(To<NGPhysicalBoxFragment>(**legend), paint_info);
}

void NGFieldsetPainter::PaintFieldsetDecorationBackground(
    const NGLink* legend,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  PhysicalSize fieldset_size(fieldset_.Size());
  PhysicalRect paint_rect(paint_offset, fieldset_size);
  const auto& fragment = fieldset_;
  BoxDecorationData box_decoration_data(paint_info, fragment);
  if (!box_decoration_data.ShouldPaint())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, *fieldset_.GetLayoutObject(), paint_info.phase))
    return;

  LayoutRectOutsets fieldset_borders = fragment.Borders().ToLayoutRectOutsets();
  const ComputedStyle& style = fieldset_.Style();
  PhysicalRect legend_border_box;
  if (legend) {
    legend_border_box.offset = legend->Offset();
    legend_border_box.size = (*legend)->Size();
  }
  FieldsetPaintInfo fieldset_paint_info(style, fieldset_size, fieldset_borders,
                                        legend_border_box);
  PhysicalRect contracted_rect(paint_rect);
  contracted_rect.Contract(fieldset_paint_info.border_outsets);

  DrawingRecorder recorder(paint_info.context, *fieldset_.GetLayoutObject(),
                           paint_info.phase);

  NGBoxFragmentPainter fragment_painter(fieldset_);
  if (box_decoration_data.ShouldPaintShadow()) {
    fragment_painter.PaintNormalBoxShadow(paint_info, contracted_rect, style);
  }
  if (box_decoration_data.ShouldPaintBackground()) {
    // TODO(eae): Switch to LayoutNG version of BackgroundImageGeometry.
    BackgroundImageGeometry geometry(
        *static_cast<const LayoutBoxModelObject*>(fieldset_.GetLayoutObject()));
    fragment_painter.PaintFillLayers(
        paint_info, box_decoration_data.BackgroundColor(),
        style.BackgroundLayers(), contracted_rect, geometry);
  }
  if (box_decoration_data.ShouldPaintShadow()) {
    fragment_painter.PaintInsetBoxShadowWithBorderRect(
        paint_info, contracted_rect, fieldset_.Style());
  }
  if (box_decoration_data.ShouldPaintBorder()) {
    // Create a clipping region around the legend and paint the border as
    // normal.
    GraphicsContext& graphics_context = paint_info.context;
    GraphicsContextStateSaver state_saver(graphics_context);

    PhysicalRect legend_cutout_rect = fieldset_paint_info.legend_cutout_rect;
    legend_cutout_rect.Move(paint_rect.offset);
    graphics_context.ClipOut(PixelSnappedIntRect(legend_cutout_rect));

    const LayoutObject* layout_object = fieldset_.GetLayoutObject();
    Node* node = layout_object->GeneratingNode();
    fragment_painter.PaintBorder(*fieldset_.GetLayoutObject(),
                                 layout_object->GetDocument(), node, paint_info,
                                 contracted_rect, fieldset_.Style());
  }
}

void NGFieldsetPainter::PaintLegend(const NGPhysicalBoxFragment& legend,
                                    const PaintInfo& paint_info) {
  // Unless the legend establishes its own self-painting layer, paint the legend
  // as part of the border phase, according to spec.
  const LayoutObject* legend_object = legend.GetLayoutObject();
  if (ToLayoutBox(legend_object)->HasSelfPaintingLayer())
    return;
  PaintInfo legend_paint_info = paint_info;
  legend_paint_info.phase = PaintPhase::kForeground;
  ObjectPainter(*legend_object).PaintAllPhasesAtomically(legend_paint_info);
}

}  // namespace blink
