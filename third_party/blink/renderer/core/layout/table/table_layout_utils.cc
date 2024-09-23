// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/table/table_layout_utils.h"

#include "third_party/blink/renderer/core/layout/block_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_column.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_column_visitor.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/table/table_borders.h"
#include "third_party/blink/renderer/core/layout/table/table_node.h"

namespace blink {

namespace {

// We cannot distribute space to mergeable columns. Mark at least one of the
// spanned columns as distributable (i.e. non-mergeable).
//
// We'll mark the first (non-collapsed) column as non-mergeable. We should only
// merge adjacent columns that have no cells that start there.
//
// Example:
//
//             +------------------------+------------------------+
//             |          cell          |         cell           |
//    row 1    |       colspan 2        |       colspan 2        |
//             |                        |                        |
//             +------------+-----------+-----------+------------+
//             |    cell    |         cell          |   cell     |
//    row 2    | colspan 1  |       colspan 2       | colspan 1  |
//             |            |                       |            |
//             +------------+-----------------------+------------+
//
//   columns   |  column 1  |  column 2 | column 3  |  column 4  |
//
// No columns should be merged here, as there are no columns that has no cell
// starting there. We want all four columns to receive some space, or
// distribution would be uneven.
//
// Another interesting problem being solved here is the interaction between
// collapsed (visibility:collapse) and mergeable columns. We need to find the
// first column that isn't collapsed and mark it as non-mergeable. Otherwise the
// entire cell might merge into the first column, and collapse, and the whole
// cell would be hidden if the first column is collapsed.
//
// If all columns spanned actually collapse, the first column will be marked as
// non-meargeable.
void EnsureDistributableColumnExists(wtf_size_t start_column_index,
                                     wtf_size_t span,
                                     TableTypes::Columns* column_constraints) {
  DCHECK_LT(start_column_index, column_constraints->data.size());
  DCHECK_GT(span, 1u);

  wtf_size_t effective_span =
      std::min(span, column_constraints->data.size() - start_column_index);
  TableTypes::Column* start_column =
      &column_constraints->data[start_column_index];
  TableTypes::Column* end_column = start_column + effective_span;
  for (TableTypes::Column* column = start_column; column != end_column;
       ++column) {
    if (!column->is_collapsed) {
      column->is_mergeable = false;
      return;
    }
  }
  // We didn't find any non-collapsed column. Mark the first one as
  // non-mergeable.
  start_column->is_mergeable = false;
}

// Applies cell/wide cell constraints to columns.
// Guarantees columns min/max widths have non-empty values.
void ApplyCellConstraintsToColumnConstraints(
    const TableTypes::CellInlineConstraints& cell_constraints,
    LayoutUnit inline_border_spacing,
    bool is_fixed_layout,
    TableTypes::ColspanCells* colspan_cell_constraints,
    TableTypes::Columns* column_constraints) {
  // Satisfy prerequisites for cell merging:

  if (column_constraints->data.size() < cell_constraints.size()) {
    // Column constraint must exist for each cell.
    TableTypes::Column default_column;
    default_column.is_table_fixed = is_fixed_layout;
    default_column.is_mergeable = !is_fixed_layout;
    wtf_size_t column_count =
        cell_constraints.size() - column_constraints->data.size();
    // Must loop because WTF::Vector does not support resize with default value.
    for (wtf_size_t i = 0; i < column_count; ++i)
      column_constraints->data.push_back(default_column);
    DCHECK_EQ(column_constraints->data.size(), cell_constraints.size());

  } else if (column_constraints->data.size() > cell_constraints.size()) {
    // Trim mergeable columns off the end.
    wtf_size_t last_non_merged_column = column_constraints->data.size() - 1;
    while (last_non_merged_column + 1 > cell_constraints.size() &&
           column_constraints->data[last_non_merged_column].is_mergeable) {
      --last_non_merged_column;
    }
    column_constraints->data.resize(last_non_merged_column + 1);
    DCHECK_GE(column_constraints->data.size(), cell_constraints.size());
  }
  // Make sure there exists a non-mergeable column for each colspanned cell.
  for (const TableTypes::ColspanCell& colspan_cell :
       *colspan_cell_constraints) {
    EnsureDistributableColumnExists(colspan_cell.start_column,
                                    colspan_cell.span, column_constraints);
  }

  // Distribute cell constraints to column constraints.
  for (wtf_size_t i = 0; i < cell_constraints.size(); ++i) {
    column_constraints->data[i].Encompass(cell_constraints[i]);
  }

  // Wide cell constraints are sorted by span length/starting column.
  auto colspan_cell_less_than = [](const TableTypes::ColspanCell& lhs,
                                   const TableTypes::ColspanCell& rhs) {
    if (lhs.span == rhs.span)
      return lhs.start_column < rhs.start_column;
    return lhs.span < rhs.span;
  };
  std::stable_sort(colspan_cell_constraints->begin(),
                   colspan_cell_constraints->end(), colspan_cell_less_than);

  DistributeColspanCellsToColumns(*colspan_cell_constraints,
                                  inline_border_spacing, is_fixed_layout,
                                  column_constraints);

  // Column total percentage inline-size is clamped to 100%.
  // Auto tables: max(0, 100% minus the sum of percentages of all
  //   prior columns in the table)
  // Fixed tables: scale all percentage columns so that total percentage
  //   is 100%.
  float total_percentage = 0;
  for (TableTypes::Column& column : column_constraints->data) {
    if (column.percent) {
      if (!is_fixed_layout && (*column.percent + total_percentage > 100.0))
        column.percent = 100 - total_percentage;
      total_percentage += *column.percent;
    }
    // A column may have no min/max inline-sizes if there are no cells in this
    // column. E.g. a cell has a large colspan which no other cell belongs to.
    column.min_inline_size = column.min_inline_size.value_or(LayoutUnit());
    column.max_inline_size = column.max_inline_size.value_or(LayoutUnit());
  }

  if (is_fixed_layout && total_percentage > 100.0) {
    for (TableTypes::Column& column : column_constraints->data) {
      if (column.percent)
        column.percent = *column.percent * 100 / total_percentage;
    }
  }
}

template <typename RowCountFunc>
TableTypes::Row ComputeMinimumRowBlockSize(
    const RowCountFunc& row_count_func,
    const BlockNode& row,
    const LayoutUnit cell_percentage_inline_size,
    const bool is_table_block_size_specified,
    const Vector<TableColumnLocation>& column_locations,
    const TableBorders& table_borders,
    wtf_size_t start_row_index,
    wtf_size_t row_index,
    wtf_size_t section_index,
    bool is_section_collapsed,
    TableTypes::CellBlockConstraints* cell_block_constraints,
    TableTypes::RowspanCells* rowspan_cells,
    ColspanCellTabulator* colspan_cell_tabulator) {
  const WritingDirectionMode table_writing_direction =
      row.Style().GetWritingDirection();
  const bool has_collapsed_borders = table_borders.IsCollapsed();

  // TODO(layout-ng) Scrollbars should be frozen when computing row sizes.
  // This cannot be done today, because fragments with frozen scrollbars
  // will be cached. Needs to be fixed in NG framework.

  LayoutUnit max_cell_block_size;
  std::optional<float> row_percent;
  bool is_constrained = false;
  bool has_rowspan_start = false;
  wtf_size_t start_cell_index = cell_block_constraints->size();
  RowBaselineTabulator row_baseline_tabulator;

  // Gather block sizes of all cells.
  for (BlockNode cell = To<BlockNode>(row.FirstChild()); cell;
       cell = To<BlockNode>(cell.NextSibling())) {
    colspan_cell_tabulator->FindNextFreeColumn();
    const ComputedStyle& cell_style = cell.Style();
    const auto cell_writing_direction = cell_style.GetWritingDirection();
    const BoxStrut cell_borders = table_borders.CellBorder(
        cell, row_index, colspan_cell_tabulator->CurrentColumn(), section_index,
        table_writing_direction);

    // Clamp the rowspan if it exceeds the total section row-count.
    wtf_size_t effective_rowspan = cell.TableCellRowspan();
    if (effective_rowspan > 1) {
      const wtf_size_t max_rows =
          row_count_func() - (row_index - start_row_index);
      effective_rowspan = std::min(max_rows, effective_rowspan);
    }

    ConstraintSpaceBuilder space_builder(
        table_writing_direction.GetWritingMode(), cell_writing_direction,
        /* is_new_fc */ true);

    // We want these values to match the "layout" pass as close as possible.
    SetupTableCellConstraintSpaceBuilder(
        table_writing_direction, cell, cell_borders, column_locations,
        /* cell_block_size */ kIndefiniteSize, cell_percentage_inline_size,
        /* alignment_baseline */ std::nullopt,
        colspan_cell_tabulator->CurrentColumn(),
        /* is_initial_block_size_indefinite */ true,
        is_table_block_size_specified, has_collapsed_borders,
        LayoutResultCacheSlot::kMeasure, &space_builder);

    const auto cell_space = space_builder.ToConstraintSpace();
    const LayoutResult* layout_result = cell.Layout(cell_space);

    const LogicalBoxFragment fragment(
        table_writing_direction,
        To<PhysicalBoxFragment>(layout_result->GetPhysicalFragment()));
    const Length& cell_specified_block_length =
        IsParallelWritingMode(table_writing_direction.GetWritingMode(),
                              cell_style.GetWritingMode())
            ? cell_style.LogicalHeight()
            : cell_style.LogicalWidth();

    bool has_descendant_that_depends_on_percentage_block_size =
        layout_result->HasDescendantThatDependsOnPercentageBlockSize();
    bool has_effective_rowspan = effective_rowspan > 1;

    TableTypes::CellBlockConstraint cell_block_constraint = {
        fragment.BlockSize(),
        cell_borders,
        colspan_cell_tabulator->CurrentColumn(),
        effective_rowspan,
        cell_specified_block_length.IsFixed(),
        has_descendant_that_depends_on_percentage_block_size};
    colspan_cell_tabulator->ProcessCell(cell);
    cell_block_constraints->push_back(cell_block_constraint);
    is_constrained |=
        cell_block_constraint.is_constrained && !has_effective_rowspan;
    row_baseline_tabulator.ProcessCell(
        fragment, ComputeContentAlignmentForTableCell(cell_style),
        has_effective_rowspan,
        has_descendant_that_depends_on_percentage_block_size);

    // Compute cell's css block size.
    std::optional<LayoutUnit> cell_css_block_size;
    std::optional<float> cell_css_percent;

    // TODO(1105272) Handle cell_specified_block_length.IsCalculated()
    if (cell_specified_block_length.IsPercent()) {
      cell_css_percent = cell_specified_block_length.Percent();
    } else if (cell_specified_block_length.IsFixed()) {
      // NOTE: Ignore min/max-height for determining the |cell_css_block_size|.
      BoxStrut cell_padding = ComputePadding(cell_space, cell_style);
      BoxStrut border_padding = cell_borders + cell_padding;
      // https://quirks.spec.whatwg.org/#the-table-cell-height-box-sizing-quirk
      if (cell.GetDocument().InQuirksMode() ||
          cell_style.BoxSizing() == EBoxSizing::kBorderBox) {
        cell_css_block_size =
            std::max(border_padding.BlockSum(),
                     LayoutUnit(cell_specified_block_length.Value()));
      } else {
        cell_css_block_size = border_padding.BlockSum() +
                              LayoutUnit(cell_specified_block_length.Value());
      }
    }

    if (!has_effective_rowspan) {
      if (cell_css_block_size || cell_css_percent)
        is_constrained = true;
      if (cell_css_percent)
        row_percent = std::max(row_percent.value_or(0), *cell_css_percent);
      // Cell's block layout ignores CSS block size properties. Row must use it
      // to compute it's minimum block size.
      max_cell_block_size =
          std::max({max_cell_block_size, cell_block_constraint.min_block_size,
                    cell_css_block_size.value_or(LayoutUnit())});
    } else {
      has_rowspan_start = true;
      LayoutUnit min_block_size = cell_block_constraint.min_block_size;
      if (cell_css_block_size)
        min_block_size = std::max(min_block_size, *cell_css_block_size);
      rowspan_cells->push_back(TableTypes::RowspanCell{
          row_index, effective_rowspan, min_block_size});
    }
  }

  // Apply row's CSS block size.
  const Length& row_specified_block_length = row.Style().LogicalHeight();
  if (row_specified_block_length.IsPercent()) {
    is_constrained = true;
    row_percent =
        std::max(row_percent.value_or(0), row_specified_block_length.Percent());
  } else if (row_specified_block_length.IsFixed()) {
    is_constrained = true;
    max_cell_block_size = std::max(
        LayoutUnit(row_specified_block_length.Value()), max_cell_block_size);
  }

  const LayoutUnit row_block_size =
      row_baseline_tabulator.ComputeRowBlockSize(max_cell_block_size);
  std::optional<LayoutUnit> row_baseline;
  if (!row_baseline_tabulator.BaselineDependsOnPercentageBlockDescendant())
    row_baseline = row_baseline_tabulator.ComputeBaseline(row_block_size);

  return TableTypes::Row{
      row_block_size,
      start_cell_index,
      cell_block_constraints->size() - start_cell_index,
      row_baseline,
      row_percent,
      is_constrained,
      has_rowspan_start,
      /* is_collapsed */ is_section_collapsed ||
          row.Style().UsedVisibility() == EVisibility::kCollapse};
}

// Computes inline constraints for COLGROUP/COLs.
class ColumnConstraintsBuilder {
 public:
  void VisitCol(const LayoutInputNode& column,
                wtf_size_t start_column_index,
                wtf_size_t span) {
    // COL creates SPAN constraints. Its width is col css width, or enclosing
    // colgroup css width.
    TableTypes::Column col_constraint =
        TableTypes::CreateColumn(column.Style(),
                                 !is_fixed_layout_ && colgroup_constraint_
                                     ? colgroup_constraint_->max_inline_size
                                     : std::nullopt,
                                 is_fixed_layout_);
    for (wtf_size_t i = 0; i < span; ++i)
      column_constraints_->data.push_back(col_constraint);
    column.GetLayoutBox()->ClearNeedsLayout();
  }

