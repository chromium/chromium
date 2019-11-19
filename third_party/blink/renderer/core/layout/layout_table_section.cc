/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010, 2013 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/layout_table_section.h"

#include <algorithm>
#include <limits>
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_col.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/subtree_layout_scope.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/table_section_painter.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

void LayoutTableSection::TableGridRow::
    SetRowLogicalHeightToRowStyleLogicalHeight() {
  DCHECK(row);
  logical_height = row->StyleRef().LogicalHeight();
}

void LayoutTableSection::TableGridRow::UpdateLogicalHeightForCell(
    const LayoutTableCell* cell) {
  // We ignore height settings on rowspan cells.
  if (cell->ResolvedRowSpan() != 1)
    return;

  const Length& cell_logical_height = cell->StyleRef().LogicalHeight();
  if (cell_logical_height.IsPositive()) {
    switch (cell_logical_height.GetType()) {
      case Length::kPercent:
        // TODO(alancutter): Make this work correctly for calc lengths.
        if (!(logical_height.IsPercentOrCalc()) ||
            (logical_height.IsPercent() &&
             logical_height.Percent() < cell_logical_height.Percent()))
          logical_height = cell_logical_height;
        break;
      case Length::kFixed:
        if (logical_height.IsAuto() ||
            (logical_height.IsFixed() &&
             logical_height.Value() < cell_logical_height.Value()))
          logical_height = cell_logical_height;
        break;
      default:
        break;
    }
  }
}

void CellSpan::EnsureConsistency(const unsigned maximum_span_size) {
  static_assert(std::is_same<decltype(start_), unsigned>::value,
                "Asserts below assume start_ is unsigned");
  static_assert(std::is_same<decltype(end_), unsigned>::value,
                "Asserts below assume end_ is unsigned");
  CHECK_LE(start_, maximum_span_size);
  CHECK_LE(end_, maximum_span_size);
  CHECK_LE(start_, end_);
}

LayoutTableSection::LayoutTableSection(Element* element)
    : LayoutTableBoxComponent(element),
      c_col_(0),
      c_row_(0),
      needs_cell_recalc_(false),
      force_full_paint_(false),
      has_multiple_cell_levels_(false),
      has_spanning_cells_(false),
      is_repeating_header_group_(false),
      is_repeating_footer_group_(false) {
  // init LayoutObject attributes
  SetInline(false);  // our object is not Inline
}

LayoutTableSection::~LayoutTableSection() = default;

void LayoutTableSection::StyleDidChange(StyleDifference diff,
                                        const ComputedStyle* old_style) {
  DCHECK(StyleRef().Display() == EDisplay::kTableFooterGroup ||
         StyleRef().Display() == EDisplay::kTableRowGroup ||
         StyleRef().Display() == EDisplay::kTableHeaderGroup);

  LayoutTableBoxComponent::StyleDidChange(diff, old_style);
  PropagateStyleToAnonymousChildren();

  if (!old_style)
    return;

  LayoutTable* table = Table();
  if (!table)
    return;

  LayoutTableBoxComponent::InvalidateCollapsedBordersOnStyleChange(
      *this, *table, diff, *old_style);

  if (LayoutTableBoxComponent::DoCellsHaveDirtyWidth(*this, *table, diff,
                                                     *old_style)) {
    MarkAllCellsWidthsDirtyAndOrNeedsLayout(
        LayoutTable::kMarkDirtyAndNeedsLayout);
  }
}

void LayoutTableSection::WillBeRemovedFromTree() {
  LayoutTableBoxComponent::WillBeRemovedFromTree();

  // Preventively invalidate our cells as we may be re-inserted into
  // a new table which would require us to rebuild our structure.
  SetNeedsCellRecalc();
}

void LayoutTableSection::AddChild(LayoutObject* child,
                                  LayoutObject* before_child) {
  if (!child->IsTableRow()) {
    LayoutObject* last = before_child;
    if (!last)
      last = LastRow();
    if (last && last->IsAnonymous() && last->IsTablePart() &&
        !last->IsBeforeOrAfterContent()) {
      if (before_child == last)
        before_child = last->SlowFirstChild();
      last->AddChild(child, before_child);
      return;
    }

    if (before_child && !before_child->IsAnonymous() &&
        before_child->Parent() == this) {
      LayoutObject* row = before_child->PreviousSibling();
      if (row && row->IsTableRow() && row->IsAnonymous()) {
        row->AddChild(child);
        return;
      }
    }

    // If beforeChild is inside an anonymous cell/row, insert into the cell or
    // into the anonymous row containing it, if there is one.
    LayoutObject* last_box = last;
    while (last_box && last_box->Parent()->IsAnonymous() &&
           !last_box->IsTableRow())
      last_box = last_box->Parent();
    if (last_box && last_box->IsAnonymous() &&
        !last_box->IsBeforeOrAfterContent()) {
      last_box->AddChild(child, before_child);
      return;
    }

    LayoutObject* row = LayoutTableRow::CreateAnonymousWithParent(this);
    AddChild(row, before_child);
    row->AddChild(child);
    return;
  }

  if (before_child)
    SetNeedsCellRecalc();

  unsigned insertion_row = c_row_;
  ++c_row_;
  c_col_ = 0;

  EnsureRows(c_row_);

  LayoutTableRow* row = To<LayoutTableRow>(child);
  grid_[insertion_row].row = row;
  row->SetRowIndex(insertion_row);

  if (!before_child)
    grid_[insertion_row].SetRowLogicalHeightToRowStyleLogicalHeight();

  if (before_child && before_child->Parent() != this)
    before_child = SplitAnonymousBoxesAroundChild(before_child);

  DCHECK(!before_child || before_child->IsTableRow());
  LayoutTableBoxComponent::AddChild(child, before_child);
}

static inline void CheckThatVectorIsDOMOrdered(
    const Vector<LayoutTableCell*, 1>& cells) {
#ifndef NDEBUG
  // This function should be called on a non-empty vector.
  DCHECK_GT(cells.size(), 0u);

  const LayoutTableCell* previous_cell = cells[0];
  for (wtf_size_t i = 1; i < cells.size(); ++i) {
    const LayoutTableCell* current_cell = cells[i];
    // The check assumes that all cells belong to the same row group.
    DCHECK_EQ(previous_cell->Section(), current_cell->Section());

    // 2 overlapping cells can't be on the same row.
    DCHECK_NE(current_cell->Row(), previous_cell->Row());

    // Look backwards in the tree for the previousCell's row. If we are
    // DOM ordered, we should find it.
    const LayoutTableRow* row = current_cell->Row();
    for (; row && row != previous_cell->Row(); row = row->PreviousRow()) {
    }
    DCHECK_EQ(row, previous_cell->Row());

    previous_cell = current_cell;
  }
#endif  // NDEBUG
}

void LayoutTableSection::AddCell(LayoutTableCell* cell, LayoutTableRow* row) {
  // We don't insert the cell if we need cell recalc as our internal columns'
  // representation will have drifted from the table's representation. Also
  // recalcCells will call addCell at a later time after sync'ing our columns'
  // with the table's.
  if (NeedsCellRecalc())
    return;

  DCHECK(cell);
  unsigned r_span = cell->ResolvedRowSpan();
  unsigned c_span = cell->ColSpan();
  if (r_span > 1 || c_span > 1)
    has_spanning_cells_ = true;

  const Vector<LayoutTable::ColumnStruct>& columns =
      Table()->EffectiveColumns();
  unsigned insertion_row = row->RowIndex();

  // ### mozilla still seems to do the old HTML way, even for strict DTD
  // (see the annotation on table cell layouting in the CSS specs and the
  // testcase below:
  // <TABLE border>
  // <TR><TD>1 <TD rowspan="2">2 <TD>3 <TD>4
  // <TR><TD colspan="2">5
  // </TABLE>
  unsigned n_cols = NumCols(insertion_row);
  while (c_col_ < n_cols && (GridCellAt(insertion_row, c_col_).HasCells() ||
                             GridCellAt(insertion_row, c_col_).InColSpan()))
    c_col_++;

  grid_[insertion_row].UpdateLogicalHeightForCell(cell);

  EnsureRows(insertion_row + r_span);

  grid_[insertion_row].row = row;

  unsigned col = c_col_;
  // tell the cell where it is
  bool in_col_span = false;
  unsigned col_size = columns.size();
  while (c_span) {
    unsigned current_span;
    if (c_col_ >= col_size) {
      Table()->AppendEffectiveColumn(c_span);
      current_span = c_span;
    } else {
      if (c_span < columns[c_col_].span)
        Table()->SplitEffectiveColumn(c_col_, c_span);
      current_span = columns[c_col_].span;
    }
    for (unsigned r = 0; r < r_span; r++) {
      EnsureCols(insertion_row + r, c_col_ + 1);
      auto& grid_cell = GridCellAt(insertion_row + r, c_col_);
      grid_cell.Cells().push_back(cell);
      CheckThatVectorIsDOMOrdered(grid_cell.Cells());
      // If cells overlap then we take the special paint path for them.
      if (grid_cell.Cells().size() > 1)
        has_multiple_cell_levels_ = true;
      if (in_col_span)
        grid_cell.SetInColSpan(true);
    }
    c_col_++;
    c_span -= current_span;
    in_col_span = true;
  }
  cell->SetAbsoluteColumnIndex(Table()->EffectiveColumnToAbsoluteColumn(col));
}

bool LayoutTableSection::RowHasOnlySpanningCells(unsigned row) {
  if (grid_[row].grid_cells.IsEmpty())
    return false;

  for (const auto& grid_cell : grid_[row].grid_cells) {
    // Empty cell is not a valid cell so it is not a rowspan cell.
    if (!grid_cell.HasCells())
      return false;

    if (grid_cell.Cells()[0]->ResolvedRowSpan() == 1)
      return false;
  }

  return true;
}

