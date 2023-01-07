// Copyright 2014 The Chromium Authors
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

void TableRowPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const CellSpan& dirtied_columns) {
  ScopedPaintState paint_state(layout_table_row_, paint_info);
  const auto& local_paint_info = paint_state.GetPaintInfo();
  auto paint_offset = paint_state.PaintOffset();
  PhysicalRect paint_rect(paint_offset, layout_table_row_.Size());

  BoxPainter(layout_table_row_)
      .RecordHitTestData(local_paint_info, paint_rect, layout_table_row_);
  BoxPainter(layout_table_row_)
      .RecordRegionCaptureData(local_paint_info, paint_rect, layout_table_row_);

  const bool has_background = layout_table_row_.StyleRef().HasBackground();
  const bool has_box_shadow = layout_table_row_.StyleRef().BoxShadow();
  if (!has_background && !has_box_shadow)
    return;

  HandleChangedPartialPaint(paint_info, dirtied_columns);

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_row_,
          DisplayItem::kBoxDecorationBackground))
    return;

  BoxDrawingRecorder recorder(local_paint_info.context, layout_table_row_,
                              DisplayItem::kBoxDecorationBackground,
                              paint_offset);

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
  ScopedPaintState paint_state(
      layout_table_row_, paint_info,
      /*painting_legacy_table_part_in_ancestor_layer*/ true);
  absl::optional<BoxDrawingRecorder> recorder;

  HandleChangedPartialPaint(paint_info, dirtied_columns);

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_row_,
          DisplayItem::kTableCollapsedBorders))
    return;

  recorder.emplace(paint_info.context, layout_table_row_,
                   DisplayItem::kTableCollapsedBorders,
                   paint_state.PaintOffset());

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