  void EnterColgroup(const LayoutInputNode& colgroup,
                     wtf_size_t start_column_index) {
    colgroup_constraint_ = TableTypes::CreateColumn(
        colgroup.Style(), std::nullopt, is_fixed_layout_);
  }

  void LeaveColgroup(const LayoutInputNode& colgroup,
                     wtf_size_t start_column_index,
                     wtf_size_t span,
                     bool has_children) {
    if (!has_children) {
      for (wtf_size_t i = 0; i < span; ++i)
        column_constraints_->data.push_back(*colgroup_constraint_);
    }
    colgroup_constraint_.reset();
    colgroup.GetLayoutBox()->ClearNeedsLayout();
    To<LayoutTableColumn>(colgroup.GetLayoutBox())
        ->ClearNeedsLayoutForChildren();
  }

  ColumnConstraintsBuilder(TableTypes::Columns* column_constraints,
                           bool is_fixed_layout)
      : column_constraints_(column_constraints),
        is_fixed_layout_(is_fixed_layout) {}

 private:
  TableTypes::Columns* column_constraints_;
  bool is_fixed_layout_;
  std::optional<TableTypes::Column> colgroup_constraint_;
};

// Computes constraints specified on column elements.
void ComputeColumnElementConstraints(const HeapVector<BlockNode>& columns,
                                     bool is_fixed_layout,
                                     TableTypes::Columns* column_constraints) {
  ColumnConstraintsBuilder constraints_builder(column_constraints,
                                               is_fixed_layout);
  // |table_column_count| is UINT_MAX because columns will get trimmed later.
  VisitLayoutTableColumn(columns, UINT_MAX, &constraints_builder);
}

void ComputeSectionInlineConstraints(
    const BlockNode& section,
    bool is_fixed_layout,
    bool is_first_section,
    WritingDirectionMode table_writing_direction,
    const TableBorders& table_borders,
    wtf_size_t section_index,
    wtf_size_t* row_index,
    TableTypes::CellInlineConstraints* cell_inline_constraints,
    TableTypes::ColspanCells* colspan_cell_inline_constraints) {
  ColspanCellTabulator colspan_cell_tabulator;
  bool is_first_row = true;
  for (BlockNode row = To<BlockNode>(section.FirstChild()); row;
       row = To<BlockNode>(row.NextSibling())) {
    colspan_cell_tabulator.StartRow();

    // Gather constraints for each cell, and merge them into
    // CellInlineConstraints.
    for (BlockNode cell = To<BlockNode>(row.FirstChild()); cell;
         cell = To<BlockNode>(cell.NextSibling())) {
      colspan_cell_tabulator.FindNextFreeColumn();
      wtf_size_t colspan = cell.TableCellColspan();

      bool ignore_because_of_fixed_layout =
          is_fixed_layout && (!is_first_section || !is_first_row);

      wtf_size_t max_column = ComputeMaxColumn(
          colspan_cell_tabulator.CurrentColumn(), colspan, is_fixed_layout);
      if (max_column >= cell_inline_constraints->size())
        cell_inline_constraints->Grow(max_column);
      if (!ignore_because_of_fixed_layout) {
        BoxStrut cell_border = table_borders.CellBorder(
            cell, *row_index, colspan_cell_tabulator.CurrentColumn(),
            section_index, table_writing_direction);
        BoxStrut cell_padding = table_borders.CellPaddingForMeasure(
            cell.Style(), table_writing_direction);
        TableTypes::CellInlineConstraint cell_constraint =
            TableTypes::CreateCellInlineConstraint(
                cell, table_writing_direction, is_fixed_layout, cell_border,
                cell_padding);
        if (colspan == 1) {
          std::optional<TableTypes::CellInlineConstraint>& constraint =
              (*cell_inline_constraints)[colspan_cell_tabulator
                                             .CurrentColumn()];
          // Standard cell, update final column inline size values.
          if (constraint.has_value()) {
            constraint->Encompass(cell_constraint);
          } else {
            constraint = cell_constraint;
          }
        } else {
          colspan_cell_inline_constraints->emplace_back(
              cell_constraint, colspan_cell_tabulator.CurrentColumn(), colspan);
        }
      }
      colspan_cell_tabulator.ProcessCell(cell);
    }
    is_first_row = false;
    *row_index += 1;
    colspan_cell_tabulator.EndRow();
  }
}

// Implements spec distribution algorithm:
// https://www.w3.org/TR/css-tables-3/#width-distribution-algorithm
// |treat_target_size_as_constrained| constrained target can grow fixed-width
// columns. unconstrained target cannot grow fixed-width columns beyond
// specified size.
Vector<LayoutUnit> DistributeInlineSizeToComputedInlineSizeAuto(
    LayoutUnit target_inline_size,
    const TableTypes::Column* start_column,
    const TableTypes::Column* end_column,
    const bool treat_target_size_as_constrained) {
  unsigned all_columns_count = 0;
  unsigned percent_columns_count = 0;
  unsigned fixed_columns_count = 0;
  unsigned auto_columns_count = 0;
  // What guesses mean is described in table specification.
  // https://www.w3.org/TR/css-tables-3/#width-distribution-algorithm
  enum { kMinGuess, kPercentageGuess, kSpecifiedGuess, kMaxGuess, kAboveMax };
  // sizes are collected for all guesses except kAboveMax
  LayoutUnit guess_sizes[kAboveMax];
  LayoutUnit guess_size_total_increases[kAboveMax];
  float total_percent = 0.0f;
  LayoutUnit total_auto_max_inline_size;
  LayoutUnit total_fixed_max_inline_size;

  for (const TableTypes::Column* column = start_column; column != end_column;
       ++column) {
    all_columns_count++;
    DCHECK(column->min_inline_size);
    DCHECK(column->max_inline_size);

    // Mergeable columns are ignored.
    if (column->is_mergeable) {
      continue;
    }

    if (column->percent) {
      percent_columns_count++;
      total_percent += *column->percent;
      LayoutUnit percent_inline_size =
          column->ResolvePercentInlineSize(target_inline_size);
      guess_sizes[kMinGuess] += *column->min_inline_size;
      guess_sizes[kPercentageGuess] += percent_inline_size;
      guess_sizes[kSpecifiedGuess] += percent_inline_size;
      guess_sizes[kMaxGuess] += percent_inline_size;
      guess_size_total_increases[kPercentageGuess] +=
          percent_inline_size - *column->min_inline_size;
    } else if (column->is_constrained) {  // Fixed column
      fixed_columns_count++;
      total_fixed_max_inline_size += *column->max_inline_size;
      guess_sizes[kMinGuess] += *column->min_inline_size;
      guess_sizes[kPercentageGuess] += *column->min_inline_size;
      guess_sizes[kSpecifiedGuess] += *column->max_inline_size;
      guess_sizes[kMaxGuess] += *column->max_inline_size;
      guess_size_total_increases[kSpecifiedGuess] +=
          *column->max_inline_size - *column->min_inline_size;
    } else {  // Auto column
      auto_columns_count++;
      total_auto_max_inline_size += *column->max_inline_size;
      guess_sizes[kMinGuess] += *column->min_inline_size;
      guess_sizes[kPercentageGuess] += *column->min_inline_size;
      guess_sizes[kSpecifiedGuess] += *column->min_inline_size;
      guess_sizes[kMaxGuess] += *column->max_inline_size;
      guess_size_total_increases[kMaxGuess] +=
          *column->max_inline_size - *column->min_inline_size;
    }
  }

  Vector<LayoutUnit> computed_sizes;
  computed_sizes.resize(all_columns_count);

  // Distributing inline sizes can never cause cells to be < min_inline_size.
  // Target inline size must be wider than sum of min inline sizes.
  // This is always true for assignable_table_inline_size, but not for
  // colspan_cells.
  target_inline_size = std::max(target_inline_size, guess_sizes[kMinGuess]);

  unsigned starting_guess = kAboveMax;
  for (unsigned i = kMinGuess; i != kAboveMax; ++i) {
    if (guess_sizes[i] >= target_inline_size) {
      starting_guess = i;
      break;
    }
  }

  switch (starting_guess) {
    case kMinGuess: {
      // All columns are their min inline-size.
      LayoutUnit* computed_size = computed_sizes.data();
      for (const TableTypes::Column* column = start_column;
           column != end_column; ++column, ++computed_size) {
        if (column->is_mergeable) {
          continue;
        }
        *computed_size = column->min_inline_size.value_or(LayoutUnit());
      }
    } break;
    case kPercentageGuess: {
      // Percent columns grow in proportion to difference between their
      // percentage size and their minimum size.
      LayoutUnit percent_inline_size_increase =
          guess_size_total_increases[kPercentageGuess];
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kMinGuess];
      LayoutUnit remaining_deficit = distributable_inline_size;
      LayoutUnit* computed_size = computed_sizes.data();
      LayoutUnit* last_computed_size = nullptr;
      for (const TableTypes::Column* column = start_column;
           column != end_column; ++column, ++computed_size) {
        if (column->is_mergeable) {
          continue;
        }
        if (column->percent) {
          last_computed_size = computed_size;
          LayoutUnit percent_inline_size =
              column->ResolvePercentInlineSize(target_inline_size);
          LayoutUnit column_inline_size_increase =
              percent_inline_size - *column->min_inline_size;
          LayoutUnit delta;
          if (percent_inline_size_increase > LayoutUnit()) {
            delta = distributable_inline_size.MulDiv(
                column_inline_size_increase, percent_inline_size_increase);
          } else {
            delta = distributable_inline_size / percent_columns_count;
          }
          remaining_deficit -= delta;
          *computed_size = *column->min_inline_size + delta;
        } else {
          // Auto/Fixed columns get their min inline-size.
          *computed_size = *column->min_inline_size;
        }
      }
      if (remaining_deficit != LayoutUnit()) {
        DCHECK(last_computed_size);
        *last_computed_size += remaining_deficit;
      }
    } break;
    case kSpecifiedGuess: {
      // Fixed columns grow, auto gets min, percent gets %max.
      LayoutUnit fixed_inline_size_increase =
          guess_size_total_increases[kSpecifiedGuess];
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kPercentageGuess];
      LayoutUnit remaining_deficit = distributable_inline_size;
      LayoutUnit* last_computed_size = nullptr;
      LayoutUnit* computed_size = computed_sizes.data();
      for (const TableTypes::Column* column = start_column;
           column != end_column; ++column, ++computed_size) {
        if (column->is_mergeable) {
          continue;
        }
        if (column->percent) {
          *computed_size = column->ResolvePercentInlineSize(target_inline_size);
        } else if (column->is_constrained) {
          last_computed_size = computed_size;
          LayoutUnit column_inline_size_increase =
              *column->max_inline_size - *column->min_inline_size;
          LayoutUnit delta;
          if (fixed_inline_size_increase > LayoutUnit()) {
            delta = distributable_inline_size.MulDiv(
                column_inline_size_increase, fixed_inline_size_increase);
          } else {
            delta = distributable_inline_size / fixed_columns_count;
          }
          remaining_deficit -= delta;
          *computed_size = *column->min_inline_size + delta;
        } else {
          *computed_size = *column->min_inline_size;
        }
      }
      if (remaining_deficit != LayoutUnit()) {
        DCHECK(last_computed_size);
        *last_computed_size += remaining_deficit;
      }
    } break;
    case kMaxGuess: {
      // Auto columns grow, fixed gets max, percent gets %max.
      LayoutUnit auto_inline_size_increase =
          guess_size_total_increases[kMaxGuess];
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kSpecifiedGuess];
      // When the inline-sizes match exactly, this usually means that table
      // inline-size is auto, and that columns should be wide enough to
      // accommodate content without wrapping.
      // Instead of using the distributing math to compute final column
      // inline-size, we use the max inline-size. Using distributing math can
      // cause rounding errors, and unintended line wrap.
      bool is_exact_match = target_inline_size == guess_sizes[kMaxGuess];
      LayoutUnit remaining_deficit =
          is_exact_match ? LayoutUnit() : distributable_inline_size;
      LayoutUnit* last_computed_size = nullptr;
      LayoutUnit* computed_size = computed_sizes.data();
      for (const TableTypes::Column* column = start_column;
           column != end_column; ++column, ++computed_size) {
        if (column->is_mergeable) {
          continue;
        }
        if (column->percent) {
          *computed_size = column->ResolvePercentInlineSize(target_inline_size);
        } else if (column->is_constrained || is_exact_match) {
          *computed_size = *column->max_inline_size;
        } else {
          last_computed_size = computed_size;
          LayoutUnit column_inline_size_increase =
              *column->max_inline_size - *column->min_inline_size;
          LayoutUnit delta;
          if (auto_inline_size_increase > LayoutUnit()) {
            delta = distributable_inline_size.MulDiv(
                column_inline_size_increase, auto_inline_size_increase);
          } else {
            delta = distributable_inline_size / auto_columns_count;
          }
          remaining_deficit -= delta;
          *computed_size = *column->min_inline_size + delta;
        }
      }
      if (remaining_deficit != LayoutUnit()) {
        DCHECK(last_computed_size);
        *last_computed_size += remaining_deficit;
      }
    } break;
    case kAboveMax: {
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kMaxGuess];
      if (auto_columns_count > 0) {
        // Grow auto columns if available.
        LayoutUnit remaining_deficit = distributable_inline_size;
        LayoutUnit* last_computed_size = nullptr;
        LayoutUnit* computed_size = computed_sizes.data();
        for (const TableTypes::Column* column = start_column;
             column != end_column; ++column, ++computed_size) {
          if (column->is_mergeable) {
            continue;
          }
          if (column->percent) {
            *computed_size =
                column->ResolvePercentInlineSize(target_inline_size);
          } else if (column->is_constrained) {
            *computed_size = *column->max_inline_size;
          } else {
            last_computed_size = computed_size;
            LayoutUnit delta;
            if (total_auto_max_inline_size > LayoutUnit()) {
              delta = distributable_inline_size.MulDiv(
                  *column->max_inline_size, total_auto_max_inline_size);
            } else {
              delta = distributable_inline_size / auto_columns_count;
            }
            remaining_deficit -= delta;
            *computed_size = *column->max_inline_size + delta;
          }
        }
        if (remaining_deficit != LayoutUnit()) {
          DCHECK(last_computed_size);
          *last_computed_size += remaining_deficit;
        }
      } else if (fixed_columns_count > 0 && treat_target_size_as_constrained) {
        // Grow fixed columns if available.
        LayoutUnit remaining_deficit = distributable_inline_size;
        LayoutUnit* last_computed_size = nullptr;
        LayoutUnit* computed_size = computed_sizes.data();
        for (const TableTypes::Column* column = start_column;
             column != end_column; ++column, ++computed_size) {
          if (column->is_mergeable) {
            continue;
          }
          if (column->percent) {
            *computed_size =
                column->ResolvePercentInlineSize(target_inline_size);
          } else if (column->is_constrained) {
            last_computed_size = computed_size;
            LayoutUnit delta;
            if (total_fixed_max_inline_size > LayoutUnit()) {
              delta = distributable_inline_size.MulDiv(
                  *column->max_inline_size, total_fixed_max_inline_size);
            } else {
              delta = distributable_inline_size / fixed_columns_count;
            }
            remaining_deficit -= delta;
            *computed_size = *column->max_inline_size + delta;
          } else {
            NOTREACHED_IN_MIGRATION();
          }
        }
        if (remaining_deficit != LayoutUnit()) {
          DCHECK(last_computed_size);
          *last_computed_size += remaining_deficit;
        }
      } else if (percent_columns_count > 0) {
        // All remaining columns are percent.
        // They grow to max(col minimum, %ge size) + additional size
        // proportional to column percent.
        LayoutUnit remaining_deficit = distributable_inline_size;
        LayoutUnit* last_computed_size = nullptr;
        LayoutUnit* computed_size = computed_sizes.data();
        for (const TableTypes::Column* column = start_column;
             column != end_column; ++column, ++computed_size) {
          if (column->is_mergeable || !column->percent) {
            continue;
          }
          last_computed_size = computed_size;
          LayoutUnit percent_inline_size =
              column->ResolvePercentInlineSize(target_inline_size);
          LayoutUnit delta;
          if (total_percent != 0.0f) {
            delta = LayoutUnit(distributable_inline_size * *column->percent /
                               total_percent);
          } else {
            delta = distributable_inline_size / percent_columns_count;
          }
          remaining_deficit -= delta;
          *computed_size = percent_inline_size + delta;
        }
        if (remaining_deficit != LayoutUnit() && last_computed_size) {
          *last_computed_size += remaining_deficit;
        }
      }
    }
  }
  return computed_sizes;
}