void LayoutTableSection::PopulateSpanningRowsHeightFromCell(
    LayoutTableCell* cell,
    struct SpanningRowsHeight& spanning_rows_height) {
  const unsigned row_span = cell->ResolvedRowSpan();
  const unsigned row_index = cell->RowIndex();

  spanning_rows_height.spanning_cell_height_ignoring_border_spacing =
      cell->LogicalHeightForRowSizing();

  spanning_rows_height.row_height.resize(row_span);
  spanning_rows_height.total_rows_height = 0;
  for (unsigned row = 0; row < row_span; row++) {
    unsigned actual_row = row + row_index;

    spanning_rows_height.row_height[row] = row_pos_[actual_row + 1] -
                                           row_pos_[actual_row] -
                                           BorderSpacingForRow(actual_row);
    if (!spanning_rows_height.row_height[row])
      spanning_rows_height.is_any_row_with_only_spanning_cells |=
          RowHasOnlySpanningCells(actual_row);

    spanning_rows_height.total_rows_height +=
        spanning_rows_height.row_height[row];
    spanning_rows_height.spanning_cell_height_ignoring_border_spacing -=
        BorderSpacingForRow(actual_row);
  }
  // We don't span the following row so its border-spacing (if any) should be
  // included.
  spanning_rows_height.spanning_cell_height_ignoring_border_spacing +=
      BorderSpacingForRow(row_index + row_span - 1);
}

void LayoutTableSection::DistributeExtraRowSpanHeightToPercentRows(
    LayoutTableCell* cell,
    float total_percent,
    int& extra_row_spanning_height,
    Vector<int>& rows_height) {
  if (!extra_row_spanning_height || !total_percent)
    return;

  const unsigned row_span = cell->ResolvedRowSpan();
  const unsigned row_index = cell->RowIndex();
  float percent = std::min(total_percent, 100.0f);
  const int table_height = row_pos_[grid_.size()] + extra_row_spanning_height;

  // Our algorithm matches Firefox. Extra spanning height would be distributed
  // Only in first percent height rows those total percent is 100. Other percent
  // rows would be uneffected even extra spanning height is remain.
  int accumulated_position_increase = 0;
  for (unsigned row = row_index; row < (row_index + row_span); row++) {
    if (percent > 0 && extra_row_spanning_height > 0) {
      // TODO(alancutter): Make this work correctly for calc lengths.
      if (grid_[row].logical_height.IsPercent()) {
        int to_add =
            (table_height *
             std::min(grid_[row].logical_height.Percent(), percent) / 100) -
            rows_height[row - row_index];

        to_add = std::max(std::min(to_add, extra_row_spanning_height), 0);
        accumulated_position_increase += to_add;
        extra_row_spanning_height -= to_add;
        percent -= grid_[row].logical_height.Percent();
      }
    }
    row_pos_[row + 1] += accumulated_position_increase;
  }
}

static void UpdatePositionIncreasedWithRowHeight(
    int extra_height,
    float row_height,
    float total_height,
    int& accumulated_position_increase,
    double& remainder) {
  // Without the cast we lose enough precision to cause heights to miss pixels
  // (and trigger asserts) in some web tests.
  double proportional_position_increase =
      remainder + (extra_height * double(row_height)) / total_height;
  // The epsilon is to push any values that are close to a whole number but
  // aren't due to floating point imprecision. The epsilons are not accumulated,
  // any that aren't necessary are lost in the cast to int.
  int position_increase_int = proportional_position_increase + 0.000001;
  accumulated_position_increase += position_increase_int;
  remainder = proportional_position_increase - position_increase_int;
}

// This is mainly used to distribute whole extra rowspanning height in percent
// rows when all spanning rows are percent rows.
// Distributing whole extra rowspanning height in percent rows based on the
// ratios of percent because this method works same as percent distribution when
// only percent rows are present and percent is 100. Also works perfectly fine
// when percent is not equal to 100.
void LayoutTableSection::DistributeWholeExtraRowSpanHeightToPercentRows(
    LayoutTableCell* cell,
    float total_percent,
    int& extra_row_spanning_height,
    Vector<int>& rows_height) {
  if (!extra_row_spanning_height || !total_percent)
    return;

  const unsigned row_span = cell->ResolvedRowSpan();
  const unsigned row_index = cell->RowIndex();
  double remainder = 0;

  int accumulated_position_increase = 0;
  for (unsigned row = row_index; row < (row_index + row_span); row++) {
    // TODO(alancutter): Make this work correctly for calc lengths.
    if (grid_[row].logical_height.IsPercent()) {
      UpdatePositionIncreasedWithRowHeight(
          extra_row_spanning_height, grid_[row].logical_height.Percent(),
          total_percent, accumulated_position_increase, remainder);
    }
    row_pos_[row + 1] += accumulated_position_increase;
  }

  DCHECK(!round(remainder)) << "remainder was " << remainder;

  extra_row_spanning_height -= accumulated_position_increase;
}

void LayoutTableSection::DistributeExtraRowSpanHeightToAutoRows(
    LayoutTableCell* cell,
    int total_auto_rows_height,
    int& extra_row_spanning_height,
    Vector<int>& rows_height) {
  if (!extra_row_spanning_height || !total_auto_rows_height)
    return;

  const unsigned row_span = cell->ResolvedRowSpan();
  const unsigned row_index = cell->RowIndex();
  int accumulated_position_increase = 0;
  double remainder = 0;

  // Aspect ratios of auto rows should not change otherwise table may look
  // different than user expected. So extra height distributed in auto spanning
  // rows based on their weight in spanning cell.
  for (unsigned row = row_index; row < (row_index + row_span); row++) {
    if (grid_[row].logical_height.IsAuto()) {
      UpdatePositionIncreasedWithRowHeight(
          extra_row_spanning_height, rows_height[row - row_index],
          total_auto_rows_height, accumulated_position_increase, remainder);
    }
    row_pos_[row + 1] += accumulated_position_increase;
  }

  DCHECK(!round(remainder)) << "remainder was " << remainder;

  extra_row_spanning_height -= accumulated_position_increase;
}

void LayoutTableSection::DistributeExtraRowSpanHeightToRemainingRows(
    LayoutTableCell* cell,
    int total_remaining_rows_height,
    int& extra_row_spanning_height,
    Vector<int>& rows_height) {
  if (!extra_row_spanning_height || !total_remaining_rows_height)
    return;

  const unsigned row_span = cell->ResolvedRowSpan();
  const unsigned row_index = cell->RowIndex();
  int accumulated_position_increase = 0;
  double remainder = 0;

  // Aspect ratios of the rows should not change otherwise table may look
  // different than user expected. So extra height distribution in remaining
  // spanning rows based on their weight in spanning cell.
  for (unsigned row = row_index; row < (row_index + row_span); row++) {
    if (!grid_[row].logical_height.IsPercentOrCalc()) {
      UpdatePositionIncreasedWithRowHeight(
          extra_row_spanning_height, rows_height[row - row_index],
          total_remaining_rows_height, accumulated_position_increase,
          remainder);
    }
    row_pos_[row + 1] += accumulated_position_increase;
  }

  DCHECK(!round(remainder)) << "remainder was " << remainder;

  extra_row_spanning_height -= accumulated_position_increase;
}

static bool CellIsFullyIncludedInOtherCell(const LayoutTableCell* cell1,
                                           const LayoutTableCell* cell2) {
  return (cell1->RowIndex() >= cell2->RowIndex() &&
          (cell1->RowIndex() + cell1->ResolvedRowSpan()) <=
              (cell2->RowIndex() + cell2->ResolvedRowSpan()));
}

// To avoid unneeded extra height distributions, we apply the following sorting
// algorithm:
static bool CompareRowSpanCellsInHeightDistributionOrder(
    const LayoutTableCell* cell1,
    const LayoutTableCell* cell2) {
  // Sorting bigger height cell first if cells are at same index with same span
  // because we will skip smaller height cell to distribute it's extra height.
  if (cell1->RowIndex() == cell2->RowIndex() &&
      cell1->ResolvedRowSpan() == cell2->ResolvedRowSpan())
    return (cell1->LogicalHeightForRowSizing() >
            cell2->LogicalHeightForRowSizing());
  // Sorting inner most cell first because if inner spanning cell'e extra height
  // is distributed then outer spanning cell's extra height will adjust
  // accordingly. In reverse order, there is more chances that outer spanning
  // cell's height will exceed than defined by user.
  if (CellIsFullyIncludedInOtherCell(cell1, cell2))
    return true;
  // Sorting lower row index first because first we need to apply the extra
  // height of spanning cell which comes first in the table so lower rows's
  // position would increment in sequence.
  if (!CellIsFullyIncludedInOtherCell(cell2, cell1))
    return (cell1->RowIndex() < cell2->RowIndex());

  return false;
}

unsigned LayoutTableSection::CalcRowHeightHavingOnlySpanningCells(
    unsigned row,
    int& accumulated_cell_position_increase,
    unsigned row_to_apply_extra_height,
    unsigned& extra_table_height_to_propgate,
    Vector<int>& rows_count_with_only_spanning_cells) {
  DCHECK(RowHasOnlySpanningCells(row));

  unsigned row_height = 0;

  for (const auto& row_span_cell : grid_[row].grid_cells) {
    DCHECK(row_span_cell.HasCells());
    LayoutTableCell* cell = row_span_cell.Cells()[0];
    DCHECK_GE(cell->ResolvedRowSpan(), 2u);

    const unsigned cell_row_index = cell->RowIndex();
    const unsigned cell_row_span = cell->ResolvedRowSpan();

    // As we are going from the top of the table to the bottom to calculate the
    // row heights for rows that only contain spanning cells and all previous
    // rows are processed we only need to find the number of rows with spanning
    // cells from the current cell to the end of the current cells spanning
    // height.
    unsigned start_row_for_spanning_cell_count = std::max(cell_row_index, row);
    unsigned end_row = cell_row_index + cell_row_span;
    unsigned spanning_cells_rows_count_having_zero_height =
        rows_count_with_only_spanning_cells[end_row - 1];

    if (start_row_for_spanning_cell_count)
      spanning_cells_rows_count_having_zero_height -=
          rows_count_with_only_spanning_cells
              [start_row_for_spanning_cell_count - 1];

    int total_rowspan_cell_height =
        (row_pos_[end_row] - row_pos_[cell_row_index]) -
        BorderSpacingForRow(end_row - 1);

    total_rowspan_cell_height += accumulated_cell_position_increase;
    if (row_to_apply_extra_height >= cell_row_index &&
        row_to_apply_extra_height < end_row)
      total_rowspan_cell_height += extra_table_height_to_propgate;

    if (total_rowspan_cell_height < cell->LogicalHeightForRowSizing()) {
      unsigned extra_height_required =
          cell->LogicalHeightForRowSizing() - total_rowspan_cell_height;

      row_height = std::max(
          row_height,
          extra_height_required / spanning_cells_rows_count_having_zero_height);
    }
  }

  return row_height;
}

