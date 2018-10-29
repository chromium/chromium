// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/table_cell_painter.h"

#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void TableCellPainter::PaintContainerBackgroundBehindCell(
    const PaintInfo& paint_info,
    const LayoutObject& background_object) {
  DCHECK(background_object != layout_table_cell_);

  if (layout_table_cell_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  LayoutTable* table = layout_table_cell_.Table();
  if (!table->ShouldCollapseBorders() &&
      layout_table_cell_.StyleRef().EmptyCells() == EEmptyCells::kHide &&
      !layout_table_cell_.FirstChild())
    return;

  ScopedPaintState paint_state(layout_table_cell_, paint_info);
  auto paint_rect =
      PaintRectNotIncludingVisualOverflow(paint_state.PaintOffset());
  PaintBackground(paint_state.GetPaintInfo(), paint_rect, background_object);
}

void TableCellPainter::PaintBackground(const PaintInfo& paint_info,
                                       const LayoutRect& paint_rect,
                                       const LayoutObject& background_object) {
  if (layout_table_cell_.BackgroundTransfersToView())
    return;

  Color c = background_object.ResolveColor(GetCSSPropertyBackgroundColor());
  const FillLayer& bg_layer = background_object.StyleRef().BackgroundLayers();
  if (bg_layer.AnyLayerHasImage() || c.Alpha()) {
    // We have to clip here because the background would paint
    // on top of the borders otherwise.  This only matters for cells and rows.
    bool should_clip = background_object.HasLayer() &&
                       (background_object == layout_table_cell_ ||
                        background_object == layout_table_cell_.Parent()) &&
                       layout_table_cell_.Table()->ShouldCollapseBorders();
    GraphicsContextStateSaver state_saver(paint_info.context, should_clip);
    if (should_clip) {
      LayoutRect clip_rect(paint_rect.Location(), layout_table_cell_.Size());
      clip_rect.Expand(layout_table_cell_.BorderInsets());
      paint_info.context.Clip(PixelSnappedIntRect(clip_rect));
    }
    BackgroundImageGeometry geometry(layout_table_cell_, &background_object);
    BoxModelObjectPainter(layout_table_cell_)
        .PaintFillLayers(paint_info, c, bg_layer, paint_rect, geometry,
                         kBackgroundBleedNone, SkBlendMode::kSrcOver);
  }
}

void TableCellPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  LayoutTable* table = layout_table_cell_.Table();
  const ComputedStyle& style = layout_table_cell_.StyleRef();
  if (!table->ShouldCollapseBorders() &&
      style.EmptyCells() == EEmptyCells::kHide &&
      !layout_table_cell_.FirstChild())
    return;

  bool has_background = style.HasBackground();
  bool has_box_shadow = style.BoxShadow();
  bool needs_to_paint_border =
      style.HasBorderDecoration() && !table->ShouldCollapseBorders();
  if (has_background || has_box_shadow || needs_to_paint_border) {
    if (!DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, layout_table_cell_,
            DisplayItem::kBoxDecorationBackground)) {
      // TODO(chrishtr): the pixel-snapping here is likely incorrect.
      DrawingRecorder recorder(paint_info.context, layout_table_cell_,
                               DisplayItem::kBoxDecorationBackground);

      LayoutRect paint_rect = PaintRectNotIncludingVisualOverflow(paint_offset);

      if (has_box_shadow)
        BoxPainterBase::PaintNormalBoxShadow(paint_info, paint_rect, style);

      if (has_background)
        PaintBackground(paint_info, paint_rect, layout_table_cell_);

      if (has_box_shadow) {
        // If the table collapses borders, the inner rect is the border box rect
        // inset by inner half widths of collapsed borders (which are returned
        // from the overriden BorderXXX() methods). Otherwise the following code
        // is equivalent to BoxPainterBase::PaintInsetBoxShadowWithBorderRect().
        auto inner_rect = paint_rect;
        inner_rect.ContractEdges(
            layout_table_cell_.BorderTop(), layout_table_cell_.BorderRight(),
            layout_table_cell_.BorderBottom(), layout_table_cell_.BorderLeft());
        BoxPainterBase::PaintInsetBoxShadowWithInnerRect(
            paint_info, inner_rect, layout_table_cell_.StyleRef());
      }

      if (needs_to_paint_border) {
        BoxPainterBase::PaintBorder(
            layout_table_cell_, layout_table_cell_.GetDocument(),
            layout_table_cell_.GeneratingNode(), paint_info, paint_rect, style);
      }
    }
  }

  if (RuntimeEnabledFeatures::PaintTouchActionRectsEnabled()) {
    LayoutRect rect = PaintRectNotIncludingVisualOverflow(paint_offset);
    BoxPainter(layout_table_cell_)
        .RecordHitTestData(paint_info, paint_offset, rect);
  }
}

void TableCellPainter::PaintMask(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  if (layout_table_cell_.StyleRef().Visibility() != EVisibility::kVisible ||
      paint_info.phase != PaintPhase::kMask)
    return;

  LayoutTable* table_elt = layout_table_cell_.Table();
  if (!table_elt->ShouldCollapseBorders() &&
      layout_table_cell_.StyleRef().EmptyCells() == EEmptyCells::kHide &&
      !layout_table_cell_.FirstChild())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_cell_, paint_info.phase))
    return;

  DrawingRecorder recorder(paint_info.context, layout_table_cell_,
                           paint_info.phase);
  LayoutRect paint_rect = PaintRectNotIncludingVisualOverflow(paint_offset);
  BoxPainter(layout_table_cell_).PaintMaskImages(paint_info, paint_rect);
}

// TODO(crbug.com/377847): When table cells fully support subpixel layout, we
// should not snap the size to pixels here. We should remove this function and
// snap to pixels for the rect with paint offset applied.
LayoutRect TableCellPainter::PaintRectNotIncludingVisualOverflow(
    const LayoutPoint& paint_offset) {
  return LayoutRect(paint_offset,
                    LayoutSize(layout_table_cell_.PixelSnappedSize()));
}

}  // namespace blink