Vector<LayoutUnit> SynchronizeAssignableTableInlineSizeAndColumnsFixed(
    LayoutUnit target_inline_size,
    const TableTypes::Columns& column_constraints) {
  unsigned all_columns_count = 0;
  unsigned percent_columns_count = 0;
  unsigned auto_columns_count = 0;
  unsigned fixed_columns_count = 0;
  unsigned zero_inline_size_constrained_colums_count = 0;

  auto TreatAsFixed = [](const TableTypes::Column& column) {
    // Columns of width 0 are treated as auto by all browsers.
    return column.IsFixed() && column.max_inline_size != LayoutUnit();
  };

  auto IsZeroInlineSizeConstrained = [](const TableTypes::Column& column) {
    // Columns of width 0 are treated as auto by all browsers.
    return column.is_constrained && column.max_inline_size == LayoutUnit();
  };

  LayoutUnit total_percent_inline_size;
  LayoutUnit total_auto_max_inline_size;
  LayoutUnit total_fixed_inline_size;
  LayoutUnit assigned_inline_size;
  Vector<LayoutUnit> column_sizes;
  column_sizes.resize(column_constraints.data.size());
  for (const TableTypes::Column& column : column_constraints.data) {
    all_columns_count++;
    if (column.percent) {
      percent_columns_count++;
      total_percent_inline_size +=
          column.ResolvePercentInlineSize(target_inline_size);
    } else if (TreatAsFixed(column)) {
      fixed_columns_count++;
      total_fixed_inline_size += column.max_inline_size.value_or(LayoutUnit());
    } else if (IsZeroInlineSizeConstrained(column)) {
      zero_inline_size_constrained_colums_count++;
    } else {
      auto_columns_count++;
      total_auto_max_inline_size +=
          column.max_inline_size.value_or(LayoutUnit());
    }
  }

  LayoutUnit* last_column_size = nullptr;
  // Distribute to fixed columns.
  if (fixed_columns_count > 0) {
    float scale = 1.0f;
    bool scale_available = true;
    LayoutUnit target_fixed_size =
        (target_inline_size - total_percent_inline_size).ClampNegativeToZero();
    bool scale_up =
        total_fixed_inline_size < target_fixed_size && auto_columns_count == 0;
    // Fixed columns grow if there are no auto columns. They fill up space not
    // taken up by percentage columns.
    bool scale_down = total_fixed_inline_size > target_inline_size;
    if (scale_up || scale_down) {
      if (total_fixed_inline_size != LayoutUnit()) {
        scale = target_fixed_size.ToFloat() / total_fixed_inline_size;
      } else {
        scale_available = false;
      }
    }
    LayoutUnit* column_size = column_sizes.data();
    for (auto column = column_constraints.data.begin();
         column != column_constraints.data.end(); ++column, ++column_size) {
      if (!TreatAsFixed(*column)) {
        continue;
      }
      last_column_size = column_size;
      if (scale_available) {
        *column_size =
            LayoutUnit(scale * column->max_inline_size.value_or(LayoutUnit()));
      } else {
        DCHECK_EQ(fixed_columns_count, all_columns_count);
        *column_size =
            LayoutUnit(target_inline_size.ToFloat() / fixed_columns_count);
      }
      assigned_inline_size += *column_size;
    }
  }
  if (assigned_inline_size >= target_inline_size) {
    return column_sizes;
  }
  // Distribute to percent columns.
  if (percent_columns_count > 0) {
    float scale = 1.0f;
    bool scale_available = true;
    // Percent columns only grow if there are no auto columns.
    bool scale_up = total_percent_inline_size <
                        (target_inline_size - assigned_inline_size) &&
                    auto_columns_count == 0;
    bool scale_down =
        total_percent_inline_size > (target_inline_size - assigned_inline_size);
    if (scale_up || scale_down) {
      if (total_percent_inline_size != LayoutUnit()) {
        scale = (target_inline_size - assigned_inline_size).ToFloat() /
                total_percent_inline_size;
      } else {
        scale_available = false;
      }
    }
    LayoutUnit* column_size = column_sizes.data();
    for (auto column = column_constraints.data.begin();
         column != column_constraints.data.end(); ++column, ++column_size) {
      if (!column->percent) {
        continue;
      }
      last_column_size = column_size;
      if (scale_available) {
        *column_size = LayoutUnit(
            scale * column->ResolvePercentInlineSize(target_inline_size));
      } else {
        *column_size =
            LayoutUnit((target_inline_size - assigned_inline_size).ToFloat() /
                       percent_columns_count);
      }
      assigned_inline_size += *column_size;
    }
  }
  // Distribute to auto, and zero inline size columns.
  LayoutUnit distributing_inline_size =
      target_inline_size - assigned_inline_size;
  LayoutUnit* column_size = column_sizes.data();

  bool distribute_zero_inline_size =
      zero_inline_size_constrained_colums_count == all_columns_count;

  for (auto column = column_constraints.data.begin();
       column != column_constraints.data.end(); ++column, ++column_size) {
    if (column->percent || TreatAsFixed(*column)) {
      continue;
    }
    // Zero-width columns only grow if all columns are zero-width.
    if (IsZeroInlineSizeConstrained(*column) && !distribute_zero_inline_size) {
      continue;
    }

    last_column_size = column_size;
    *column_size =
        LayoutUnit(distributing_inline_size /
                   float(distribute_zero_inline_size
                             ? zero_inline_size_constrained_colums_count
                             : auto_columns_count));
    assigned_inline_size += *column_size;
  }
  LayoutUnit delta = target_inline_size - assigned_inline_size;
  DCHECK(last_column_size);
  *last_column_size += delta;

  return column_sizes;
}