void LayoutTableSection::UpdateRowsHeightHavingOnlySpanningCells(
    LayoutTableCell* cell,
    struct SpanningRowsHeight& spanning_rows_height,
    unsigned& extra_height_to_propagate,
    Vector<int>& rows_count_with_only_spanning_cells) {
  DCHECK(spanning_rows_height.row_height.size());

  int accumulated_position_increase = 0;
  const unsigned row_span = cell->ResolvedRowSpan();
  const unsigned row_index = cell->RowIndex();

  DCHECK_EQ(row_span, spanning_rows_height.row_height.size());

  for (unsigned row = 0; row < spanning_rows_height.row_height.size(); row++) {
    unsigned actual_row = row + row_index;
    if (!spanning_rows_height.row_height[row] &&
        RowHasOnlySpanningCells(actual_row)) {
      spanning_rows_height.row_height[row] =
          CalcRowHeightHavingOnlySpanningCells(
              actual_row, accumulated_position_increase, row_index + row_span,
              extra_height_to_propagate, rows_count_with_only_spanning_cells);
      accumulated_position_increase += spanning_rows_height.row_height[row];
    }
    row_pos_[actual_row + 1] += accumulated_position_increase;
  }

  spanning_rows_height.total_rows_height += accumulated_position_increase;
}

// Distribute rowSpan cell height in rows those comes in rowSpan cell based on
// the ratio of row's height if 1 RowSpan cell height is greater than the total
// height of rows in rowSpan cell.
void LayoutTableSection::DistributeRowSpanHeightToRows(
    SpanningLayoutTableCells& row_span_cells) {
  DCHECK(row_span_cells.size());

  // 'rowSpanCells' list is already sorted based on the cells rowIndex in
  // ascending order
  // Arrange row spanning cell in the order in which we need to process first.
  std::sort(row_span_cells.begin(), row_span_cells.end(),
            CompareRowSpanCellsInHeightDistributionOrder);

  unsigned extra_height_to_propagate = 0;
  unsigned last_row_index = 0;
  unsigned last_row_span = 0;

  Vector<int> rows_count_with_only_spanning_cells;

  // At this stage, Height of the rows are zero for the one containing only
  // spanning cells.
  int count = 0;
  for (unsigned row = 0; row < grid_.size(); row++) {
    if (RowHasOnlySpanningCells(row))
      count++;
    rows_count_with_only_spanning_cells.push_back(count);
  }

  for (unsigned i = 0; i < row_span_cells.size(); i++) {
    LayoutTableCell* cell = row_span_cells[i];

    unsigned row_index = cell->RowIndex();

    unsigned row_span = cell->ResolvedRowSpan();

    unsigned spanning_cell_end_index = row_index + row_span;
    unsigned last_spanning_cell_end_index = last_row_index + last_row_span;

    // Only the highest spanning cell will distribute its extra height in a row
    // if more than one spanning cell is present at the same level.
    if (row_index == last_row_index && row_span == last_row_span)
      continue;

    int original_before_position = row_pos_[spanning_cell_end_index];

    // When 2 spanning cells are ending at same row index then while extra
    // height distribution of first spanning cell updates position of the last
    // row so getting the original position of the last row in second spanning
    // cell need to reduce the height changed by first spanning cell.
    if (spanning_cell_end_index == last_spanning_cell_end_index)
      original_before_position -= extra_height_to_propagate;

    if (extra_height_to_propagate) {
      for (unsigned row = last_spanning_cell_end_index + 1;
           row <= spanning_cell_end_index; row++)
        row_pos_[row] += extra_height_to_propagate;
    }

    last_row_index = row_index;
    last_row_span = row_span;

    struct SpanningRowsHeight spanning_rows_height;

    PopulateSpanningRowsHeightFromCell(cell, spanning_rows_height);

    // Here we are handling only row(s) who have only rowspanning cells and do
    // not have any empty cell.
    if (spanning_rows_height.is_any_row_with_only_spanning_cells)
      UpdateRowsHeightHavingOnlySpanningCells(
          cell, spanning_rows_height, extra_height_to_propagate,
          rows_count_with_only_spanning_cells);

    // This code handle row(s) that have rowspanning cell(s) and at least one
    // empty cell. Such rows are not handled below and end up having a height of
    // 0. That would mean content overlapping if one of their cells has any
    // content. To avoid the problem, we add all the remaining spanning cells'
    // height to the last spanned row. This means that we could grow a row past
    // its 'height' or break percentage spreading however this is better than
    // overlapping content.
    // FIXME: Is there a better algorithm?
    if (!spanning_rows_height.total_rows_height) {
      if (spanning_rows_height.spanning_cell_height_ignoring_border_spacing)
        row_pos_[spanning_cell_end_index] +=
            spanning_rows_height.spanning_cell_height_ignoring_border_spacing +
            BorderSpacingForRow(spanning_cell_end_index - 1);

      extra_height_to_propagate =
          row_pos_[spanning_cell_end_index] - original_before_position;
      continue;
    }

    if (spanning_rows_height.spanning_cell_height_ignoring_border_spacing <=
        spanning_rows_height.total_rows_height) {
      extra_height_to_propagate =
          row_pos_[row_index + row_span] - original_before_position;
      continue;
    }

    // Below we are handling only row(s) who have at least one visible cell
    // without rowspan value.
    float total_percent = 0;
    int total_auto_rows_height = 0;
    int total_remaining_rows_height = spanning_rows_height.total_rows_height;

    // FIXME: Inner spanning cell height should not change if it have fixed
    // height when it's parent spanning cell is distributing it's extra height
    // in rows.

    // Calculate total percentage, total auto rows height and total rows height
    // except percent rows.
    for (unsigned row = row_index; row < spanning_cell_end_index; row++) {
      // TODO(alancutter): Make this work correctly for calc lengths.
      if (grid_[row].logical_height.IsPercent()) {
        total_percent += grid_[row].logical_height.Percent();
        total_remaining_rows_height -=
            spanning_rows_height.row_height[row - row_index];
      } else if (grid_[row].logical_height.IsAuto()) {
        total_auto_rows_height +=
            spanning_rows_height.row_height[row - row_index];
      }
    }

    int extra_row_spanning_height =
        spanning_rows_height.spanning_cell_height_ignoring_border_spacing -
        spanning_rows_height.total_rows_height;

    if (total_percent < 100 && !total_auto_rows_height &&
        !total_remaining_rows_height) {
      // Distributing whole extra rowspanning height in percent row when only
      // non-percent rows height is 0.
      DistributeWholeExtraRowSpanHeightToPercentRows(
          cell, total_percent, extra_row_spanning_height,
          spanning_rows_height.row_height);
    } else {
      DistributeExtraRowSpanHeightToPercentRows(
          cell, total_percent, extra_row_spanning_height,
          spanning_rows_height.row_height);
      DistributeExtraRowSpanHeightToAutoRows(cell, total_auto_rows_height,
                                             extra_row_spanning_height,
                                             spanning_rows_height.row_height);
      DistributeExtraRowSpanHeightToRemainingRows(
          cell, total_remaining_rows_height, extra_row_spanning_height,
          spanning_rows_height.row_height);
    }

    DCHECK(!extra_row_spanning_height);

    // Getting total changed height in the table
    extra_height_to_propagate =
        row_pos_[spanning_cell_end_index] - original_before_position;
  }

  if (extra_height_to_propagate) {
    // Apply changed height by rowSpan cells to rows present at the end of the
    // table
    for (unsigned row = last_row_index + last_row_span + 1; row <= grid_.size();
         row++)
      row_pos_[row] += extra_height_to_propagate;
  }
}

bool LayoutTableSection::RowHasVisibilityCollapse(unsigned row) const {
  return ((grid_[row].row &&
           grid_[row].row->StyleRef().Visibility() == EVisibility::kCollapse) ||
          StyleRef().Visibility() == EVisibility::kCollapse);
}

// Find out the baseline of the cell
// If the cell's baseline is more than the row's baseline then the cell's
// baseline become the row's baseline and if the row's baseline goes out of the
// row's boundaries then adjust row height accordingly.
void LayoutTableSection::UpdateBaselineForCell(LayoutTableCell* cell,
                                               unsigned row,
                                               LayoutUnit& baseline_descent) {
  if (!cell->IsBaselineAligned())
    return;

  // Ignoring the intrinsic padding as it depends on knowing the row's baseline,
  // which won't be accurate until the end of this function.
  LayoutUnit baseline_position =
      cell->CellBaselinePosition() - cell->IntrinsicPaddingBefore();
  if (baseline_position >
      cell->BorderBefore() +
          (cell->PaddingBefore() - cell->IntrinsicPaddingBefore())) {
    grid_[row].baseline = std::max(grid_[row].baseline, baseline_position);

    LayoutUnit cell_start_row_baseline_descent;
    if (cell->ResolvedRowSpan() == 1) {
      baseline_descent =
          std::max(baseline_descent,
                   cell->LogicalHeightForRowSizing() - baseline_position);
      cell_start_row_baseline_descent = baseline_descent;
    }
    row_pos_[row + 1] = std::max(
        row_pos_[row + 1],
        (row_pos_[row] + grid_[row].baseline + cell_start_row_baseline_descent)
            .ToInt());
  }
}

int16_t LayoutTableSection::VBorderSpacingBeforeFirstRow() const {
  // We ignore the border-spacing on any non-top section, as it is already
  // included in the previous section's last row position.
  if (this != Table()->TopSection())
    return 0;
  return Table()->VBorderSpacing();
}

