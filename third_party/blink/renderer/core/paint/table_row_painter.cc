// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/table_row_painter.h"

#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/collapsed_border_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/table_cell_painter.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/hit_test_display_item.h"

namespace blink {

void TableRowPainter::Paint(const PaintInfo& paint_info) {
  DCHECK(layout_table_row_.HasSelfPaintingLayer());

  // TODO(crbug.com/805514): Paint mask for table row.
  if (paint_info.phase == PaintPhase::kMask)
    return;

  // TODO(crbug.com/577282): This painting order is inconsistent with other
  // outlines.
  if (ShouldPaintSelfOutline(paint_info.phase))
    PaintOutline(paint_info);
  if (paint_info.phase == PaintPhase::kSelfOutlineOnly)
    return;

  if (ShouldPaintSelfBlockBackground(paint_info.phase)) {
    PaintBoxDecorationBackground(
        paint_info,
        layout_table_row_.Section()->FullTableEffectiveColumnSpan());
  }

  if (paint_info.phase == PaintPhase::kSelfBlockBackgroundOnly)
    return;

  PaintInfo paint_info_for_cells = paint_info.ForDescendants();
  for (LayoutTableCell* cell = layout_table_row_.FirstCell(); cell;
       cell = cell->NextCell()) {
    if (!cell->HasSelfPaintingLayer())
      cell->Paint(paint_info_for_cells);
  }
}

void TableRowPainter::PaintOutline(const PaintInfo& paint_info) {
  DCHECK(ShouldPaintSelfOutline(paint_info.phase));
  ScopedPaintState paint_state(layout_table_row_, paint_info);
  ObjectPainter(layout_table_row_)
      .PaintOutline(paint_state.GetPaintInfo(), paint_state.PaintOffset());
}

void TableRowPainter::HandleChangedPartialPaint(
    const PaintInfo& paint_info,
    const CellSpan& dirtied_columns) {
  PaintResult paint_result =
      dirtied_columns ==
              layout_table_row_.Section()->FullTableEffectiveColumnSpan()
          ? kFullyPainted
          : kMayBeClippedByCullRect;
  layout_table_row_.GetMutableForPainting().UpdatePaintResult(
      paint_result, paint_info.GetCullRect());
}

void TableRowPainter::RecordHitTestData(const PaintInfo& paint_info,
                                        const PhysicalOffset& paint_offset) {
  // Hit test display items are only needed for compositing. This flag is used
  // for for printing and drag images which do not need hit testing.
  if (paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers)
    return;

  // If an object is not visible, it does not participate in hit testing.
  if (layout_table_row_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  auto touch_action = layout_table_row_.EffectiveAllowedTouchAction();
  if (touch_action == TouchAction::kTouchActionAuto)
    return;

  auto rect = layout_table_row_.PhysicalBorderBoxRect();
  rect.offset += paint_offset;
  HitTestDisplayItem::Record(paint_info.context, layout_table_row_,
                             HitTestRect(rect.ToLayoutRect(), touch_action));
}

void TableRowPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const CellSpan& dirtied_columns) {
  ScopedPaintState paint_state(layout_table_row_, paint_info);
  const auto& local_paint_info = paint_state.GetPaintInfo();
  auto paint_offset = paint_state.PaintOffset();
  RecordHitTestData(local_paint_info, paint_offset);

  bool has_background = layout_table_row_.StyleRef().HasBackground();
  bool has_box_shadow = layout_table_row_.StyleRef().BoxShadow();
  if (!has_background && !has_box_shadow)
    return;

  HandleChangedPartialPaint(paint_info, dirtied_columns);

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_row_,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(local_paint_info.context, layout_table_row_,
                           DisplayItem::kBoxDecorationBackground);
  PhysicalRect paint_rect(paint_offset, layout_table_row_.Size());

  if (has_box_shadow) {
    BoxPainterBase::PaintNormalBoxShadow(local_paint_info, paint_rect,
                                         layout_table_row_.StyleRef());
  }

  if (has_background) {
    const auto* section = layout_table_row_.Section();
    PaintInfo paint_info_for_cells = local_paint_info.ForDescendants();
    for (auto c = dirtied_columns.Start(); c < dirtied_columns.End(); c++) {
      if (const auto* cell =
              section->OriginatingCellAt(layout_table_row_.RowIndex(), c)) {
        TableCellPainter(*cell).PaintContainerBackgroundBehindCell(
            paint_info_for_cells, layout_table_row_);
      }
    }
  }

  if (has_box_shadow) {
    BoxPainterBase::PaintInsetBoxShadowWithInnerRect(
        local_paint_info, paint_rect, layout_table_row_.StyleRef());
  }
}

void TableRowPainter::PaintCollapsedBorders(const PaintInfo& paint_info,
                                            const CellSpan& dirtied_columns) {
  ScopedPaintState paint_state(layout_table_row_, paint_info);
  base::Optional<DrawingRecorder> recorder;

  if (LIKELY(!layout_table_row_.Table()->ShouldPaintAllCollapsedBorders())) {
    HandleChangedPartialPaint(paint_info, dirtied_columns);

    if (DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, layout_table_row_,
            DisplayItem::kTableCollapsedBorders))
      return;

    recorder.emplace(paint_info.context, layout_table_row_,
                     DisplayItem::kTableCollapsedBorders);
  }
  // Otherwise TablePainter should have created the drawing recorder.

  const auto* section = layout_table_row_.Section();
  unsigned row = layout_table_row_.RowIndex();
  for (unsigned c = std::min(dirtied_columns.End(), section->NumCols(row));
       c > dirtied_columns.Start(); c--) {
    if (const auto* cell = section->OriginatingCellAt(row, c - 1)) {
      CollapsedBorderPainter(*cell).PaintCollapsedBorders(
          paint_state.GetPaintInfo());
    }
  }
}

}  // namespace blink