void DistributeColspanCellToColumnsFixed(
    const TableTypes::ColspanCell& colspan_cell,
    LayoutUnit inline_border_spacing,
    TableTypes::Columns* column_constraints) {
  // Fixed layout does not merge columns.
  DCHECK_LE(colspan_cell.span,
            column_constraints->data.size() - colspan_cell.start_column);
  TableTypes::Column* start_column =
      &column_constraints->data[colspan_cell.start_column];
  TableTypes::Column* end_column = start_column + colspan_cell.span;
  DCHECK_NE(start_column, end_column);

  // Inline sizes for redistribution exclude border spacing.
  LayoutUnit total_inner_border_spacing;
  unsigned effective_span = 0;
  bool is_first_column = true;
  for (TableTypes::Column* column = start_column; column != end_column;
       ++column) {
    if (column->is_mergeable) {
      continue;
    }
    ++effective_span;
    if (!is_first_column) {
      total_inner_border_spacing += inline_border_spacing;
    } else {
      is_first_column = false;
    }
  }
  LayoutUnit colspan_cell_min_inline_size;
  LayoutUnit colspan_cell_max_inline_size;
  // Colspanned cells only distribute min inline size if constrained.
  if (colspan_cell.cell_inline_constraint.is_constrained) {
    colspan_cell_min_inline_size =
        (colspan_cell.cell_inline_constraint.min_inline_size -
         total_inner_border_spacing)
            .ClampNegativeToZero();
  }
  colspan_cell_max_inline_size =
      (colspan_cell.cell_inline_constraint.max_inline_size -
       total_inner_border_spacing)
          .ClampNegativeToZero();

  // Distribute min/max evenly between all cells.
  LayoutUnit rounding_error_min_inline_size = colspan_cell_min_inline_size;
  LayoutUnit rounding_error_max_inline_size = colspan_cell_max_inline_size;

  LayoutUnit new_min_size = LayoutUnit(colspan_cell_min_inline_size /
                                       static_cast<float>(effective_span));
  LayoutUnit new_max_size = LayoutUnit(colspan_cell_max_inline_size /
                                       static_cast<float>(effective_span));
  std::optional<float> new_percent;
  if (colspan_cell.cell_inline_constraint.percent) {
    new_percent = *colspan_cell.cell_inline_constraint.percent / effective_span;
  }

  TableTypes::Column* last_column = nullptr;
  for (TableTypes::Column* column = start_column; column < end_column;
       ++column) {
    if (column->is_mergeable) {
      continue;
    }
    last_column = column;
    rounding_error_min_inline_size -= new_min_size;
    rounding_error_max_inline_size -= new_max_size;

    if (!column->min_inline_size) {
      column->is_constrained |=
          colspan_cell.cell_inline_constraint.is_constrained;
      column->min_inline_size = new_min_size;
    }
    if (!column->max_inline_size) {
      column->is_constrained |=
          colspan_cell.cell_inline_constraint.is_constrained;
      column->max_inline_size = new_max_size;
    }
    // Percentages only get distributed over auto columns.
    if (!column->percent && !column->is_constrained && new_percent) {
      column->percent = *new_percent;
    }
  }
  DCHECK(last_column);
  last_column->min_inline_size =
      *last_column->min_inline_size + rounding_error_min_inline_size;
  last_column->max_inline_size =
      *last_column->max_inline_size + rounding_error_max_inline_size;
}