int LayoutTableSection::CalcRowLogicalHeight() {
#if DCHECK_IS_ON()
  SetLayoutNeededForbiddenScope layout_forbidden_scope(*this);
#endif

  DCHECK(!NeedsLayout());

  // We may have to forcefully lay out cells here, in which case we need a
  // layout state.
  LayoutState state(*this);

  row_pos_.resize(grid_.size() + 1);
  row_pos_[0] = VBorderSpacingBeforeFirstRow();

  SpanningLayoutTableCells row_span_cells;

  // At fragmentainer breaks we need to prevent rowspanned cells (and whatever
  // else) from distributing their extra height requirements over the rows that
  // it spans. Otherwise we'd need to refragment afterwards.
  unsigned index_of_first_stretchable_row = 0;

  is_any_row_collapsed_ = false;

  for (unsigned r = 0; r < grid_.size(); r++) {
    grid_[r].baseline = LayoutUnit(-1);
    LayoutUnit baseline_descent;

    if (!is_any_row_collapsed_)
      is_any_row_collapsed_ = RowHasVisibilityCollapse(r);

    if (state.IsPaginated() && grid_[r].row)
      row_pos_[r] += grid_[r].row->PaginationStrut().Ceil();

    if (grid_[r].logical_height.IsSpecified()) {
      // Our base size is the biggest logical height from our cells' styles
      // (excluding row spanning cells).
      row_pos_[r + 1] =
          std::max(row_pos_[r] + MinimumValueForLength(grid_[r].logical_height,
                                                       LayoutUnit())
                                     .Round(),
                   0);
    } else {
      // Non-specified lengths are ignored because the row already accounts for
      // the cells intrinsic logical height.
      row_pos_[r + 1] = std::max(row_pos_[r], 0);
    }

    for (auto& grid_cell : grid_[r].grid_cells) {
      if (grid_cell.InColSpan())
        continue;
      for (auto* cell : grid_cell.Cells()) {
        // For row spanning cells, we only handle them for the first row they
        // span. This ensures we take their baseline into account.
        if (cell->RowIndex() != r)
          continue;

        if (r < index_of_first_stretchable_row ||
            (state.IsPaginated() &&
             CrossesPageBoundary(
                 LayoutUnit(row_pos_[r]),
                 LayoutUnit(cell->LogicalHeightForRowSizing())))) {
          // Entering or extending a range of unstretchable rows. We enter this
          // mode when a cell in a row crosses a fragmentainer boundary, and
          // we'll stay in this mode until we get to a row where we're past all
          // rowspanned cells that we encountered while in this mode.
          DCHECK(state.IsPaginated());
          unsigned row_index_below_cell = r + cell->ResolvedRowSpan();
          index_of_first_stretchable_row =
              std::max(index_of_first_stretchable_row, row_index_below_cell);
        } else if (cell->ResolvedRowSpan() > 1) {
          DCHECK(!row_span_cells.Contains(cell));

          cell->SetIsSpanningCollapsedRow(false);
          unsigned end_row = cell->ResolvedRowSpan() + r;
          for (unsigned spanning = r; spanning < end_row; spanning++) {
            if (RowHasVisibilityCollapse(spanning)) {
              cell->SetIsSpanningCollapsedRow(true);
              break;
            }
          }

          row_span_cells.push_back(cell);
        }

        if (cell->HasOverrideLogicalHeight()) {
          cell->ClearIntrinsicPadding();
          cell->ClearOverrideSize();
          cell->ForceLayout();
        }

        if (cell->ResolvedRowSpan() == 1)
          row_pos_[r + 1] = std::max(
              row_pos_[r + 1], row_pos_[r] + cell->LogicalHeightForRowSizing());

        // Find out the baseline. The baseline is set on the first row in a
        // rowSpan.
        UpdateBaselineForCell(cell, r, baseline_descent);
      }
    }

    if (r < index_of_first_stretchable_row && grid_[r].row) {
      // We're not allowed to resize this row. Just scratch what we've
      // calculated so far, and use the height that we got during initial
      // layout instead.
      row_pos_[r + 1] = row_pos_[r] + grid_[r].row->LogicalHeight().ToInt();
    }

    // Add the border-spacing to our final position.
    row_pos_[r + 1] += BorderSpacingForRow(r);
    row_pos_[r + 1] = std::max(row_pos_[r + 1], row_pos_[r]);
  }

  if (!row_span_cells.IsEmpty())
    DistributeRowSpanHeightToRows(row_span_cells);

  DCHECK(!NeedsLayout());

  // Collapsed rows are dealt with after distributing row span height to rows.
  // This is because the distribution calculations should be as if the row were
  // not collapsed. First, all rows' collapsed heights are set. After, row
  // positions are adjusted accordingly.
  if (is_any_row_collapsed_) {
    row_collapsed_height_.resize(grid_.size());
    for (unsigned r = 0; r < grid_.size(); r++) {
      if (RowHasVisibilityCollapse(r)) {
        // Update vector that keeps track of collapsed height of each row.
        row_collapsed_height_[r] = row_pos_[r + 1] - row_pos_[r];
      } else {
        // Reset rows that are no longer collapsed.
        row_collapsed_height_[r] = 0;
      }
    }

    int total_collapsed_height = 0;
    for (unsigned r = 0; r < grid_.size(); r++) {
      total_collapsed_height += row_collapsed_height_[r];
      // Adjust row position according to the height collapsed so far.
      row_pos_[r + 1] -= total_collapsed_height;
      DCHECK_GE(row_pos_[r + 1], row_pos_[r]);
    }
  }

  return row_pos_[grid_.size()];
}

void LayoutTableSection::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);
  CHECK(!NeedsCellRecalc());
  DCHECK(!Table()->NeedsSectionRecalc());

  // addChild may over-grow grid_ but we don't want to throw away the memory
  // too early as addChild can be called in a loop (e.g during parsing). Doing
  // it now ensures we have a stable-enough structure.
  grid_.ShrinkToFit();

  LayoutState state(*this);

  const Vector<int>& column_pos = Table()->EffectiveColumnPositions();
  LayoutUnit row_logical_top(VBorderSpacingBeforeFirstRow());

  SubtreeLayoutScope layouter(*this);
  for (unsigned r = 0; r < grid_.size(); ++r) {
    auto& grid_cells = grid_[r].grid_cells;
    unsigned cols = grid_cells.size();
    // First, propagate our table layout's information to the cells. This will
    // mark the row as needing layout if there was a column logical width
    // change.
    for (unsigned start_column = 0; start_column < cols; ++start_column) {
      auto& grid_cell = grid_cells[start_column];
      LayoutTableCell* cell = grid_cell.PrimaryCell();
      if (!cell || grid_cell.InColSpan())
        continue;

      unsigned end_col = start_column;
      unsigned cspan = cell->ColSpan();
      while (cspan && end_col < cols) {
        DCHECK_LT(end_col, Table()->EffectiveColumns().size());
        cspan -= Table()->EffectiveColumns()[end_col].span;
        end_col++;
      }
      int table_layout_logical_width = column_pos[end_col] -
                                       column_pos[start_column] -
                                       Table()->HBorderSpacing();
      cell->SetCellLogicalWidth(table_layout_logical_width, layouter);
    }

    if (LayoutTableRow* row = grid_[r].row) {
      if (state.IsPaginated())
        row->SetLogicalTop(row_logical_top);
      if (!row->NeedsLayout())
        MarkChildForPaginationRelayoutIfNeeded(*row, layouter);
      row->LayoutIfNeeded();
      if (state.IsPaginated()) {
        AdjustRowForPagination(*row, layouter);
        UpdateFragmentationInfoForChild(*row);
        row_logical_top = row->LogicalBottom();
        row_logical_top += LayoutUnit(Table()->VBorderSpacing());
      }

      if (!Table()->HasSameDirectionAs(row)) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kTableRowDirectionDifferentFromTable);
      }
    }
  }

  if (!Table()->HasSameDirectionAs(this)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kTableSectionDirectionDifferentFromTable);
  }

  ClearNeedsLayout();
}

void LayoutTableSection::DistributeExtraLogicalHeightToPercentRows(
    int& extra_logical_height,
    int total_percent) {
  if (!total_percent)
    return;

  unsigned total_rows = grid_.size();
  int total_height = row_pos_[total_rows] + extra_logical_height;
  int total_logical_height_added = 0;
  total_percent = std::min(total_percent, 100);
  int row_height = row_pos_[1] - row_pos_[0];
  for (unsigned r = 0; r < total_rows; ++r) {
    // TODO(alancutter): Make this work correctly for calc lengths.
    if (total_percent > 0 && grid_[r].logical_height.IsPercent()) {
      int to_add = std::min<int>(
          extra_logical_height,
          (total_height * grid_[r].logical_height.Percent() / 100) -
              row_height);
      // If toAdd is negative, then we don't want to shrink the row (this bug
      // affected Outlook Web Access).
      to_add = std::max(0, to_add);
      total_logical_height_added += to_add;
      extra_logical_height -= to_add;
      total_percent -= grid_[r].logical_height.Percent();
    }
    DCHECK_GE(total_rows, 1u);
    if (r < total_rows - 1)
      row_height = row_pos_[r + 2] - row_pos_[r + 1];
    row_pos_[r + 1] += total_logical_height_added;
  }
}

void LayoutTableSection::DistributeExtraLogicalHeightToAutoRows(
    int& extra_logical_height,
    unsigned auto_rows_count) {
  if (!auto_rows_count)
    return;

  int total_logical_height_added = 0;
  for (unsigned r = 0; r < grid_.size(); ++r) {
    if (auto_rows_count > 0 && grid_[r].logical_height.IsAuto()) {
      // Recomputing |extraLogicalHeightForRow| guarantees that we properly
      // ditribute round |extraLogicalHeight|.
      int extra_logical_height_for_row = extra_logical_height / auto_rows_count;
      total_logical_height_added += extra_logical_height_for_row;
      extra_logical_height -= extra_logical_height_for_row;
      --auto_rows_count;
    }
    row_pos_[r + 1] += total_logical_height_added;
  }
}

