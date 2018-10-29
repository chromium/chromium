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
    const LayoutPoint paint_offset) {
  const NGPaintFragment* legend = nullptr;
  if (fieldset_.Children().size()) {
    const auto& first_child = fieldset_.Children().front();
    if (first_child.PhysicalFragment().IsRenderedLegend())
      legend = &first_child;
  }

  // Paint the fieldset (background, other decorations, and) border, with the
  // cutout hole for the legend.
  PaintFieldsetDecorationBackground(legend, paint_info, paint_offset);

  // Proceed to painting the legend. According to the spec, it should be done as
  // part of the border phase.
  if (legend)
    PaintLegend(*legend, paint_info);
}

void NGFieldsetPainter::PaintFieldsetDecorationBackground(
    const NGPaintFragment* legend,
    const PaintInfo& paint_info,
    const LayoutPoint paint_offset) {
  LayoutSize fieldset_size(fieldset_.Size().ToLayoutSize());
  LayoutRect paint_rect(paint_offset, fieldset_size);

  if (DrawingRecorder::UseCachedDrawingIfPossible(paint_info.context, fieldset_,
                                                  paint_info.phase))
    return;

  const NGPhysicalBoxFragment& fragment =
      ToNGPhysicalBoxFragment(fieldset_.PhysicalFragment());
  LayoutRectOutsets fieldset_borders = fragment.Borders().ToLayoutRectOutsets();
  const ComputedStyle& style = fieldset_.Style();
  LayoutRect legend_border_box;
  if (legend) {
    legend_border_box.SetLocation(legend->Offset().ToLayoutPoint());
    legend_border_box.SetSize(legend->Size().ToLayoutSize());
  }
  FieldsetPaintInfo fieldset_paint_info(style, fieldset_size, fieldset_borders,
                                        legend_border_box);
  LayoutRect contracted_rect(paint_rect);
  contracted_rect.Contract(fieldset_paint_info.border_outsets);

  DrawingRecorder recorder(paint_info.context, fieldset_, paint_info.phase);
  BoxDecorationData box_decoration_data(fragment);

  NGBoxFragmentPainter fragment_painter(fieldset_);
  fragment_painter.PaintNormalBoxShadow(paint_info, contracted_rect, style);

  // TODO(eae): Switch to LayoutNG version of BackgroundImageGeometry.
  BackgroundImageGeometry geometry(
      *static_cast<const LayoutBoxModelObject*>(fieldset_.GetLayoutObject()));

  fragment_painter.PaintFillLayers(
      paint_info, box_decoration_data.background_color,
      style.BackgroundLayers(), contracted_rect, geometry);
  fragment_painter.PaintInsetBoxShadowWithBorderRect(
      paint_info, contracted_rect, fieldset_.Style());

  if (!box_decoration_data.has_border_decoration)
    return;

  // Create a clipping region around the legend and paint the border as normal.
  GraphicsContext& graphics_context = paint_info.context;
  GraphicsContextStateSaver state_saver(graphics_context);

  LayoutRect legend_cutout_rect = fieldset_paint_info.legend_cutout_rect;
  legend_cutout_rect.MoveBy(paint_rect.Location());
  graphics_context.ClipOut(PixelSnappedIntRect(legend_cutout_rect));

  LayoutObject* layout_object = fieldset_.GetLayoutObject();
  Node* node = layout_object->GeneratingNode();
  fragment_painter.PaintBorder(fieldset_, layout_object->GetDocument(), node,
                               paint_info, contracted_rect, fieldset_.Style());
}

void NGFieldsetPainter::PaintLegend(const NGPaintFragment& legend,
                                    const PaintInfo& paint_info) {
  // Unless the legend establishes its own self-painting layer, paint the legend
  // as part of the border phase, according to spec.
  LayoutObject* legend_object = legend.GetLayoutObject();
  if (ToLayoutBox(legend_object)->HasSelfPaintingLayer())
    return;
  PaintInfo legend_paint_info = paint_info;
  legend_paint_info.phase = PaintPhase::kForeground;
  ObjectPainter(*legend_object).PaintAllPhasesAtomically(legend_paint_info);
}

}  // namespace blink