void DistributeColspanCellToColumnsAuto(
    const TableTypes::ColspanCell& colspan_cell,
    LayoutUnit inline_border_spacing,
    TableTypes::Columns* column_constraints) {
  if (column_constraints->data.empty()) {
    return;
  }
  unsigned effective_span =
      std::min(colspan_cell.span,
               column_constraints->data.size() - colspan_cell.start_column);
  TableTypes::Column* start_column =
      &column_constraints->data[colspan_cell.start_column];
  TableTypes::Column* end_column = start_column + effective_span;

  // Inline sizes for redistribution exclude border spacing.
  LayoutUnit total_inner_border_spacing;
  bool is_first_column = true;
  for (TableTypes::Column* column = start_column; column != end_column;
       ++column) {
    if (!column->is_mergeable) {
      if (!is_first_column) {
        total_inner_border_spacing += inline_border_spacing;
      } else {
        is_first_column = false;
      }
    }
  }

  LayoutUnit colspan_cell_min_inline_size =
      (colspan_cell.cell_inline_constraint.min_inline_size -
       total_inner_border_spacing)
          .ClampNegativeToZero();
  LayoutUnit colspan_cell_max_inline_size =
      (colspan_cell.cell_inline_constraint.max_inline_size -
       total_inner_border_spacing)
          .ClampNegativeToZero();
  std::optional<float> colspan_cell_percent =
      colspan_cell.cell_inline_constraint.percent;

  if (colspan_cell_percent.has_value()) {
    float columns_percent = 0.0f;
    unsigned all_columns_count = 0;
    unsigned percent_columns_count = 0;
    unsigned nonpercent_columns_count = 0;
    LayoutUnit nonpercent_columns_max_inline_size;
    for (TableTypes::Column* column = start_column; column != end_column;
         ++column) {
      if (!column->max_inline_size) {
        column->max_inline_size = LayoutUnit();
      }
      if (!column->min_inline_size) {
        column->min_inline_size = LayoutUnit();
      }
      if (column->is_mergeable) {
        continue;
      }
      all_columns_count++;
      if (column->percent) {
        percent_columns_count++;
        columns_percent += *column->percent;
      } else {
        nonpercent_columns_count++;
        nonpercent_columns_max_inline_size += *column->max_inline_size;
      }
    }
    float surplus_percent = *colspan_cell_percent - columns_percent;
    if (surplus_percent > 0.0f && all_columns_count > percent_columns_count) {
      // Distribute surplus percent to non-percent columns in proportion to
      // max_inline_size.
      for (TableTypes::Column* column = start_column; column != end_column;
           ++column) {
        if (column->percent || column->is_mergeable) {
          continue;
        }
        float column_percent;
        if (nonpercent_columns_max_inline_size != LayoutUnit()) {
          // Column percentage is proportional to its max_inline_size.
          column_percent = surplus_percent *
                           column->max_inline_size.value_or(LayoutUnit()) /
                           nonpercent_columns_max_inline_size;
        } else {
          // Distribute evenly instead.
          // Legacy difference: Legacy forces max_inline_size to be at least
          // 1px.
          column_percent = surplus_percent / nonpercent_columns_count;
        }
        column->percent = column_percent;
      }
    }
  }

  // TODO(atotic) See crbug.com/531752 for discussion about differences
  // between FF/Chrome.
  // Minimum inline size gets distributed with standard distribution algorithm.
  for (TableTypes::Column* column = start_column; column != end_column;
       ++column) {
    if (!column->min_inline_size) {
      column->min_inline_size = LayoutUnit();
    }
    if (!column->max_inline_size) {
      column->max_inline_size = LayoutUnit();
    }
  }
  Vector<LayoutUnit> computed_sizes =
      DistributeInlineSizeToComputedInlineSizeAuto(
          colspan_cell_min_inline_size, start_column, end_column, true);
  LayoutUnit* computed_size = computed_sizes.data();
  for (TableTypes::Column* column = start_column; column != end_column;
       ++column, ++computed_size) {
    column->min_inline_size =
        std::max(*column->min_inline_size, *computed_size);
  }
  computed_sizes = DistributeInlineSizeToComputedInlineSizeAuto(
      colspan_cell_max_inline_size, start_column,
      end_column, /* treat_target_size_as_constrained */
      colspan_cell.cell_inline_constraint.is_constrained);
  computed_size = computed_sizes.data();
  for (TableTypes::Column* column = start_column; column != end_column;
       ++column, ++computed_size) {
    column->max_inline_size =
        std::max(std::max(*column->min_inline_size, *column->max_inline_size),
                 *computed_size);
  }
}

// Handles distribution of excess block size from: table, sections,
// rows, and rowspanned cells, to rows.
// Rowspanned cells distribute with slight differences from
// general distribution algorithm.
void DistributeExcessBlockSizeToRows(
    const wtf_size_t start_row_index,
    const wtf_size_t row_count,
    LayoutUnit desired_block_size,
    bool is_rowspan_distribution,
    LayoutUnit border_block_spacing,
    LayoutUnit percentage_resolution_block_size,
    TableTypes::Rows* rows) {
  DCHECK_GE(desired_block_size, LayoutUnit());
  // This algorithm has not been defined by the standard in 2019.
  // Discussion at https://github.com/w3c/csswg-drafts/issues/4418
  if (row_count == 0) {
    return;
  }

  const wtf_size_t end_row_index = start_row_index + row_count;
  DCHECK_LE(end_row_index, rows->size());

  auto RowBlockSizeDeficit = [&percentage_resolution_block_size](
                                 const TableTypes::Row& row) {
    DCHECK_NE(percentage_resolution_block_size, kIndefiniteSize);
    DCHECK(row.percent);
    return (LayoutUnit(*row.percent * percentage_resolution_block_size / 100) -
            row.block_size)
        .ClampNegativeToZero();
  };

  Vector<wtf_size_t> rows_with_originating_rowspan;
  Vector<wtf_size_t> percent_rows_with_deficit;
  Vector<wtf_size_t> unconstrained_non_empty_rows;
  Vector<wtf_size_t> empty_rows;
  Vector<wtf_size_t> non_empty_rows;
  Vector<wtf_size_t> unconstrained_empty_rows;
  unsigned constrained_non_empty_row_count = 0;

  LayoutUnit total_block_size;
  LayoutUnit percent_block_size_deficit;
  LayoutUnit unconstrained_non_empty_row_block_size;

  for (auto index = start_row_index; index < end_row_index; ++index) {
    const auto& row = rows->at(index);
    total_block_size += row.block_size;

    // Rowspans are treated specially only during rowspan distribution.
    bool is_row_with_originating_rowspan = is_rowspan_distribution &&
                                           index != start_row_index &&
                                           row.has_rowspan_start;
    if (is_row_with_originating_rowspan) {
      rows_with_originating_rowspan.push_back(index);
    }

    bool is_row_empty = row.block_size == LayoutUnit();

    if (row.percent && *row.percent != 0 &&
        percentage_resolution_block_size != kIndefiniteSize) {
      LayoutUnit deficit = RowBlockSizeDeficit(row);
      if (deficit != LayoutUnit()) {
        percent_rows_with_deficit.push_back(index);
        percent_block_size_deficit += deficit;
        is_row_empty = false;
      }
    }

    // Only consider percent rows that resolve as constrained.
    const bool is_row_constrained =
        row.is_constrained &&
        (!row.percent || percentage_resolution_block_size != kIndefiniteSize);

    if (is_row_empty) {
      empty_rows.push_back(index);
      if (!is_row_constrained) {
        unconstrained_empty_rows.push_back(index);
      }
    } else {
      non_empty_rows.push_back(index);
      if (is_row_constrained) {
        constrained_non_empty_row_count++;
      } else {
        unconstrained_non_empty_rows.push_back(index);
        unconstrained_non_empty_row_block_size += row.block_size;
      }
    }
  }

  LayoutUnit distributable_block_size =
      (desired_block_size - border_block_spacing * (row_count - 1)) -
      total_block_size;
  if (distributable_block_size <= LayoutUnit()) {
    return;
  }

  // Step 1: percentage rows grow to no more than their percentage size.
  if (!percent_rows_with_deficit.empty()) {
    // Don't distribute more than the percent block-size deficit.
    LayoutUnit percent_distributable_block_size =
        std::min(percent_block_size_deficit, distributable_block_size);

    LayoutUnit remaining_deficit = percent_distributable_block_size;
    for (auto& index : percent_rows_with_deficit) {
      auto& row = rows->at(index);
      LayoutUnit delta = percent_distributable_block_size.MulDiv(
          RowBlockSizeDeficit(row), percent_block_size_deficit);
      row.block_size += delta;
      total_block_size += delta;
      distributable_block_size -= delta;
      remaining_deficit -= delta;
    }
    auto& last_row = rows->at(percent_rows_with_deficit.back());
    last_row.block_size += remaining_deficit;
    distributable_block_size -= remaining_deficit;
    DCHECK_GE(last_row.block_size, LayoutUnit());

    // Rounding may cause us to distribute more than the distributable size.
    if (distributable_block_size <= LayoutUnit()) {
      return;
    }
  }

  // Step 2: Distribute to rows that have an originating rowspan.
  if (!rows_with_originating_rowspan.empty()) {
    LayoutUnit remaining_deficit = distributable_block_size;
    for (auto& index : rows_with_originating_rowspan) {
      auto& row = rows->at(index);
      LayoutUnit delta =
          distributable_block_size / rows_with_originating_rowspan.size();
      row.block_size += delta;
      remaining_deficit -= delta;
    }
    auto& last_row = rows->at(rows_with_originating_rowspan.back());
    last_row.block_size += remaining_deficit;
    last_row.block_size = std::max(last_row.block_size, LayoutUnit());
    return;
  }

  // Step 3: "unconstrained non-empty rows" grow in proportion to current
  // block size.
  if (!unconstrained_non_empty_rows.empty()) {
    LayoutUnit remaining_deficit = distributable_block_size;
    for (auto& index : unconstrained_non_empty_rows) {
      auto& row = rows->at(index);
      LayoutUnit delta = distributable_block_size.MulDiv(
          row.block_size, unconstrained_non_empty_row_block_size);
      row.block_size += delta;
      remaining_deficit -= delta;
    }
    auto& last_row = rows->at(unconstrained_non_empty_rows.back());
    last_row.block_size += remaining_deficit;
    DCHECK_GE(last_row.block_size, LayoutUnit());
    return;
  }

  // Step 4: Empty row distribution
  // At this point all rows are empty and/or constrained.
  if (!empty_rows.empty()) {
    const bool has_only_empty_rows = empty_rows.size() == row_count;
    if (is_rowspan_distribution) {
      // If we are doing a rowspan distribution, *and* only have empty rows,
      // distribute everything to the last empty row.
      if (has_only_empty_rows) {
        rows->at(empty_rows.back()).block_size += distributable_block_size;
        return;
      }
    } else if (has_only_empty_rows ||
               (empty_rows.size() + constrained_non_empty_row_count ==
                row_count)) {
      // Grow empty rows if either of these is true:
      // - All rows are empty.
      // - Non-empty rows are all constrained.
      LayoutUnit remaining_deficit = distributable_block_size;
      // If there are constrained and unconstrained empty rows, only
      // the unconstrained rows grow.
      Vector<wtf_size_t>& rows_to_grow = !unconstrained_empty_rows.empty()
                                             ? unconstrained_empty_rows
                                             : empty_rows;
      for (auto& index : rows_to_grow) {
        auto& row = rows->at(index);
        LayoutUnit delta = distributable_block_size / rows_to_grow.size();
        row.block_size = delta;
        remaining_deficit -= delta;
      }
      auto& last_row = rows->at(rows_to_grow.back());
      last_row.block_size += remaining_deficit;
      DCHECK_GE(last_row.block_size, LayoutUnit());
      return;
    }
  }

  // Step 5: Grow non-empty rows in proportion to current block size.
  // It grows constrained, and unconstrained rows.
  if (!non_empty_rows.empty()) {
    LayoutUnit remaining_deficit = distributable_block_size;
    for (auto& index : non_empty_rows) {
      auto& row = rows->at(index);
      LayoutUnit delta =
          distributable_block_size.MulDiv(row.block_size, total_block_size);
      row.block_size += delta;
      remaining_deficit -= delta;
    }
    auto& last_row = rows->at(non_empty_rows.back());
    last_row.block_size += remaining_deficit;
    DCHECK_GE(last_row.block_size, LayoutUnit());
  }
}

}  // namespace