void LayoutTableSection::DistributeRemainingExtraLogicalHeight(
    int& extra_logical_height) {
  unsigned total_rows = grid_.size();

  if (extra_logical_height <= 0 || !row_pos_[total_rows])
    return;

  int total_logical_height_added = 0;
  int previous_row_position = row_pos_[0];
  float total_row_size = row_pos_[total_rows] - previous_row_position;
  for (unsigned r = 0; r < total_rows; r++) {
    // weight with the original height
    float height_to_add = extra_logical_height *
                          (row_pos_[r + 1] - previous_row_position) /
                          total_row_size;
    total_logical_height_added =
        std::min<int>(total_logical_height_added + std::ceil(height_to_add),
                      extra_logical_height);
    previous_row_position = row_pos_[r + 1];
    row_pos_[r + 1] += total_logical_height_added;
  }

  extra_logical_height -= total_logical_height_added;
}

int LayoutTableSection::DistributeExtraLogicalHeightToRows(
    int extra_logical_height) {
  if (!extra_logical_height)
    return extra_logical_height;

  unsigned total_rows = grid_.size();
  if (!total_rows)
    return extra_logical_height;

  if (!row_pos_[total_rows] && NextSibling())
    return extra_logical_height;

  unsigned auto_rows_count = 0;
  int total_percent = 0;
  for (unsigned r = 0; r < total_rows; r++) {
    if (grid_[r].logical_height.IsAuto())
      ++auto_rows_count;
    else if (grid_[r].logical_height.IsPercent())
      total_percent += grid_[r].logical_height.Percent();
  }

  int remaining_extra_logical_height = extra_logical_height;
  DistributeExtraLogicalHeightToPercentRows(remaining_extra_logical_height,
                                            total_percent);
  DistributeExtraLogicalHeightToAutoRows(remaining_extra_logical_height,
                                         auto_rows_count);
  DistributeRemainingExtraLogicalHeight(remaining_extra_logical_height);
  return extra_logical_height - remaining_extra_logical_height;
}

static bool CellHasExplicitlySpecifiedHeight(const LayoutTableCell& cell) {
  if (cell.StyleRef().LogicalHeight().IsFixed())
    return true;
  LayoutBlock* cb = cell.ContainingBlock();
  if (cb->AvailableLogicalHeightForPercentageComputation() == -1)
    return false;
  return true;
}

void LayoutTableSection::LayoutRows() {
#if DCHECK_IS_ON()
  SetLayoutNeededForbiddenScope layout_forbidden_scope(*this);
#endif

  DCHECK(!NeedsLayout());

  LayoutAnalyzer::Scope analyzer(*this);

  // FIXME: Changing the height without a layout can change the overflow so it
  // seems wrong.

  unsigned total_rows = grid_.size();

  // Set the width of our section now.  The rows will also be this width.
  SetLogicalWidth(Table()->ContentLogicalWidth());

  int16_t vspacing = Table()->VBorderSpacing();
  LayoutState state(*this);

  // Set the rows' location and size.
  for (unsigned r = 0; r < total_rows; r++) {
    if (LayoutTableRow* row = grid_[r].row) {
      row->SetLogicalLocation(LayoutPoint(0, row_pos_[r]));
      row->SetLogicalWidth(LogicalWidth());
      LayoutUnit row_logical_height;
      // If the row is collapsed then it has 0 height. vspacing was implicitly
      // removed earlier, when row_pos_[r+1] was set to row_pos[r].
      if (!RowHasVisibilityCollapse(r)) {
        row_logical_height =
            LayoutUnit(row_pos_[r + 1] - row_pos_[r] - vspacing);
      }
      DCHECK_GE(row_logical_height, 0);
      if (state.IsPaginated() && r + 1 < total_rows) {
        // If the next row has a pagination strut, we need to subtract it. It
        // should not be included in this row's height.
        if (LayoutTableRow* next_row_object = grid_[r + 1].row)
          row_logical_height -= next_row_object->PaginationStrut();
      }
      DCHECK_GE(row_logical_height, 0);
      row->SetLogicalHeight(row_logical_height);
      row->UpdateAfterLayout();
    }
  }

  // Vertically align and flex the cells in each row.
  for (unsigned r = 0; r < total_rows; r++) {
    LayoutTableRow* row = grid_[r].row;

    unsigned n_cols = NumCols(r);
    for (unsigned c = 0; c < n_cols; c++) {
      LayoutTableCell* cell = OriginatingCellAt(r, c);
      if (!cell)
        continue;

      int r_height;
      int row_logical_top;
      unsigned row_span = std::max(1U, cell->ResolvedRowSpan());
      unsigned end_row_index = std::min(r + row_span, total_rows) - 1;
      LayoutTableRow* last_row_object = grid_[end_row_index].row;
      if (last_row_object && row) {
        row_logical_top = row->LogicalTop().ToInt();
        r_height = last_row_object->LogicalBottom().ToInt() - row_logical_top;
      } else {
        r_height = row_pos_[end_row_index + 1] - row_pos_[r] - vspacing;
        row_logical_top = row_pos_[r];
      }

      RelayoutCellIfFlexed(*cell, r, r_height);

      SubtreeLayoutScope layouter(*cell);
      EVerticalAlign cell_vertical_align;
      // If the cell crosses a fragmentainer boundary, just align it at the
      // top. That's how it was laid out initially, before we knew the final
      // row height, and re-aligning it now could result in the cell being
      // fragmented differently, which could change its height and thus violate
      // the requested alignment. Give up instead of risking circular
      // dependencies and unstable layout.
      if (state.IsPaginated() &&
          CrossesPageBoundary(LayoutUnit(row_logical_top),
                              LayoutUnit(r_height)))
        cell_vertical_align = EVerticalAlign::kTop;
      else
        cell_vertical_align = cell->StyleRef().VerticalAlign();

      // Calculate total collapsed height affecting one cell.
      int collapsed_height = 0;
      if (is_any_row_collapsed_) {
        unsigned end_row = cell->ResolvedRowSpan() + r;
        for (unsigned spanning = r; spanning < end_row; spanning++) {
          collapsed_height += row_collapsed_height_[spanning];
        }
      }

      cell->ComputeIntrinsicPadding(collapsed_height, r_height,
                                    cell_vertical_align, layouter);

      LayoutRect old_cell_rect = cell->FrameRect();

      SetLogicalPositionForCell(cell, c);

      cell->LayoutIfNeeded();

      LayoutSize child_offset(cell->Location() - old_cell_rect.Location());
      if (child_offset.Width() || child_offset.Height()) {
        // If the child moved, we have to issue paint invalidations to it as
        // well as any floating/positioned descendants. An exception is if we
        // need a layout. In this case, we know we're going to issue paint
        // invalidations ourselves (and the child) anyway.
        if (!Table()->SelfNeedsLayout())
          cell->SetShouldCheckForPaintInvalidation();
      }
    }
    if (row)
      row->ComputeLayoutOverflow();
  }

  DCHECK(!NeedsLayout());

  SetLogicalHeight(LayoutUnit(row_pos_[total_rows]));

  ComputeLayoutOverflowFromDescendants();
}

void LayoutTableSection::UpdateLogicalWidthForCollapsedCells(
    const Vector<int>& col_collapsed_width) {
  if (!RuntimeEnabledFeatures::VisibilityCollapseColumnEnabled())
    return;
  unsigned total_rows = grid_.size();
  for (unsigned r = 0; r < total_rows; r++) {
    unsigned n_cols = NumCols(r);
    for (unsigned c = 0; c < n_cols; c++) {
      LayoutTableCell* cell = OriginatingCellAt(r, c);
      if (!cell)
        continue;
      if (!col_collapsed_width.size()) {
        cell->SetIsSpanningCollapsedColumn(false);
        continue;
      }
      // TODO(joysyu): Current behavior assumes that collapsing the first column
      // in a col-spanning cell makes the cell width zero. This is consistent
      // with collapsing row-spanning cells, but still needs to be specified.
      if (cell->IsFirstColumnCollapsed()) {
        // Collapsed cells have zero width.
        cell->SetLogicalWidth(LayoutUnit());
      } else if (cell->ColSpan() > 1) {
        // A column-spanning cell may be affected by collapsed columns, so its
        // width needs to be adjusted accordingly
        int collapsed_width = 0;
        cell->SetIsSpanningCollapsedColumn(false);
        unsigned end_col = std::min(cell->ColSpan() + c, n_cols);
        for (unsigned spanning = c; spanning < end_col; spanning++)
          collapsed_width += col_collapsed_width[spanning];
        cell->SetLogicalWidth(cell->LogicalWidth() - collapsed_width);
        if (collapsed_width != 0)
          cell->SetIsSpanningCollapsedColumn(true);
        // Recompute overflow in case overflow clipping is necessary.
        cell->ComputeLayoutOverflow(cell->ClientLogicalBottom(), false);
        DCHECK_GE(cell->LogicalWidth(), 0);
      }
    }
  }
}

int LayoutTableSection::PaginationStrutForRow(LayoutTableRow* row,
                                              LayoutUnit logical_offset) const {
  DCHECK(row);
  const LayoutTableSection* footer = Table()->Footer();
  bool make_room_for_repeating_footer =
      footer && footer->IsRepeatingFooterGroup() && row->RowIndex();
  if (!make_room_for_repeating_footer &&
      row->GetPaginationBreakability() == kAllowAnyBreaks)
    return 0;
  if (!IsPageLogicalHeightKnown())
    return 0;
  LayoutUnit page_logical_height = PageLogicalHeightForOffset(logical_offset);
  // If the row is too tall for the page don't insert a strut.
  LayoutUnit row_logical_height = row->LogicalHeight();
  if (row_logical_height > page_logical_height)
    return 0;

  LayoutUnit remaining_logical_height = PageRemainingLogicalHeightForOffset(
      logical_offset, LayoutBlock::kAssociateWithLatterPage);
  if (remaining_logical_height >= row_logical_height)
    return 0;  // It fits fine where it is. No need to break.
  LayoutUnit pagination_strut =
      CalculatePaginationStrutToFitContent(logical_offset, row_logical_height);
  if (pagination_strut == remaining_logical_height &&
      remaining_logical_height == page_logical_height) {
    // Don't break if we were at the top of a page, and we failed to fit the
    // content completely. No point in leaving a page completely blank.
    return 0;
  }
  // Table layout parts only work on integers, so we have to round. Round up, to
  // make sure that no fraction ever gets left behind in the previous
  // fragmentainer.
  return pagination_strut.Ceil();
}

