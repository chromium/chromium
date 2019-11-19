// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/table_section_painter.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_col.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/collapsed_border_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/table_cell_painter.h"
#include "third_party/blink/renderer/core/paint/table_row_painter.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void TableSectionPainter::Paint(const PaintInfo& paint_info) {
  // TODO(crbug.com/805514): Paint mask for table section.
  if (paint_info.phase == PaintPhase::kMask)
    return;

  // If the section has multiple fragments, it should repeatedly paint the
  // fragments by itself if:
  // - It's not a self-painting layer (otherwise PaintLayerPainter should
  //   initiate painting of the multiple fragments);
  // - the table doesn't have multiple fragments (otherwise the table's
  //   containing painting layer should initiate painting of the fragments).
  bool should_paint_fragments_by_itself =
      layout_table_section_.FirstFragment().NextFragment() &&
      !layout_table_section_.HasSelfPaintingLayer() &&
      !layout_table_section_.Table()->FirstFragment().NextFragment();

  if (!should_paint_fragments_by_itself) {
    PaintSection(paint_info);
    return;
  }

  for (const auto* fragment = &layout_table_section_.FirstFragment(); fragment;
       fragment = fragment->NextFragment()) {
    PaintInfo fragment_paint_info = paint_info;
    fragment_paint_info.SetFragmentLogicalTopInFlowThread(
        fragment->LogicalTopInFlowThread());
    PaintSection(fragment_paint_info);
  }
}

void TableSectionPainter::PaintSection(const PaintInfo& paint_info) {
  DCHECK(!layout_table_section_.NeedsLayout());
  // avoid crashing on bugs that cause us to paint with dirty layout
  if (layout_table_section_.NeedsLayout())
    return;

  unsigned total_rows = layout_table_section_.NumRows();
  unsigned total_cols = layout_table_section_.Table()->NumEffectiveColumns();

  if (!total_rows || !total_cols)
    return;

  ScopedPaintState paint_state(layout_table_section_, paint_info);
  const auto& local_paint_info = paint_state.GetPaintInfo();
  auto paint_offset = paint_state.PaintOffset();

  if (local_paint_info.phase != PaintPhase::kSelfOutlineOnly) {
    if (local_paint_info.phase != PaintPhase::kSelfBlockBackgroundOnly &&
        local_paint_info.phase != PaintPhase::kMask) {
      ScopedBoxContentsPaintState contents_paint_state(paint_state,
                                                       layout_table_section_);
      PaintObject(contents_paint_state.GetPaintInfo(),
                  contents_paint_state.PaintOffset());
    } else {
      PaintObject(local_paint_info, paint_offset);
    }
  }

  if (ShouldPaintSelfOutline(local_paint_info.phase)) {
    ObjectPainter(layout_table_section_)
        .PaintOutline(local_paint_info, paint_offset);
  }
}

void TableSectionPainter::PaintCollapsedBorders(const PaintInfo& paint_info) {
  // If the section has multiple fragments, it should repeatedly paint the
  // fragments for collapsed borders by itself if the table doesn't have
  // multiple fragments (otherwise the table's containing painting layer
  // should initiate painting of the fragments). The condition here is
  // different from that in Paint() because the table always initiate painting
  // of collapsed borders regardless of self-painting status of the section.
  bool should_paint_fragments_by_itself =
      layout_table_section_.FirstFragment().NextFragment() &&
      !layout_table_section_.Table()->FirstFragment().NextFragment();

  if (!should_paint_fragments_by_itself) {
    PaintCollapsedSectionBorders(paint_info);
    return;
  }

  for (const auto* fragment = &layout_table_section_.FirstFragment(); fragment;
       fragment = fragment->NextFragment()) {
    PaintInfo fragment_paint_info = paint_info;
    fragment_paint_info.SetFragmentLogicalTopInFlowThread(
        fragment->LogicalTopInFlowThread());
    PaintCollapsedSectionBorders(fragment_paint_info);
  }
}

LayoutRect TableSectionPainter::TableAlignedRect(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  PhysicalRect local_cull_rect(paint_info.GetCullRect().Rect());
  local_cull_rect.offset -= paint_offset;

  LayoutRect table_aligned_rect =
      layout_table_section_.LogicalRectForWritingModeAndDirection(
          local_cull_rect);
  return table_aligned_rect;
}