CellBlockSizeData ComputeCellBlockSize(
    const TableTypes::CellBlockConstraint& cell_block_constraint,
    const TableTypes::Rows& rows,
    wtf_size_t row_index,
    const LogicalSize& border_spacing,
    bool is_table_block_size_specified) {
  // NOTE: Confusingly rowspanned cells originating from a collapsed-row also
  // have no block-size.
  LayoutUnit cell_block_size;
  if (!rows[row_index].is_collapsed) {
    for (wtf_size_t i = 0; i < cell_block_constraint.effective_rowspan; ++i) {
      if (rows[row_index + i].is_collapsed)
        continue;
      cell_block_size += rows[row_index + i].block_size;
      if (i != 0)
        cell_block_size += border_spacing.block_size;
    }
  }

  bool has_grown = cell_block_size > cell_block_constraint.min_block_size;

  // Our initial block-size is definite if this cell has a fixed block-size,
  // or we have grown and the table has a specified block-size.
  bool is_initial_block_size_definite =
      cell_block_constraint.is_constrained ||
      (has_grown && is_table_block_size_specified);

  return {cell_block_size, !is_initial_block_size_definite};
}

void SetupTableCellConstraintSpaceBuilder(
    const WritingDirectionMode table_writing_direction,
    const BlockNode cell,
    const BoxStrut& cell_borders,
    const Vector<TableColumnLocation>& column_locations,
    LayoutUnit cell_block_size,
    LayoutUnit percentage_inline_size,
    std::optional<LayoutUnit> alignment_baseline,
    wtf_size_t start_column,
    bool is_initial_block_size_indefinite,
    bool is_table_block_size_specified,
    bool has_collapsed_borders,
    LayoutResultCacheSlot cache_slot,
    ConstraintSpaceBuilder* builder) {
  const auto& cell_style = cell.Style();
  const auto table_writing_mode = table_writing_direction.GetWritingMode();
  const wtf_size_t end_column = std::min(
      start_column + cell.TableCellColspan() - 1, column_locations.size() - 1);
  const LayoutUnit cell_inline_size = column_locations[end_column].offset +
                                      column_locations[end_column].size -
                                      column_locations[start_column].offset;

  // A table-cell is hidden if all the columns it spans are collapsed.
  const bool is_hidden_for_paint = [&]() -> bool {
    for (wtf_size_t column = start_column; column <= end_column; ++column) {
      if (!column_locations[column].is_collapsed)
        return false;
    }
    return true;
  }();

  builder->SetIsTableCell(true);

  if (!IsParallelWritingMode(table_writing_mode, cell_style.GetWritingMode())) {
    const PhysicalSize icb_size = cell.InitialContainingBlockSize();
    builder->SetOrthogonalFallbackInlineSize(
        table_writing_direction.IsHorizontal() ? icb_size.height
                                               : icb_size.width);
  }

  builder->SetAvailableSize({cell_inline_size, cell_block_size});
  builder->SetIsFixedInlineSize(true);
  if (cell_block_size != kIndefiniteSize)
    builder->SetIsFixedBlockSize(true);
  builder->SetIsInitialBlockSizeIndefinite(is_initial_block_size_indefinite);

  // https://www.w3.org/TR/css-tables-3/#computing-the-table-height
  // "the computed height (if definite, percentages being considered 0px)"
  builder->SetPercentageResolutionSize(
      {percentage_inline_size, kIndefiniteSize});

  builder->SetTableCellBorders(cell_borders, cell_style.GetWritingDirection(),
                               table_writing_direction);
  builder->SetTableCellAlignmentBaseline(alignment_baseline);
  builder->SetTableCellColumnIndex(start_column);
  builder->SetIsRestrictedBlockSizeTableCell(
      is_table_block_size_specified || cell_style.LogicalHeight().IsFixed());
  builder->SetIsHiddenForPaint(is_hidden_for_paint);
  builder->SetIsTableCellWithCollapsedBorders(has_collapsed_borders);
  builder->SetHideTableCellIfEmpty(
      !has_collapsed_borders && cell_style.EmptyCells() == EEmptyCells::kHide);
  builder->SetCacheSlot(cache_slot);
}

// Computes maximum possible number of non-mergeable columns.
wtf_size_t ComputeMaximumNonMergeableColumnCount(
    const HeapVector<BlockNode>& columns,
    bool is_fixed_layout) {
  // Build column constraints.
  scoped_refptr<TableTypes::Columns> column_constraints =
      base::MakeRefCounted<TableTypes::Columns>();
  ColumnConstraintsBuilder constraints_builder(column_constraints.get(),
                                               is_fixed_layout);
  VisitLayoutTableColumn(columns, UINT_MAX, &constraints_builder);
  // Find last non-mergeable column.
  if (column_constraints->data.size() == 0)
    return 0;
  wtf_size_t column_index = column_constraints->data.size() - 1;
  while (column_index > 0 &&
         column_constraints->data[column_index].is_mergeable) {
    --column_index;
  }
  if (column_index == 0 && column_constraints->data[0].is_mergeable)
    return 0;
  return column_index + 1;
}

scoped_refptr<TableTypes::Columns> ComputeColumnConstraints(
    const BlockNode& table,
    const TableGroupedChildren& grouped_children,
    const TableBorders& table_borders,
    const BoxStrut& border_padding) {
  const auto& table_style = table.Style();
  bool is_fixed_layout = table_style.IsFixedTableLayout();

  TableTypes::CellInlineConstraints cell_inline_constraints;
  TableTypes::ColspanCells colspan_cell_constraints;

  scoped_refptr<TableTypes::Columns> column_constraints =
      base::MakeRefCounted<TableTypes::Columns>();
  ComputeColumnElementConstraints(grouped_children.columns, is_fixed_layout,
                                  column_constraints.get());

  // Collect section constraints
  bool is_first_section = true;
  wtf_size_t row_index = 0;
  wtf_size_t section_index = 0;
  for (BlockNode section : grouped_children) {
    if (!section.IsEmptyTableSection()) {
      ComputeSectionInlineConstraints(
          section, is_fixed_layout, is_first_section,
          table_style.GetWritingDirection(), table_borders, section_index,
          &row_index, &cell_inline_constraints, &colspan_cell_constraints);
      is_first_section = false;
    }
    section_index++;
  }
  ApplyCellConstraintsToColumnConstraints(
      cell_inline_constraints, table_style.TableBorderSpacing().inline_size,
      is_fixed_layout, &colspan_cell_constraints, column_constraints.get());

  return column_constraints;
}

void ComputeSectionMinimumRowBlockSizes(
    const BlockNode& section,
    const LayoutUnit cell_percentage_inline_size,
    const bool is_table_block_size_specified,
    const Vector<TableColumnLocation>& column_locations,
    const TableBorders& table_borders,
    const LayoutUnit block_border_spacing,
    wtf_size_t section_index,
    bool treat_section_as_tbody,
    TableTypes::Sections* sections,
    TableTypes::Rows* rows,
    TableTypes::CellBlockConstraints* cell_block_constraints) {
  // In rare circumstances we need to know the total row count before we've
  // visited all them (for computing effective rowspans). We don't want to
  // perform this unnecessarily.
  std::optional<wtf_size_t> row_count;
  auto RowCountFunc = [&]() -> wtf_size_t {
    if (!row_count) {
      row_count = 0;
      for (BlockNode row = To<BlockNode>(section.FirstChild()); row;
           row = To<BlockNode>(row.NextSibling())) {
        (*row_count)++;
      }
    }

    return *row_count;
  };

  wtf_size_t start_row = rows->size();
  wtf_size_t current_row = start_row;
  TableTypes::RowspanCells rowspan_cells;
  LayoutUnit section_block_size;
  // Used to compute column index.
  ColspanCellTabulator colspan_cell_tabulator;
  // total_row_percent must be under 100%
  float total_row_percent = 0;
  // Get minimum block size of each row.
  for (BlockNode row = To<BlockNode>(section.FirstChild()); row;
       row = To<BlockNode>(row.NextSibling())) {
    colspan_cell_tabulator.StartRow();
    TableTypes::Row row_constraint = ComputeMinimumRowBlockSize(
        RowCountFunc, row, cell_percentage_inline_size,
        is_table_block_size_specified, column_locations, table_borders,
        start_row, current_row++, section_index,
        /* is_section_collapsed */ section.Style().UsedVisibility() ==
            EVisibility::kCollapse,
        cell_block_constraints, &rowspan_cells, &colspan_cell_tabulator);
    if (row_constraint.percent.has_value()) {
      row_constraint.percent =
          std::min(100.0f - total_row_percent, *row_constraint.percent);
      total_row_percent += *row_constraint.percent;
    }
    rows->push_back(row_constraint);
    section_block_size += row_constraint.block_size;
    colspan_cell_tabulator.EndRow();
  }

  // Redistribute rowspanned cell block sizes.
  std::stable_sort(rowspan_cells.begin(), rowspan_cells.end());
  for (const auto& rowspan_cell : rowspan_cells) {
    DistributeRowspanCellToRows(rowspan_cell, block_border_spacing, rows);
  }

  const wtf_size_t block_spacing_count =
      current_row == start_row ? 0 : current_row - start_row - 1;
  const LayoutUnit border_spacing_total =
      block_border_spacing * block_spacing_count;
  section_block_size += border_spacing_total;

  // Redistribute section's css block size.
  const Length& section_specified_block_length =
      section.Style().LogicalHeight();
  // TODO(1105272) Handle section_specified_block_length.IsCalculated()
  if (section_specified_block_length.IsFixed()) {
    LayoutUnit section_fixed_block_size =
        LayoutUnit(section_specified_block_length.Value());
    if (section_fixed_block_size > section_block_size) {
      DistributeSectionFixedBlockSizeToRows(
          start_row, current_row - start_row, section_fixed_block_size,
          block_border_spacing, section_fixed_block_size, rows);
      section_block_size = section_fixed_block_size;
    }
  }
  sections->push_back(
      TableTypes::CreateSection(section, start_row, current_row - start_row,
                                section_block_size, treat_section_as_tbody));
}