void LayoutTableSection::ComputeVisualOverflowFromDescendants() {
  auto old_self_visual_overflow_rect = SelfVisualOverflowRect();
  ClearVisualOverflow();

  visually_overflowing_cells_.clear();
  force_full_paint_ = false;

  // These 2 variables are used to balance the memory consumption vs the paint
  // time on big sections with overflowing cells:
  // 1. For small sections, don't track overflowing cells because for them the
  //    full paint path is actually faster than the partial paint path.
  // 2. For big sections, if overflowing cells are scarce, track overflowing
  //    cells to enable the partial paint path.
  // 3. Otherwise don't track overflowing cells to avoid adding a lot of cells
  //    to the HashSet, and force the full paint path.
  // See TableSectionPainter::PaintObject() for the full paint path and the
  // partial paint path.
  static const unsigned kMinCellCountToUsePartialPaint = 75 * 75;
  static const float kMaxOverflowingCellRatioForPartialPaint = 0.1f;

  unsigned total_cell_count = NumRows() * Table()->NumEffectiveColumns();
  unsigned max_overflowing_cell_count =
      total_cell_count < kMinCellCountToUsePartialPaint
          ? 0
          : kMaxOverflowingCellRatioForPartialPaint * total_cell_count;

#if DCHECK_IS_ON()
  bool has_overflowing_cell = false;
#endif
  for (auto* row = FirstRow(); row; row = row->NextRow()) {
    AddVisualOverflowFromChild(*row);

    for (auto* cell = row->FirstCell(); cell; cell = cell->NextCell()) {
      if (cell->HasSelfPaintingLayer())
        continue;
      if (force_full_paint_ || !cell->HasVisualOverflow())
        continue;

#if DCHECK_IS_ON()
      has_overflowing_cell = true;
#endif
      if (visually_overflowing_cells_.size() >= max_overflowing_cell_count) {
        force_full_paint_ = true;
        // The full paint path does not make any use of the overflowing cells
        // info, so don't hold on to the memory.
        visually_overflowing_cells_.clear();
        continue;
      }

      visually_overflowing_cells_.insert(cell);
    }
  }

#if DCHECK_IS_ON()
  DCHECK_EQ(has_overflowing_cell, HasVisuallyOverflowingCell());
#endif

  // Overflow rect contributes to the visual rect, so if it has changed then we
  // need to signal a possible paint invalidation.
  if (old_self_visual_overflow_rect != SelfVisualOverflowRect())
    SetShouldCheckForPaintInvalidation();
}

void LayoutTableSection::ComputeLayoutOverflowFromDescendants() {
  ClearLayoutOverflow();
  for (auto* row = FirstRow(); row; row = row->NextRow())
    AddLayoutOverflowFromChild(*row);
}

LayoutNGTableRowInterface* LayoutTableSection::FirstRowInterface() const {
  return FirstRow();
}
LayoutNGTableRowInterface* LayoutTableSection::LastRowInterface() const {
  return LastRow();
}
const LayoutNGTableCellInterface* LayoutTableSection::PrimaryCellInterfaceAt(
    unsigned row,
    unsigned effective_column) const {
  return PrimaryCellAt(row, effective_column);
}

bool LayoutTableSection::RecalcLayoutOverflow() {
  if (!ChildNeedsLayoutOverflowRecalc())
    return false;
  ClearChildNeedsLayoutOverflowRecalc();
  unsigned total_rows = grid_.size();
  bool children_layout_overflow_changed = false;
  for (unsigned r = 0; r < total_rows; r++) {
    LayoutTableRow* row_layouter = RowLayoutObjectAt(r);
    if (!row_layouter || !row_layouter->ChildNeedsLayoutOverflowRecalc())
      continue;
    row_layouter->ClearChildNeedsLayoutOverflowRecalc();
    bool row_children_layout_overflow_changed = false;
    unsigned n_cols = NumCols(r);
    for (unsigned c = 0; c < n_cols; c++) {
      auto* cell = OriginatingCellAt(r, c);
      if (!cell)
        continue;
      row_children_layout_overflow_changed |= cell->RecalcLayoutOverflow();
    }
    if (row_children_layout_overflow_changed)
      row_layouter->ComputeLayoutOverflow();
    children_layout_overflow_changed |= row_children_layout_overflow_changed;
  }
  if (children_layout_overflow_changed)
    ComputeLayoutOverflowFromDescendants();

  return children_layout_overflow_changed;
}

void LayoutTableSection::RecalcVisualOverflow() {
  SECURITY_CHECK(!needs_cell_recalc_);
  unsigned total_rows = grid_.size();
  for (unsigned r = 0; r < total_rows; r++) {
    LayoutTableRow* row_layouter = RowLayoutObjectAt(r);
    if (!row_layouter || (row_layouter->HasLayer() &&
                          row_layouter->Layer()->IsSelfPaintingLayer()))
      continue;
    row_layouter->RecalcVisualOverflow();
  }
  ComputeVisualOverflowFromDescendants();
  AddVisualEffectOverflow();
}

void LayoutTableSection::MarkAllCellsWidthsDirtyAndOrNeedsLayout(
    LayoutTable::WhatToMarkAllCells what_to_mark) {
  for (LayoutTableRow* row = FirstRow(); row; row = row->NextRow()) {
    for (LayoutTableCell* cell = row->FirstCell(); cell;
         cell = cell->NextCell()) {
      cell->SetPreferredLogicalWidthsDirty();
      if (what_to_mark == LayoutTable::kMarkDirtyAndNeedsLayout)
        cell->SetChildNeedsLayout();
    }
  }
}

LayoutUnit LayoutTableSection::FirstLineBoxBaseline() const {
  DCHECK(!NeedsCellRecalc());
  if (!grid_.size())
    return LayoutUnit(-1);

  LayoutUnit first_line_baseline(grid_[0].baseline);
  if (first_line_baseline >= 0)
    return first_line_baseline + row_pos_[0];

  for (const auto& grid_cell : grid_[0].grid_cells) {
    if (const auto* cell = grid_cell.PrimaryCell()) {
      first_line_baseline = std::max<LayoutUnit>(
          first_line_baseline, cell->LogicalTop() + cell->BorderBefore() +
                                   cell->PaddingBefore() +
                                   cell->ContentLogicalHeight());
    }
  }

  return first_line_baseline;
}

void LayoutTableSection::Paint(const PaintInfo& paint_info) const {
  TableSectionPainter(*this).Paint(paint_info);
}

LayoutRect LayoutTableSection::LogicalRectForWritingModeAndDirection(
    const PhysicalRect& rect) const {
  LayoutRect table_aligned_rect = FlipForWritingMode(rect);

  if (!TableStyle().IsHorizontalWritingMode())
    table_aligned_rect = table_aligned_rect.TransposedRect();

  const Vector<int>& column_pos = Table()->EffectiveColumnPositions();
  if (!TableStyle().IsLeftToRightDirection()) {
    table_aligned_rect.SetX(column_pos[column_pos.size() - 1] -
                            table_aligned_rect.MaxX());
  }

  return table_aligned_rect;
}

void LayoutTableSection::DirtiedRowsAndEffectiveColumns(
    const LayoutRect& damage_rect,
    CellSpan& rows,
    CellSpan& columns) const {
  DCHECK(!NeedsCellRecalc());
  if (!grid_.size()) {
    rows = CellSpan();
    columns = CellSpan(1, 1);
    return;
  }

  if (force_full_paint_) {
    rows = FullSectionRowSpan();
    columns = FullTableEffectiveColumnSpan();
    return;
  }

  rows = SpannedRows(damage_rect);
  columns = SpannedEffectiveColumns(damage_rect);

  // Expand by one cell in each direction to cover any collapsed borders.
  if (Table()->ShouldCollapseBorders()) {
    if (rows.Start() > 0)
      rows.DecreaseStart();
    if (rows.End() < grid_.size())
      rows.IncreaseEnd();
    if (columns.Start() > 0)
      columns.DecreaseStart();
    if (columns.End() < Table()->NumEffectiveColumns())
      columns.IncreaseEnd();
  }

  rows.EnsureConsistency(grid_.size());
  columns.EnsureConsistency(Table()->NumEffectiveColumns());

  if (!has_spanning_cells_)
    return;

  if (rows.Start() > 0 && rows.Start() < grid_.size()) {
    // If there are any cells spanning into the first row, expand |rows| to
    // cover the cells.
    unsigned n_cols = NumCols(rows.Start());
    unsigned smallest_row = rows.Start();
    for (unsigned c = columns.Start(); c < std::min(columns.End(), n_cols);
         ++c) {
      for (const auto* cell : GridCellAt(rows.Start(), c).Cells()) {
        smallest_row = std::min(smallest_row, cell->RowIndex());
        if (!smallest_row)
          break;
      }
    }
    rows = CellSpan(smallest_row, rows.End());
  }

  if (columns.Start() > 0 && columns.Start() < Table()->NumEffectiveColumns()) {
    // If there are any cells spanning into the first column, expand |columns|
    // to cover the cells.
    unsigned smallest_column = columns.Start();
    for (unsigned r = rows.Start(); r < rows.End(); ++r) {
      const auto& grid_cells = grid_[r].grid_cells;
      if (columns.Start() < grid_cells.size()) {
        unsigned c = columns.Start();
        while (c && grid_cells[c].InColSpan())
          --c;
        smallest_column = std::min(c, smallest_column);
        if (!smallest_column)
          break;
      }
    }
    columns = CellSpan(smallest_column, columns.End());
  }
}