void TableSectionPainter::PaintCollapsedSectionBorders(
    const PaintInfo& paint_info) {
  if (!layout_table_section_.NumRows() ||
      !layout_table_section_.Table()->EffectiveColumns().size())
    return;

  ScopedPaintState paint_state(layout_table_section_, paint_info);
  base::Optional<ScopedBoxContentsPaintState> contents_paint_state;
  if (paint_info.phase != PaintPhase::kMask)
    contents_paint_state.emplace(paint_state, layout_table_section_);
  const auto& local_paint_info = contents_paint_state
                                     ? contents_paint_state->GetPaintInfo()
                                     : paint_state.GetPaintInfo();
  auto paint_offset = contents_paint_state ? contents_paint_state->PaintOffset()
                                           : paint_state.PaintOffset();

  CellSpan dirtied_rows;
  CellSpan dirtied_columns;
  if (UNLIKELY(
          layout_table_section_.Table()->ShouldPaintAllCollapsedBorders())) {
    // Ignore paint cull rect to simplify paint invalidation in such rare case.
    dirtied_rows = layout_table_section_.FullSectionRowSpan();
    dirtied_columns = layout_table_section_.FullTableEffectiveColumnSpan();
  } else {
    layout_table_section_.DirtiedRowsAndEffectiveColumns(
        TableAlignedRect(local_paint_info, paint_offset), dirtied_rows,
        dirtied_columns);
  }

  if (dirtied_columns.Start() >= dirtied_columns.End())
    return;

  // Collapsed borders are painted from the bottom right to the top left so that
  // precedence due to cell position is respected.
  for (unsigned r = dirtied_rows.End(); r > dirtied_rows.Start(); r--) {
    if (const auto* row = layout_table_section_.RowLayoutObjectAt(r - 1)) {
      TableRowPainter(*row).PaintCollapsedBorders(local_paint_info,
                                                  dirtied_columns);
    }
  }
}

void TableSectionPainter::PaintObject(const PaintInfo& paint_info,
                                      const PhysicalOffset& paint_offset) {
  CellSpan dirtied_rows;
  CellSpan dirtied_columns;
  layout_table_section_.DirtiedRowsAndEffectiveColumns(
      TableAlignedRect(paint_info, paint_offset), dirtied_rows,
      dirtied_columns);

  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();

  if (ShouldPaintSelfBlockBackground(paint_info.phase)) {
    PaintBoxDecorationBackground(paint_info, paint_offset, dirtied_rows,
                                 dirtied_columns);
  }

  if (paint_info.phase == PaintPhase::kSelfBlockBackgroundOnly)
    return;

  if (ShouldPaintDescendantBlockBackgrounds(paint_info.phase)) {
    for (unsigned r = dirtied_rows.Start(); r < dirtied_rows.End(); r++) {
      const LayoutTableRow* row = layout_table_section_.RowLayoutObjectAt(r);
      // If a row has a layer, we'll paint row background though
      // TableRowPainter::paint().
      if (!row || row->HasSelfPaintingLayer())
        continue;
      TableRowPainter(*row).PaintBoxDecorationBackground(
          paint_info_for_descendants, dirtied_columns);
    }
  }

  // This is tested after background painting because during background painting
  // we need to check validity of the previous background display item based on
  // dirtyRows and dirtyColumns.
  if (dirtied_rows.Start() >= dirtied_rows.End() ||
      dirtied_columns.Start() >= dirtied_columns.End())
    return;

  const auto& visually_overflowing_cells =
      layout_table_section_.VisuallyOverflowingCells();
  if (visually_overflowing_cells.IsEmpty()) {
    // This path is for 2 cases:
    // 1. Normal partial paint, without overflowing cells;
    // 2. Full paint, for small sections or big sections with many overflowing
    //    cells.
    // The difference between the normal partial paint and full paint is that
    // whether dirtied_rows and dirtied_columns cover the whole section.
    DCHECK(!layout_table_section_.HasVisuallyOverflowingCell() ||
           (dirtied_rows == layout_table_section_.FullSectionRowSpan() &&
            dirtied_columns ==
                layout_table_section_.FullTableEffectiveColumnSpan()));

    for (unsigned r = dirtied_rows.Start(); r < dirtied_rows.End(); r++) {
      const LayoutTableRow* row = layout_table_section_.RowLayoutObjectAt(r);
      // TODO(crbug.com/577282): This painting order is inconsistent with other
      // outlines.
      if (row && !row->HasSelfPaintingLayer() &&
          ShouldPaintSelfOutline(paint_info_for_descendants.phase)) {
        TableRowPainter(*row).PaintOutline(paint_info_for_descendants);
      }
      for (unsigned c = dirtied_columns.Start(); c < dirtied_columns.End();
           c++) {
        if (const LayoutTableCell* cell =
                layout_table_section_.OriginatingCellAt(r, c))
          PaintCell(*cell, paint_info_for_descendants);
      }
    }
  } else {
    // This path paints section with a reasonable number of overflowing cells.
    // This is the "partial paint path" for overflowing cells referred in
    // LayoutTableSection::ComputeOverflowFromDescendants().
    Vector<const LayoutTableCell*> cells;
    CopyToVector(visually_overflowing_cells, cells);

    HashSet<const LayoutTableCell*> spanning_cells;
    for (unsigned r = dirtied_rows.Start(); r < dirtied_rows.End(); r++) {
      const LayoutTableRow* row = layout_table_section_.RowLayoutObjectAt(r);
      // TODO(crbug.com/577282): This painting order is inconsistent with other
      // outlines.
      if (row && !row->HasSelfPaintingLayer() &&
          ShouldPaintSelfOutline(paint_info_for_descendants.phase)) {
        TableRowPainter(*row).PaintOutline(paint_info_for_descendants);
      }
      unsigned n_cols = layout_table_section_.NumCols(r);
      for (unsigned c = dirtied_columns.Start();
           c < n_cols && c < dirtied_columns.End(); c++) {
        if (const auto* cell = layout_table_section_.OriginatingCellAt(r, c)) {
          if (!visually_overflowing_cells.Contains(cell))
            cells.push_back(cell);
        }
      }
    }

    // Sort the dirty cells by paint order.
    std::sort(cells.begin(), cells.end(), LayoutTableCell::CompareInDOMOrder);
    for (const auto* cell : cells)
      PaintCell(*cell, paint_info_for_descendants);
  }
}

void TableSectionPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    const CellSpan& dirtied_rows,
    const CellSpan& dirtied_columns) {
  bool may_have_background = layout_table_section_.Table()->HasColElements() ||
                             layout_table_section_.StyleRef().HasBackground();
  bool has_box_shadow = layout_table_section_.StyleRef().BoxShadow();
  if (!may_have_background && !has_box_shadow)
    return;

  PaintResult paint_result =
      dirtied_columns == layout_table_section_.FullTableEffectiveColumnSpan() &&
              dirtied_rows == layout_table_section_.FullSectionRowSpan()
          ? kFullyPainted
          : kMayBeClippedByCullRect;
  layout_table_section_.GetMutableForPainting().UpdatePaintResult(
      paint_result, paint_info.GetCullRect());

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_section_,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(paint_info.context, layout_table_section_,
                           DisplayItem::kBoxDecorationBackground);
  PhysicalRect paint_rect(paint_offset, layout_table_section_.Size());

  if (has_box_shadow) {
    BoxPainterBase::PaintNormalBoxShadow(paint_info, paint_rect,
                                         layout_table_section_.StyleRef());
  }

  if (may_have_background) {
    PaintInfo paint_info_for_cells = paint_info.ForDescendants();
    for (auto r = dirtied_rows.Start(); r < dirtied_rows.End(); r++) {
      for (auto c = dirtied_columns.Start(); c < dirtied_columns.End(); c++) {
        if (const auto* cell = layout_table_section_.OriginatingCellAt(r, c))
          PaintBackgroundsBehindCell(*cell, paint_info_for_cells);
      }
    }
  }

  if (has_box_shadow) {
    BoxPainterBase::PaintInsetBoxShadowWithInnerRect(
        paint_info, paint_rect, layout_table_section_.StyleRef());
  }
}

void TableSectionPainter::PaintBackgroundsBehindCell(
    const LayoutTableCell& cell,
    const PaintInfo& paint_info_for_cells) {
  // We need to handle painting a stack of backgrounds. This stack (from bottom
  // to top) consists of the column group, column, row group, row, and then the
  // cell.

  LayoutTable::ColAndColGroup col_and_col_group =
      layout_table_section_.Table()->ColElementAtAbsoluteColumn(
          cell.AbsoluteColumnIndex());
  LayoutTableCol* column = col_and_col_group.col;
  LayoutTableCol* column_group = col_and_col_group.colgroup;
  TableCellPainter table_cell_painter(cell);

  // Column groups and columns first.
  // FIXME: Columns and column groups do not currently support opacity, and they
  // are being painted "too late" in the stack, since we have already opened a
  // transparency layer (potentially) for the table row group.  Note that we
  // deliberately ignore whether or not the cell has a layer, since these
  // backgrounds paint "behind" the cell.
  if (column_group && column_group->StyleRef().HasBackground()) {
    table_cell_painter.PaintContainerBackgroundBehindCell(paint_info_for_cells,
                                                          *column_group);
  }
  if (column && column->StyleRef().HasBackground()) {
    table_cell_painter.PaintContainerBackgroundBehindCell(paint_info_for_cells,
                                                          *column);
  }

  // Paint the row group next.
  if (layout_table_section_.StyleRef().HasBackground()) {
    table_cell_painter.PaintContainerBackgroundBehindCell(
        paint_info_for_cells, layout_table_section_);
  }
}

void TableSectionPainter::PaintCell(const LayoutTableCell& cell,
                                    const PaintInfo& paint_info_for_cells) {
  if (!cell.HasSelfPaintingLayer() && !cell.Row()->HasSelfPaintingLayer())
    cell.Paint(paint_info_for_cells);
}

}  // namespace blink