void FinalizeTableCellLayout(LayoutUnit unconstrained_intrinsic_block_size,
                             BoxFragmentBuilder* builder) {
  const BlockNode& node = builder->Node();
  const auto& space = builder->GetConstraintSpace();
  const bool has_inflow_children = !builder->Children().empty();

  // Hide table-cells if:
  //  - They are within a collapsed column(s). These are already marked as
  //    hidden for paint in the constraint space, and don't need to be marked
  //    again in the fragment builder.
  //  - They have "empty-cells: hide", non-collapsed borders, and no children.
  if (!space.IsHiddenForPaint()) {
    builder->SetIsHiddenForPaint(space.HideTableCellIfEmpty() &&
                                 !has_inflow_children);
  }
  builder->SetHasCollapsedBorders(space.IsTableCellWithCollapsedBorders());
  builder->SetIsTablePart();
  builder->SetTableCellColumnIndex(space.TableCellColumnIndex());

  // If we're resuming after a break, there'll be no alignment, since the
  // fragment will start at the block-start edge of the fragmentainer then.
  if (IsBreakInside(builder->PreviousBreakToken()))
    return;

  LayoutUnit free_space =
      builder->FragmentBlockSize() - unconstrained_intrinsic_block_size;
  BlockContentAlignment alignment = ComputeContentAlignmentForTableCell(
      builder->Style(), &builder->Node().GetDocument());
  if (alignment == BlockContentAlignment::kSafeCenter ||
      alignment == BlockContentAlignment::kSafeEnd) {
    free_space = free_space.ClampNegativeToZero();
  }
  switch (alignment) {
    case BlockContentAlignment::kStart:
      // Nothing to do.
      break;
    case BlockContentAlignment::kBaseline:
      // Table-cells (with baseline vertical alignment) always produce a
      // first/last baseline of their end-content edge (even if the content
      // doesn't have any baselines).
      if (!builder->FirstBaseline() || node.ShouldApplyLayoutContainment()) {
        builder->SetBaselines(unconstrained_intrinsic_block_size -
                              builder->BorderScrollbarPadding().block_end);
      }

      // Only adjust if we have *inflow* children. If we only have
      // OOF-positioned children don't align them to the alignment baseline.
      if (has_inflow_children) {
        if (auto alignment_baseline = space.TableCellAlignmentBaseline()) {
          builder->MoveChildrenInBlockDirection(*alignment_baseline -
                                                *builder->FirstBaseline());
        }
      }
      break;
    case BlockContentAlignment::kSafeCenter:
    case BlockContentAlignment::kUnsafeCenter:
      builder->MoveChildrenInBlockDirection(free_space / 2);
      break;
    case BlockContentAlignment::kSafeEnd:
    case BlockContentAlignment::kUnsafeEnd:
      builder->MoveChildrenInBlockDirection(free_space);
      break;
  }
}

void ColspanCellTabulator::StartRow() {
  current_column_ = 0;
}

// Remove colspanned cells that are not spanning any more rows.
void ColspanCellTabulator::EndRow() {
  for (wtf_size_t i = 0; i < colspanned_cells_.size();) {
    colspanned_cells_[i].remaining_rows--;
    if (colspanned_cells_[i].remaining_rows == 0)
      colspanned_cells_.EraseAt(i);
    else
      ++i;
  }
  std::sort(colspanned_cells_.begin(), colspanned_cells_.end(),
            [](const ColspanCellTabulator::Cell& a,
               const ColspanCellTabulator::Cell& b) {
              return a.column_start < b.column_start;
            });
}

// Advance current column to position not occupied by colspanned cells.
void ColspanCellTabulator::FindNextFreeColumn() {
  for (const Cell& colspanned_cell : colspanned_cells_) {
    if (colspanned_cell.column_start <= current_column_ &&
        colspanned_cell.column_start + colspanned_cell.span > current_column_) {
      current_column_ = colspanned_cell.column_start + colspanned_cell.span;
    }
  }
}

void ColspanCellTabulator::ProcessCell(const BlockNode& cell) {
  wtf_size_t colspan = cell.TableCellColspan();
  wtf_size_t rowspan = cell.TableCellRowspan();
  if (rowspan > 1)
    colspanned_cells_.emplace_back(current_column_, colspan, rowspan);
  current_column_ += colspan;
}

void RowBaselineTabulator::ProcessCell(
    const LogicalBoxFragment& fragment,
    BlockContentAlignment align,
    const bool is_rowspanned,
    const bool descendant_depends_on_percentage_block_size) {
  if (align == BlockContentAlignment::kBaseline &&
      fragment.HasDescendantsForTablePart() && fragment.FirstBaseline()) {
    max_cell_baseline_depends_on_percentage_block_descendant_ |=
        descendant_depends_on_percentage_block_size;
    const LayoutUnit cell_baseline = *fragment.FirstBaseline();
    max_cell_ascent_ =
        std::max(max_cell_ascent_.value_or(LayoutUnit::Min()), cell_baseline);
    if (is_rowspanned) {
      if (!max_cell_descent_)
        max_cell_descent_ = LayoutUnit();
    } else {
      max_cell_descent_ =
          std::max(max_cell_descent_.value_or(LayoutUnit::Min()),
                   fragment.BlockSize() - cell_baseline);
    }
  }

  // https://www.w3.org/TR/css-tables-3/#row-layout "If there is no such
  // line box or table-row, the baseline is the bottom of content edge of
  // the cell box."
  if (!max_cell_ascent_) {
    fallback_cell_depends_on_percentage_block_descendant_ |=
        descendant_depends_on_percentage_block_size;
    const LayoutUnit cell_block_end_border_padding =
        fragment.Padding().block_end + fragment.Borders().block_end;
    fallback_cell_descent_ =
        std::min(fallback_cell_descent_.value_or(LayoutUnit::Max()),
                 cell_block_end_border_padding);
  }
}

LayoutUnit RowBaselineTabulator::ComputeRowBlockSize(
    const LayoutUnit max_cell_block_size) {
  if (max_cell_ascent_) {
    return std::max(max_cell_block_size,
                    *max_cell_ascent_ + *max_cell_descent_);
  }
  return max_cell_block_size;
}

LayoutUnit RowBaselineTabulator::ComputeBaseline(
    const LayoutUnit row_block_size) {
  if (max_cell_ascent_)
    return *max_cell_ascent_;
  if (fallback_cell_descent_)
    return (row_block_size - *fallback_cell_descent_).ClampNegativeToZero();
  // Empty row's baseline is top.
  return LayoutUnit();
}

bool RowBaselineTabulator::BaselineDependsOnPercentageBlockDescendant() {
  if (max_cell_ascent_)
    return max_cell_baseline_depends_on_percentage_block_descendant_;
  if (fallback_cell_descent_)
    return fallback_cell_depends_on_percentage_block_descendant_;
  return false;
}

MinMaxSizes ComputeGridInlineMinMax(
    const TableNode& node,
    const TableTypes::Columns& column_constraints,
    LayoutUnit undistributable_space,
    bool is_fixed_layout,
    bool is_layout_pass) {
  MinMaxSizes min_max;
  // https://www.w3.org/TR/css-tables-3/#computing-the-table-width
  // Compute standard GRID_MIN/GRID_MAX. They are sum of column_constraints.
  //
  // Standard does not specify how to handle percentages.
  // "a percentage represents a constraint on the column's inline size, which a
  // UA should try to satisfy"
  // Percentages cannot be resolved into pixels because size of containing
  // block is unknown. Instead, percentages are used to enforce following
  // constraints:
  // 1) Column min inline size and percentage imply that total inline sum must
  // be large enough to fit the column. Mathematically, column with
  // min_inline_size of X, and percentage Y% implies that the
  // total inline sum MINSUM must satisfy: MINSUM * Y% >= X.
  // 2) Let T% be sum of all percentages. Let M be sum of min_inline_sizes of
  // all non-percentage columns. Total min size sum MINSUM must satisfy:
  // T% * MINSUM + M = MINSUM.

  // Minimum total size estimate based on column's min_inline_size and percent.
  LayoutUnit percent_max_size_estimate;
  // Sum of max_inline_sizes of non-percentage columns.
  LayoutUnit non_percent_max_size_sum;
  float percent_sum = 0;
  for (const TableTypes::Column& column : column_constraints.data) {
    if (column.min_inline_size) {
      // In fixed layout, constrained cells minimum inline size is their
      // maximum.
      if (is_fixed_layout && column.IsFixed()) {
        min_max.min_size += *column.max_inline_size;
      } else {
        min_max.min_size += *column.min_inline_size;
      }
      if (column.percent && *column.percent > 0) {
        if (*column.max_inline_size > LayoutUnit()) {
          LayoutUnit estimate = LayoutUnit(
              100 / *column.percent *
              (*column.max_inline_size - column.percent_border_padding));
          percent_max_size_estimate =
              std::max(percent_max_size_estimate, estimate);
        }
      } else {
        non_percent_max_size_sum += *column.max_inline_size;
      }
    }
    if (column.max_inline_size) {
      min_max.max_size += *column.max_inline_size;
    }
    if (column.percent) {
      percent_sum += *column.percent;
    }
  }
  // Floating point math can cause total sum to be slightly above 100%.
  DCHECK_LE(percent_sum, 100.5f);
  percent_sum = std::min(percent_sum, 100.0f);

  // Table max inline size constraint can be computed from the total column
  // percentage combined with max_inline_size of non-percent columns.
  if (percent_sum > 0 && node.AllowColumnPercentages(is_layout_pass)) {
    LayoutUnit size_from_percent_and_fixed;
    DCHECK_GE(percent_sum, 0.0f);
    if (non_percent_max_size_sum != LayoutUnit()) {
      if (percent_sum == 100.0f) {
        size_from_percent_and_fixed = TableTypes::kTableMaxInlineSize;
      } else {
        size_from_percent_and_fixed =
            LayoutUnit((100 / (100 - percent_sum)) * non_percent_max_size_sum);
      }
    }
    min_max.max_size = std::max(min_max.max_size, size_from_percent_and_fixed);
    min_max.max_size = std::max(min_max.max_size, percent_max_size_estimate);
  }

  min_max.max_size = std::max(min_max.min_size, min_max.max_size);
  min_max += undistributable_space;
  return min_max;
}