CellSpan LayoutTableSection::SpannedRows(const LayoutRect& flipped_rect) const {
  // Find the first row that starts after rect top.
  unsigned next_row = static_cast<unsigned>(
      std::upper_bound(row_pos_.begin(), row_pos_.end(), flipped_rect.Y()) -
      row_pos_.begin());

  // After all rows.
  if (next_row == row_pos_.size())
    return CellSpan(row_pos_.size() - 1, row_pos_.size() - 1);

  unsigned start_row = next_row > 0 ? next_row - 1 : 0;

  // Find the first row that starts after rect bottom.
  unsigned end_row;
  if (row_pos_[next_row] >= flipped_rect.MaxY()) {
    end_row = next_row;
  } else {
    end_row = static_cast<unsigned>(
        std::upper_bound(row_pos_.begin() + next_row, row_pos_.end(),
                         flipped_rect.MaxY()) -
        row_pos_.begin());
    if (end_row == row_pos_.size())
      end_row = row_pos_.size() - 1;
  }

  return CellSpan(start_row, end_row);
}

CellSpan LayoutTableSection::SpannedEffectiveColumns(
    const LayoutRect& flipped_rect) const {
  const Vector<int>& column_pos = Table()->EffectiveColumnPositions();

  // Find the first column that starts after rect left.
  // lower_bound doesn't handle the edge between two cells properly as it would
  // wrongly return the cell on the logical top/left.
  // upper_bound on the other hand properly returns the cell on the logical
  // bottom/right, which also matches the behavior of other browsers.
  unsigned next_column = static_cast<unsigned>(
      std::upper_bound(column_pos.begin(), column_pos.end(), flipped_rect.X()) -
      column_pos.begin());

  if (next_column == column_pos.size())
    return CellSpan(column_pos.size() - 1,
                    column_pos.size() - 1);  // After all columns.

  unsigned start_column = next_column > 0 ? next_column - 1 : 0;

  // Find the first column that starts after rect right.
  unsigned end_column;
  if (column_pos[next_column] >= flipped_rect.MaxX()) {
    end_column = next_column;
  } else {
    end_column = static_cast<unsigned>(
        std::upper_bound(column_pos.begin() + next_column, column_pos.end(),
                         flipped_rect.MaxX()) -
        column_pos.begin());
    if (end_column == column_pos.size())
      end_column = column_pos.size() - 1;
  }

  return CellSpan(start_column, end_column);
}

void LayoutTableSection::RecalcCells() {
  DCHECK(needs_cell_recalc_);
  // We reset the flag here to ensure that |addCell| works. This is safe to do
  // as fillRowsWithDefaultStartingAtPosition makes sure we match the table's
  // columns representation.
  needs_cell_recalc_ = false;

  c_col_ = 0;
  c_row_ = 0;
  grid_.clear();

  bool resized_grid = false;
  for (LayoutTableRow* row = FirstRow(); row; row = row->NextRow()) {
    unsigned insertion_row = c_row_;
    ++c_row_;
    c_col_ = 0;
    EnsureRows(c_row_);

    grid_[insertion_row].row = row;
    row->SetRowIndex(insertion_row);
    grid_[insertion_row].SetRowLogicalHeightToRowStyleLogicalHeight();

    for (LayoutTableCell* cell = row->FirstCell(); cell;
         cell = cell->NextCell()) {
      // For rowspan, "the value zero means that the cell is to span all the
      // remaining rows in the row group." Calculate the size of the full
      // row grid now so that we can use it to count the remaining rows in
      // ResolvedRowSpan().
      if (!cell->ParsedRowSpan() && !resized_grid) {
        unsigned c_row = row->RowIndex() + 1;
        for (LayoutTableRow* remaining_row = row; remaining_row;
             remaining_row = remaining_row->NextRow())
          c_row++;
        EnsureRows(c_row);
        resized_grid = true;
      }
      AddCell(cell, row);
    }
  }

  grid_.ShrinkToFit();
  SetNeedsLayoutAndFullPaintInvalidation(layout_invalidation_reason::kUnknown);
}

// FIXME: This function could be made O(1) in certain cases (like for the
// non-most-constrainive cells' case).
void LayoutTableSection::RowLogicalHeightChanged(LayoutTableRow* row) {
  if (NeedsCellRecalc())
    return;

  unsigned row_index = row->RowIndex();
  grid_[row_index].SetRowLogicalHeightToRowStyleLogicalHeight();

  for (LayoutTableCell* cell = grid_[row_index].row->FirstCell(); cell;
       cell = cell->NextCell())
    grid_[row_index].UpdateLogicalHeightForCell(cell);
}

void LayoutTableSection::SetNeedsCellRecalc() {
  needs_cell_recalc_ = true;
  SetNeedsOverflowRecalc();
  if (LayoutTable* t = Table())
    t->SetNeedsSectionRecalc();
}

unsigned LayoutTableSection::NumEffectiveColumns() const {
  unsigned result = 0;

  for (unsigned r = 0; r < grid_.size(); ++r) {
    unsigned n_cols = NumCols(r);
    for (unsigned c = result; c < n_cols; ++c) {
      const auto& grid_cell = GridCellAt(r, c);
      if (grid_cell.HasCells() || grid_cell.InColSpan())
        result = c;
    }
  }

  return result + 1;
}

LayoutTableCell* LayoutTableSection::OriginatingCellAt(
    unsigned row,
    unsigned effective_column) {
  SECURITY_CHECK(!needs_cell_recalc_);
  if (effective_column >= NumCols(row))
    return nullptr;
  auto& grid_cell = GridCellAt(row, effective_column);
  if (grid_cell.InColSpan())
    return nullptr;
  if (auto* cell = grid_cell.PrimaryCell()) {
    if (cell->RowIndex() == row)
      return cell;
  }
  return nullptr;
}

void LayoutTableSection::AppendEffectiveColumn(unsigned pos) {
  DCHECK(!needs_cell_recalc_);

  for (auto& row : grid_)
    row.grid_cells.resize(pos + 1);
}

void LayoutTableSection::SplitEffectiveColumn(unsigned pos, unsigned first) {
  DCHECK(!needs_cell_recalc_);

  if (c_col_ > pos)
    c_col_++;
  for (unsigned row = 0; row < grid_.size(); ++row) {
    auto& grid_cells = grid_[row].grid_cells;
    EnsureCols(row, pos + 1);
    grid_cells.insert(pos + 1, TableGridCell());
    if (grid_cells[pos].HasCells()) {
      grid_cells[pos + 1].Cells().AppendVector(grid_cells[pos].Cells());
      LayoutTableCell* cell = grid_cells[pos].PrimaryCell();
      DCHECK(cell);
      DCHECK_GE(cell->ColSpan(), (grid_cells[pos].InColSpan() ? 1u : 0));
      unsigned colleft = cell->ColSpan() - grid_cells[pos].InColSpan();
      if (first > colleft)
        grid_cells[pos + 1].SetInColSpan(false);
      else
        grid_cells[pos + 1].SetInColSpan(first || grid_cells[pos].InColSpan());
    } else {
      grid_cells[pos + 1].SetInColSpan(false);
    }
  }
}

// Hit Testing
bool LayoutTableSection::NodeAtPoint(HitTestResult& result,
                                     const HitTestLocation& hit_test_location,
                                     const PhysicalOffset& accumulated_offset,
                                     HitTestAction action) {
  // If we have no children then we have nothing to do.
  if (!FirstRow())
    return false;

  DCHECK(!HasOverflowClip());

  // Table sections cannot ever be hit tested.  Effectively they do not exist.
  // Just forward to our children always.
  if (HasVisuallyOverflowingCell()) {
    for (LayoutTableRow* row = LastRow(); row; row = row->PreviousRow()) {
      if (row->HasSelfPaintingLayer())
        continue;
      PhysicalOffset row_accumulated_offset =
          accumulated_offset + row->PhysicalLocation(this);
      if (row->NodeAtPoint(result, hit_test_location, row_accumulated_offset,
                           action)) {
        UpdateHitTestResult(result,
                            hit_test_location.Point() - accumulated_offset);
        return true;
      }
    }
    return false;
  }

  RecalcCellsIfNeeded();

  PhysicalRect hit_test_rect = hit_test_location.BoundingBox();
  hit_test_rect.Move(-accumulated_offset);

  LayoutRect table_aligned_rect =
      LogicalRectForWritingModeAndDirection(hit_test_rect);
  CellSpan row_span = SpannedRows(table_aligned_rect);
  CellSpan column_span = SpannedEffectiveColumns(table_aligned_rect);

  // Now iterate over the spanned rows and columns.
  for (unsigned hit_row = row_span.Start(); hit_row < row_span.End();
       ++hit_row) {
    unsigned n_cols = NumCols(hit_row);
    for (unsigned hit_column = column_span.Start();
         hit_column < n_cols && hit_column < column_span.End(); ++hit_column) {
      auto& grid_cell = GridCellAt(hit_row, hit_column);

      // If the cell is empty, there's nothing to do
      if (!grid_cell.HasCells())
        continue;

      for (unsigned i = grid_cell.Cells().size(); i;) {
        --i;
        LayoutTableCell* cell = grid_cell.Cells()[i];
        PhysicalOffset cell_accumulated_offset =
            accumulated_offset + cell->PhysicalLocation(this);
        if (static_cast<LayoutObject*>(cell)->NodeAtPoint(
                result, hit_test_location, cell_accumulated_offset, action)) {
          UpdateHitTestResult(result,
                              hit_test_location.Point() - accumulated_offset);
          return true;
        }
      }
      if (!result.GetHitTestRequest().ListBased())
        break;
    }
    if (!result.GetHitTestRequest().ListBased())
      break;
  }

  return false;
}

LayoutTableSection* LayoutTableSection::CreateAnonymousWithParent(
    const LayoutObject* parent) {
  scoped_refptr<ComputedStyle> new_style =
      ComputedStyle::CreateAnonymousStyleWithDisplay(parent->StyleRef(),
                                                     EDisplay::kTableRowGroup);
  LayoutTableSection* new_section = new LayoutTableSection(nullptr);
  new_section->SetDocumentForAnonymous(&parent->GetDocument());
  new_section->SetStyle(std::move(new_style));
  return new_section;
}