void DistributeColspanCellsToColumns(
    const TableTypes::ColspanCells& colspan_cells,
    LayoutUnit inline_border_spacing,
    bool is_fixed_layout,
    TableTypes::Columns* column_constraints) {
  for (const TableTypes::ColspanCell& colspan_cell : colspan_cells) {
    // Clipped colspanned cells can end up having a span of 1 (which is not
    // wide).
    DCHECK_GT(colspan_cell.span, 1u);

    if (is_fixed_layout) {
      DistributeColspanCellToColumnsFixed(colspan_cell, inline_border_spacing,
                                          column_constraints);
    } else {
      DistributeColspanCellToColumnsAuto(colspan_cell, inline_border_spacing,
                                         column_constraints);
    }
  }
}

// Standard: https://www.w3.org/TR/css-tables-3/#width-distribution-algorithm
// After synchroniziation, assignable table inline size and sum of column
// final inline sizes will be equal.
Vector<LayoutUnit> SynchronizeAssignableTableInlineSizeAndColumns(
    LayoutUnit assignable_table_inline_size,
    bool is_fixed_layout,
    const TableTypes::Columns& column_constraints) {
  if (column_constraints.data.empty()) {
    return Vector<LayoutUnit>();
  }
  if (is_fixed_layout) {
    return SynchronizeAssignableTableInlineSizeAndColumnsFixed(
        assignable_table_inline_size, column_constraints);
  } else {
    const TableTypes::Column* start_column = &column_constraints.data[0];
    const TableTypes::Column* end_column =
        start_column + column_constraints.data.size();
    return DistributeInlineSizeToComputedInlineSizeAuto(
        assignable_table_inline_size, start_column, end_column,
        /* treat_target_size_as_constrained */ true);
  }
}

void DistributeRowspanCellToRows(const TableTypes::RowspanCell& rowspan_cell,
                                 LayoutUnit border_block_spacing,
                                 TableTypes::Rows* rows) {
  DCHECK_GT(rowspan_cell.effective_rowspan, 1u);
  DistributeExcessBlockSizeToRows(rowspan_cell.start_row,
                                  rowspan_cell.effective_rowspan,
                                  rowspan_cell.min_block_size,
                                  /* is_rowspan_distribution */ true,
                                  border_block_spacing, kIndefiniteSize, rows);
}

// Legacy code ignores section block size.
void DistributeSectionFixedBlockSizeToRows(
    const wtf_size_t start_row,
    const wtf_size_t rowspan,
    LayoutUnit section_fixed_block_size,
    LayoutUnit border_block_spacing,
    LayoutUnit percentage_resolution_block_size,
    TableTypes::Rows* rows) {
  DistributeExcessBlockSizeToRows(start_row, rowspan, section_fixed_block_size,
                                  /* is_rowspan_distribution */ false,
                                  border_block_spacing,
                                  percentage_resolution_block_size, rows);
}

void DistributeTableBlockSizeToSections(LayoutUnit border_block_spacing,
                                        LayoutUnit table_block_size,
                                        TableTypes::Sections* sections,
                                        TableTypes::Rows* rows) {
  if (sections->empty()) {
    return;
  }

  // Determine the table's block-size which we can distribute into.
  const LayoutUnit undistributable_space =
      (sections->size() + 1) * border_block_spacing;
  const LayoutUnit distributable_table_block_size =
      (table_block_size - undistributable_space).ClampNegativeToZero();

  auto ComputePercentageSize = [&distributable_table_block_size](
                                   auto& section) {
    DCHECK(section.percent.has_value());
    return std::max(
        section.block_size,
        LayoutUnit(*section.percent * distributable_table_block_size / 100));
  };

  LayoutUnit minimum_size_guess;
  LayoutUnit percent_size_guess;
  bool has_tbody = false;

  Vector<wtf_size_t> auto_sections;
  Vector<wtf_size_t> fixed_sections;
  Vector<wtf_size_t> percent_sections;
  Vector<wtf_size_t> tbody_auto_sections;
  Vector<wtf_size_t> tbody_fixed_sections;
  Vector<wtf_size_t> tbody_percent_sections;

  LayoutUnit auto_sections_size;
  LayoutUnit fixed_sections_size;
  LayoutUnit percent_sections_size;
  LayoutUnit tbody_auto_sections_size;
  LayoutUnit tbody_fixed_sections_size;
  LayoutUnit tbody_percent_sections_size;

  // Collect all our different section types.
  for (wtf_size_t index = 0u; index < sections->size(); ++index) {
    const auto& section = sections->at(index);
    minimum_size_guess += section.block_size;
    percent_size_guess +=
        section.percent ? ComputePercentageSize(section) : section.block_size;
    has_tbody |= section.is_tbody;

    if (section.percent) {
      percent_sections.push_back(index);
      if (section.is_tbody) {
        tbody_percent_sections.push_back(index);
      }
    } else if (section.is_constrained) {
      fixed_sections.push_back(index);
      fixed_sections_size += section.block_size;
      if (section.is_tbody) {
        tbody_fixed_sections.push_back(index);
        tbody_fixed_sections_size += section.block_size;
      }
    } else {
      auto_sections.push_back(index);
      auto_sections_size += section.block_size;
      if (section.is_tbody) {
        tbody_auto_sections.push_back(index);
        tbody_auto_sections_size += section.block_size;
      }
    }
  }

  // If the sections minimum size is greater than the distributable size -
  // there isn't any free space to distribute into.
  if (distributable_table_block_size <= minimum_size_guess) {
    return;
  }

  // Grow the (all) the percent sections up to what the percent specifies, and
  // in proportion to the *difference* between their percent size, and their
  // minimum size. E.g.
  //
  // <table style="height: 100px;">
  //   <tbody style="height: 50%;"></tbody>
  // </table>
  // The above <tbody> will grow to 50px.
  //
  // <table style="height: 100px;">
  //   <thead style="height: 50%;"></thead>
  //   <tbody style="height: 50%;"><td style="height: 60px;"></td></tbody>
  //   <tfoot style="height: 50%;"></tfoot>
  // </table>
  // The sections will be [20px, 60px, 20px]. The <tbody> doesn't grow as its
  // hit its minimum, remaining space distributed according to their percent.
  if (!percent_sections.empty() && percent_size_guess > minimum_size_guess) {
    const LayoutUnit distributable_size =
        std::min(percent_size_guess, distributable_table_block_size) -
        minimum_size_guess;
    DCHECK_GE(distributable_size, LayoutUnit());
    const LayoutUnit percent_minimum_difference =
        percent_size_guess - minimum_size_guess;

    LayoutUnit remaining_deficit = distributable_size;
    for (auto& index : percent_sections) {
      auto& section = sections->at(index);
      LayoutUnit delta = distributable_size.MulDiv(
          ComputePercentageSize(section) - section.block_size,
          percent_minimum_difference);
      section.block_size += delta;
      section.needs_redistribution = true;
      remaining_deficit -= delta;
      minimum_size_guess += delta;
      percent_sections_size += section.block_size;
      if (section.is_tbody) {
        tbody_percent_sections_size += section.block_size;
      }
    }
    auto& last_section = sections->at(percent_sections.back());
    last_section.block_size += remaining_deficit;
    DCHECK_GE(last_section.block_size, LayoutUnit());
    percent_sections_size += remaining_deficit;
    minimum_size_guess += remaining_deficit;
    if (last_section.is_tbody) {
      tbody_percent_sections_size += remaining_deficit;
    }
  }

  // Decide which sections to grow, we prefer any <tbody>-like sections over
  // headers/footers. Then in order:
  //  - auto sections.
  //  - fixed sections.
  //  - percent sections.
  Vector<wtf_size_t>* sections_to_grow;
  LayoutUnit sections_size;
  if (has_tbody) {
    if (!tbody_auto_sections.empty()) {
      sections_to_grow = &tbody_auto_sections;
      sections_size = tbody_auto_sections_size;
    } else if (!tbody_fixed_sections.empty()) {
      sections_to_grow = &tbody_fixed_sections;
      sections_size = tbody_fixed_sections_size;
    } else {
      DCHECK(!tbody_percent_sections.empty());
      sections_to_grow = &tbody_percent_sections;
      sections_size = tbody_percent_sections_size;
    }
  } else {
    if (!auto_sections.empty()) {
      sections_to_grow = &auto_sections;
      sections_size = auto_sections_size;
    } else if (!fixed_sections.empty()) {
      sections_to_grow = &fixed_sections;
      sections_size = fixed_sections_size;
    } else {
      DCHECK(!percent_sections.empty());
      sections_to_grow = &percent_sections;
      sections_size = percent_sections_size;
    }
  }

  // Distribute remaining size, evenly across the sections.
  LayoutUnit distributable_size =
      distributable_table_block_size - minimum_size_guess;
  if (distributable_size > LayoutUnit()) {
    LayoutUnit remaining_deficit = distributable_size;
    for (auto& index : *sections_to_grow) {
      auto& section = sections->at(index);
      LayoutUnit delta;
      if (sections_size > LayoutUnit()) {
        delta = distributable_size.MulDiv(section.block_size, sections_size);
      } else {
        delta = distributable_size / sections_to_grow->size();
      }
      section.block_size += delta;
      section.needs_redistribution = true;
      remaining_deficit -= delta;
    }
    auto& last_section = sections->at(sections_to_grow->back());
    last_section.block_size += remaining_deficit;
    DCHECK_GE(last_section.block_size, LayoutUnit());
  }

  // Propagate new section sizes to rows.
  for (TableTypes::Section& section : *sections) {
    if (!section.needs_redistribution) {
      continue;
    }
    DistributeExcessBlockSizeToRows(
        section.start_row, section.row_count, section.block_size,
        /* is_rowspan_distribution */ false, border_block_spacing,
        section.block_size, rows);
  }
}

}  // namespace blink