void LayoutTableSection::SetLogicalPositionForCell(
    LayoutTableCell* cell,
    unsigned effective_column) const {
  LayoutPoint cell_location(0, row_pos_[cell->RowIndex()]);
  int16_t horizontal_border_spacing = Table()->HBorderSpacing();

  if (!TableStyle().IsLeftToRightDirection()) {
    cell_location.SetX(LayoutUnit(
        Table()->EffectiveColumnPositions()[Table()->NumEffectiveColumns()] -
        Table()->EffectiveColumnPositions()
            [Table()->AbsoluteColumnToEffectiveColumn(
                cell->AbsoluteColumnIndex() + cell->ColSpan())] +
        horizontal_border_spacing));
  } else {
    cell_location.SetX(
        LayoutUnit(Table()->EffectiveColumnPositions()[effective_column] +
                   horizontal_border_spacing));
  }

  cell->SetLogicalLocation(cell_location);
}

void LayoutTableSection::RelayoutCellIfFlexed(LayoutTableCell& cell,
                                              int row_index,
                                              int row_height) {
  // Force percent height children to lay themselves out again now that the
  // cell's final height is determined.
  // FIXME: There is still more work to do here to fully match WinIE (should
  // it become necessary to do so).  In quirks mode, WinIE behaves like we
  // do, but it will clip the cells that spill out of the table section.
  // strict mode, Mozilla and WinIE both regrow the table to accommodate the
  // new height of the cell (thus letting the percentages cause growth one
  // time only). We may also not be handling row-spanning cells correctly.

  if (!CellHasExplicitlySpecifiedHeight(cell) &&
      (Table()->StyleRef().LogicalHeight().IsAuto() ||
       row_height == cell.LogicalHeight()))
    return;

  bool any_child_needs_relayout = cell.HasPercentHeightDescendants();

  if (!any_child_needs_relayout) {
    for (LayoutObject* child = cell.FirstChild(); child;
         child = child->NextSibling()) {
      if (!child->IsText() &&
          child->StyleRef().LogicalHeight().IsPercentOrCalc() &&
          (!child->IsTable() || (!child->IsOutOfFlowPositioned() &&
                                 To<LayoutTable>(child)->HasSections()))) {
        any_child_needs_relayout = true;
        break;
      }
    }
  }

  if (!any_child_needs_relayout)
    return;

  cell.SetOverrideLogicalHeightFromRowHeight(LayoutUnit(row_height));
  cell.ForceLayout();

  // If the baseline moved, we may have to update the data for our row. Find
  // out the new baseline.
  if (cell.IsBaselineAligned()) {
    LayoutUnit baseline = cell.CellBaselinePosition();
    if (baseline > cell.BorderBefore() + cell.PaddingBefore())
      grid_[row_index].baseline = std::max(grid_[row_index].baseline, baseline);
  }
}

int LayoutTableSection::LogicalHeightForRow(
    const LayoutTableRow& row_object) const {
  unsigned row_index = row_object.RowIndex();
  DCHECK_LT(row_index, grid_.size());
  int logical_height = 0;
  for (auto& grid_cell : grid_[row_index].grid_cells) {
    const LayoutTableCell* cell = grid_cell.PrimaryCell();
    if (!cell || grid_cell.InColSpan())
      continue;
    unsigned row_span = cell->ResolvedRowSpan();
    if (row_span == 1) {
      logical_height =
          std::max(logical_height, cell->LogicalHeightForRowSizing());
      continue;
    }
    unsigned row_index_for_cell = cell->RowIndex();
    if (row_index == grid_.size() - 1 ||
        (row_span > 1 && row_index - row_index_for_cell == row_span - 1)) {
      // This is the last row of the rowspanned cell. Add extra height if
      // needed.
      if (LayoutTableRow* first_row_for_cell = grid_[row_index_for_cell].row) {
        int min_logical_height = cell->LogicalHeightForRowSizing();
        // Subtract space provided by previous rows.
        min_logical_height -= row_object.LogicalTop().ToInt() -
                              first_row_for_cell->LogicalTop().ToInt();

        logical_height = std::max(logical_height, min_logical_height);
      }
    }
  }

  if (grid_[row_index].logical_height.IsSpecified()) {
    LayoutUnit specified_logical_height =
        MinimumValueForLength(grid_[row_index].logical_height, LayoutUnit());
    // We round here to match computations for row_pos_ in
    // CalcRowLogicalHeight().
    logical_height = std::max(logical_height, specified_logical_height.Round());
  }
  return logical_height;
}

int LayoutTableSection::OffsetForRepeatedHeader() const {
  LayoutTableSection* header = Table()->Header();
  if (header && header != this)
    return Table()->RowOffsetFromRepeatingHeader().ToInt();
  LayoutState* layout_state = View()->GetLayoutState();
  return layout_state->HeightOffsetForTableHeaders().ToInt();
}

void LayoutTableSection::AdjustRowForPagination(LayoutTableRow& row_object,
                                                SubtreeLayoutScope& layouter) {
  row_object.SetPaginationStrut(LayoutUnit());
  row_object.SetLogicalHeight(LayoutUnit(LogicalHeightForRow(row_object)));
  if (!IsPageLogicalHeightKnown())
    return;
  int pagination_strut =
      PaginationStrutForRow(&row_object, row_object.LogicalTop());
  bool row_is_at_top_of_column = false;
  LayoutUnit offset_from_top_of_page;
  if (!pagination_strut) {
    LayoutUnit page_logical_height =
        PageLogicalHeightForOffset(row_object.LogicalTop());
    if (OffsetForRepeatedHeader()) {
      offset_from_top_of_page =
          page_logical_height -
          PageRemainingLogicalHeightForOffset(row_object.LogicalTop(),
                                              kAssociateWithLatterPage);
      row_is_at_top_of_column =
          !offset_from_top_of_page ||
          offset_from_top_of_page <= OffsetForRepeatedHeader() ||
          offset_from_top_of_page <= Table()->VBorderSpacing();
    }

    if (!row_is_at_top_of_column)
      return;
  }
  // We need to push this row to the next fragmentainer. If there are repeated
  // table headers, we need to make room for those at the top of the next
  // fragmentainer, above this row. Otherwise, this row will just go at the top
  // of the next fragmentainer.

  // Border spacing from the previous row has pushed this row just past the top
  // of the page, so we must reposition it to the top of the page and avoid any
  // repeating header.
  if (row_is_at_top_of_column && offset_from_top_of_page)
    pagination_strut -= offset_from_top_of_page.ToInt();

  // If we have a header group we will paint it at the top of each page,
  // move the rows down to accommodate it.
  int additional_adjustment = OffsetForRepeatedHeader();

  // If the table collapses borders, push the row down by the max height of the
  // outer half borders to make the whole collapsed borders on the next page.
  if (Table()->ShouldCollapseBorders()) {
    for (const auto* cell = row_object.FirstCell(); cell;
         cell = cell->NextCell()) {
      additional_adjustment = std::max<int>(additional_adjustment,
                                            cell->CollapsedOuterBorderBefore());
    }
  }

  pagination_strut += additional_adjustment;
  row_object.SetPaginationStrut(LayoutUnit(pagination_strut));

  // We have inserted a pagination strut before the row. Adjust the logical top
  // and re-lay out. We no longer want to break inside the row, but rather
  // *before* it. From the previous layout pass, there are most likely
  // pagination struts inside some cell in this row that we need to get rid of.
  row_object.SetLogicalTop(row_object.LogicalTop() + pagination_strut);
  layouter.SetChildNeedsLayout(&row_object);
  row_object.LayoutIfNeeded();

  // It's very likely that re-laying out (and nuking pagination struts inside
  // cells) gave us a new height.
  row_object.SetLogicalHeight(LayoutUnit(LogicalHeightForRow(row_object)));
}

bool LayoutTableSection::GroupShouldRepeat() const {
  DCHECK(Table()->Header() == this || Table()->Footer() == this);
  if (GetPaginationBreakability() == kAllowAnyBreaks)
    return false;

  // If we don't know the page height yet, just assume we fit.
  if (!IsPageLogicalHeightKnown())
    return true;
  LayoutUnit page_height = PageLogicalHeightForOffset(LayoutUnit());

  LayoutUnit logical_height = LogicalHeight() - OffsetForRepeatedHeader();
  if (logical_height > page_height)
    return false;

  // See https://drafts.csswg.org/css-tables-3/#repeated-headers which says
  // a header/footer can repeat if it takes up less than a quarter of the page.
  if (logical_height > 0 && page_height / logical_height < 4)
    return false;

  return true;
}

bool LayoutTableSection::MapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    VisualRectFlags flags) const {
  if (ancestor == this)
    return true;
  // Repeating table headers and footers are painted once per
  // page/column. So we need to use the rect for the entire table because
  // the repeating headers/footers will appear throughout it.
  // This does not go through the regular fragmentation machinery, so we need
  // special code to expand the invalidation rect to contain all positions of
  // the header in all columns.
  // Note that this is in flow thread coordinates, not visual coordinates. The
  // enclosing LayoutFlowThread will convert to visual coordinates.
  if (IsRepeatingHeaderGroup() || IsRepeatingFooterGroup()) {
    transform_state.Flatten();
    FloatRect rect = transform_state.LastPlanarQuad().BoundingBox();
    rect.SetHeight(Table()->LogicalHeight());
    transform_state.SetQuad(FloatQuad(rect));
    return Table()->MapToVisualRectInAncestorSpaceInternal(
        ancestor, transform_state, flags);
  }
  return LayoutTableBoxComponent::MapToVisualRectInAncestorSpaceInternal(
      ancestor, transform_state, flags);
}

bool LayoutTableSection::PaintedOutputOfObjectHasNoEffectRegardlessOfSize()
    const {
  // LayoutTableSection paints background from columns.
  if (Table()->HasColElements())
    return false;
  return LayoutTableBoxComponent::
      PaintedOutputOfObjectHasNoEffectRegardlessOfSize();
}

}  // namespace blink
