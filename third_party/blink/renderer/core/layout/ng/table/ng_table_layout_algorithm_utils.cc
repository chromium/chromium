// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_disable_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column_visitor.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_helpers.h"

namespace blink {

namespace {

// Mergeable columns cannot be distributed to.
// Make at least one spanned column is distributable.
void EnsureDistributableColumnExists(
    wtf_size_t start_column_index,
    wtf_size_t span,
    NGTableTypes::Columns* column_constraints) {
  DCHECK_LT(start_column_index, column_constraints->data.size());
  DCHECK_GT(span, 1u);

  wtf_size_t effective_span =
      std::min(span, column_constraints->data.size() - start_column_index);
  NGTableTypes::Column* start_column =
      &column_constraints->data[start_column_index];
  NGTableTypes::Column* end_column = start_column + effective_span;

  NGTableTypes::Column* first_mergeable_column = nullptr;
  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column) {
    if (!column->is_collapsed) {
      if (!column->is_mergeable) {
        // Found non-collapsed, non mergeable column, nothing to do.
        return;
      } else if (!first_mergeable_column) {
        // Found first non-collapsed, mergeable column.
        first_mergeable_column = column;
      }
    }
  }
  // The interesting problem being solved here is interaction between
  // collapsed and mergeable columns.
  // All columns that are created by colspanned cell are mergeable by
  // default. Without collapsing, the first column would always be
  // marked as !mergeable.
  // What to do if the first column collapses? If that was the only
  // non-mergeable column, the entire cell would merge into first column,
  // and collapse.
  // To prevent "whole cell hidden if 1st cell is collapsed",
  // we try to make first non-collapsed column mergeable.
  // If all columns collapse, first cell is marked as meargable.
  if (first_mergeable_column) {
    // Some columns were not collapsed, mark first as mergeable.
    first_mergeable_column->is_mergeable = false;
  } else {
    start_column->is_mergeable = false;
  }
}

// Applies cell/wide cell constraints to columns.
// Guarantees columns min/max widths have non-empty values.
void ApplyCellConstraintsToColumnConstraints(
    const NGTableTypes::CellInlineConstraints& cell_constraints,
    LayoutUnit inline_border_spacing,
    bool is_fixed_layout,
    NGTableTypes::ColspanCells* colspan_cell_constraints,
    NGTableTypes::Columns* column_constraints) {
  // Satisfy prerequisites for cell merging:

  if (column_constraints->data.size() < cell_constraints.size()) {
    // Column constraint must exist for each cell.
    NGTableTypes::Column default_column;
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
  for (const NGTableTypes::ColspanCell& colspan_cell :
       *colspan_cell_constraints) {
    EnsureDistributableColumnExists(colspan_cell.start_column,
                                    colspan_cell.span, column_constraints);
  }

  // Distribute cell constraints to column constraints.
  for (wtf_size_t i = 0; i < cell_constraints.size(); ++i) {
    column_constraints->data[i].Encompass(cell_constraints[i]);
  }

  // Wide cell constraints are sorted by span length/starting column.
  auto colspan_cell_less_than = [](const NGTableTypes::ColspanCell& lhs,
                                   const NGTableTypes::ColspanCell& rhs) {
    if (lhs.span == rhs.span)
      return lhs.start_column < rhs.start_column;
    return lhs.span < rhs.span;
  };
  std::stable_sort(colspan_cell_constraints->begin(),
                   colspan_cell_constraints->end(), colspan_cell_less_than);

  NGTableAlgorithmHelpers::DistributeColspanCellsToColumns(
      *colspan_cell_constraints, inline_border_spacing, is_fixed_layout,
      column_constraints);

  // Column total percentage inline-size is clamped to 100%.
  // Auto tables: max(0, 100% minus the sum of percentages of all
  //   prior columns in the table)
  // Fixed tables: scale all percentage columns so that total percentage
  //   is 100%.
  float total_percentage = 0;
  for (NGTableTypes::Column& column : column_constraints->data) {
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
    for (NGTableTypes::Column& column : column_constraints->data) {
      if (column.percent)
        column.percent = *column.percent * 100 / total_percentage;
    }
  }
}

template <typename RowCountFunc>
NGTableTypes::Row ComputeMinimumRowBlockSize(
    const RowCountFunc& row_count_func,
    const NGBlockNode& row,
    const LayoutUnit cell_percentage_inline_size,
    const bool is_table_block_size_specified,
    const Vector<NGTableColumnLocation>& column_locations,
    const NGTableBorders& table_borders,
    wtf_size_t start_row_index,
    wtf_size_t row_index,
    wtf_size_t section_index,
    bool is_section_collapsed,
    NGTableTypes::CellBlockConstraints* cell_block_constraints,
    NGTableTypes::RowspanCells* rowspan_cells,
    NGColspanCellTabulator* colspan_cell_tabulator) {
  const WritingDirectionMode table_writing_direction =
      row.Style().GetWritingDirection();
  const bool has_collapsed_borders = table_borders.IsCollapsed();

  // TODO(layout-ng) Scrollbars should be frozen when computing row sizes.
  // This cannot be done today, because fragments with frozen scrollbars
  // will be cached. Needs to be fixed in NG framework.

  LayoutUnit max_cell_block_size;
  absl::optional<float> row_percent;
  bool is_constrained = false;
  bool has_rowspan_start = false;
  wtf_size_t start_cell_index = cell_block_constraints->size();
  NGRowBaselineTabulator row_baseline_tabulator;

  // Gather block sizes of all cells.
  for (NGBlockNode cell = To<NGBlockNode>(row.FirstChild()); cell;
       cell = To<NGBlockNode>(cell.NextSibling())) {
    colspan_cell_tabulator->FindNextFreeColumn();
    const ComputedStyle& cell_style = cell.Style();
    const auto cell_writing_direction = cell_style.GetWritingDirection();
    const NGBoxStrut cell_borders = table_borders.CellBorder(
        cell, row_index, colspan_cell_tabulator->CurrentColumn(), section_index,
        table_writing_direction);

    // Clamp the rowspan if it exceeds the total section row-count.
    wtf_size_t effective_rowspan = cell.TableCellRowspan();
    if (effective_rowspan > 1) {
      const wtf_size_t max_rows =
          row_count_func() - (row_index - start_row_index);
      effective_rowspan = std::min(max_rows, effective_rowspan);
    }

    NGConstraintSpaceBuilder space_builder(
        table_writing_direction.GetWritingMode(), cell_writing_direction,
        /* is_new_fc */ true);

    // We want these values to match the "layout" pass as close as possible.
    NGTableAlgorithmUtils::SetupTableCellConstraintSpaceBuilder(
        table_writing_direction, cell, cell_borders, column_locations,
        /* cell_block_size */ kIndefiniteSize, cell_percentage_inline_size,
        /* alignment_baseline */ absl::nullopt,
        colspan_cell_tabulator->CurrentColumn(),
        /* is_initial_block_size_indefinite */ true,
        is_table_block_size_specified, has_collapsed_borders,
        NGCacheSlot::kMeasure, &space_builder);

    const auto cell_space = space_builder.ToConstraintSpace();
    const NGLayoutResult* layout_result = cell.Layout(cell_space);

    const NGBoxFragment fragment(
        table_writing_direction,
        To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment()));
    const Length& cell_specified_block_length =
        IsParallelWritingMode(table_writing_direction.GetWritingMode(),
                              cell_style.GetWritingMode())
            ? cell_style.LogicalHeight()
            : cell_style.LogicalWidth();

    bool has_descendant_that_depends_on_percentage_block_size =
        layout_result->HasDescendantThatDependsOnPercentageBlockSize();
    bool has_effective_rowspan = effective_rowspan > 1;

    NGTableTypes::CellBlockConstraint cell_block_constraint = {
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
        fragment, NGTableAlgorithmUtils::IsBaseline(cell_style.VerticalAlign()),
        has_effective_rowspan,
        has_descendant_that_depends_on_percentage_block_size);

    // Compute cell's css block size.
    absl::optional<LayoutUnit> cell_css_block_size;
    absl::optional<float> cell_css_percent;

    // TODO(1105272) Handle cell_specified_block_length.IsCalculated()
    if (cell_specified_block_length.IsPercent()) {
      cell_css_percent = cell_specified_block_length.Percent();
    } else if (cell_specified_block_length.IsFixed()) {
      // NOTE: Ignore min/max-height for determining the |cell_css_block_size|.
      NGBoxStrut cell_padding = ComputePadding(cell_space, cell_style);
      NGBoxStrut border_padding = cell_borders + cell_padding;
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
      rowspan_cells->push_back(NGTableTypes::RowspanCell{
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
  absl::optional<LayoutUnit> row_baseline;
  if (!row_baseline_tabulator.BaselineDependsOnPercentageBlockDescendant())
    row_baseline = row_baseline_tabulator.ComputeBaseline(row_block_size);

  return NGTableTypes::Row{
      row_block_size,
      start_cell_index,
      cell_block_constraints->size() - start_cell_index,
      row_baseline,
      row_percent,
      is_constrained,
      has_rowspan_start,
      /* is_collapsed */ is_section_collapsed ||
          row.Style().Visibility() == EVisibility::kCollapse};
}

// Computes inline constraints for COLGROUP/COLs.
class ColumnConstraintsBuilder {
 public:
  void VisitCol(const NGLayoutInputNode& column,
                wtf_size_t start_column_index,
                wtf_size_t span) {
    // COL creates SPAN constraints. Its width is col css width, or enclosing
    // colgroup css width.
    NGTableTypes::Column col_constraint =
        NGTableTypes::CreateColumn(column.Style(),
                                   !is_fixed_layout_ && colgroup_constraint_
                                       ? colgroup_constraint_->max_inline_size
                                       : absl::nullopt,
                                   is_fixed_layout_);
    for (wtf_size_t i = 0; i < span; ++i)
      column_constraints_->data.push_back(col_constraint);
    column.GetLayoutBox()->ClearNeedsLayout();
  }

  void EnterColgroup(const NGLayoutInputNode& colgroup,
                     wtf_size_t start_column_index) {
    colgroup_constraint_ = NGTableTypes::CreateColumn(
        colgroup.Style(), absl::nullopt, is_fixed_layout_);
  }

  void LeaveColgroup(const NGLayoutInputNode& colgroup,
                     wtf_size_t start_column_index,
                     wtf_size_t span,
                     bool has_children) {
    if (!has_children) {
      for (wtf_size_t i = 0; i < span; ++i)
        column_constraints_->data.push_back(*colgroup_constraint_);
    }
    colgroup_constraint_.reset();
    colgroup.GetLayoutBox()->ClearNeedsLayout();
    To<LayoutNGTableColumn>(colgroup.GetLayoutBox())
        ->ClearNeedsLayoutForChildren();
  }

  ColumnConstraintsBuilder(NGTableTypes::Columns* column_constraints,
                           bool is_fixed_layout)
      : column_constraints_(column_constraints),
        is_fixed_layout_(is_fixed_layout) {}

 private:
  NGTableTypes::Columns* column_constraints_;
  bool is_fixed_layout_;
  absl::optional<NGTableTypes::Column> colgroup_constraint_;
};

// Computes constraints specified on column elements.
void ComputeColumnElementConstraints(
    const HeapVector<NGBlockNode>& columns,
    bool is_fixed_layout,
    NGTableTypes::Columns* column_constraints) {
  ColumnConstraintsBuilder constraints_builder(column_constraints,
                                               is_fixed_layout);
  // |table_column_count| is UINT_MAX because columns will get trimmed later.
  VisitLayoutNGTableColumn(columns, UINT_MAX, &constraints_builder);
}

void ComputeSectionInlineConstraints(
    const NGBlockNode& section,
    bool is_fixed_layout,
    bool is_first_section,
    WritingDirectionMode table_writing_direction,
    const NGTableBorders& table_borders,
    wtf_size_t section_index,
    wtf_size_t* row_index,
    NGTableTypes::CellInlineConstraints* cell_inline_constraints,
    NGTableTypes::ColspanCells* colspan_cell_inline_constraints) {
  NGColspanCellTabulator colspan_cell_tabulator;
  bool is_first_row = true;
  for (NGBlockNode row = To<NGBlockNode>(section.FirstChild()); row;
       row = To<NGBlockNode>(row.NextSibling())) {
    colspan_cell_tabulator.StartRow();

    // Gather constraints for each cell, and merge them into
    // CellInlineConstraints.
    for (NGBlockNode cell = To<NGBlockNode>(row.FirstChild()); cell;
         cell = To<NGBlockNode>(cell.NextSibling())) {
      colspan_cell_tabulator.FindNextFreeColumn();
      wtf_size_t colspan = cell.TableCellColspan();

      bool ignore_because_of_fixed_layout =
          is_fixed_layout && (!is_first_section || !is_first_row);

      wtf_size_t max_column = NGTableAlgorithmHelpers::ComputeMaxColumn(
          colspan_cell_tabulator.CurrentColumn(), colspan, is_fixed_layout);
      if (max_column >= cell_inline_constraints->size())
        cell_inline_constraints->Grow(max_column);
      if (!ignore_because_of_fixed_layout) {
        NGBoxStrut cell_border = table_borders.CellBorder(
            cell, *row_index, colspan_cell_tabulator.CurrentColumn(),
            section_index, table_writing_direction);
        NGBoxStrut cell_padding = table_borders.CellPaddingForMeasure(
            cell.Style(), table_writing_direction);
        NGTableTypes::CellInlineConstraint cell_constraint =
            NGTableTypes::CreateCellInlineConstraint(
                cell, table_writing_direction, is_fixed_layout, cell_border,
                cell_padding);
        if (colspan == 1) {
          absl::optional<NGTableTypes::CellInlineConstraint>& constraint =
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

}  // namespace

// static
NGTableAlgorithmUtils::CellBlockSizeData
NGTableAlgorithmUtils::ComputeCellBlockSize(
    const NGTableTypes::CellBlockConstraint& cell_block_constraint,
    const NGTableTypes::Rows& rows,
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

// static
void NGTableAlgorithmUtils::SetupTableCellConstraintSpaceBuilder(
    const WritingDirectionMode table_writing_direction,
    const NGBlockNode cell,
    const NGBoxStrut& cell_borders,
    const Vector<NGTableColumnLocation>& column_locations,
    LayoutUnit cell_block_size,
    LayoutUnit percentage_inline_size,
    absl::optional<LayoutUnit> alignment_baseline,
    wtf_size_t start_column,
    bool is_initial_block_size_indefinite,
    bool is_table_block_size_specified,
    bool has_collapsed_borders,
    NGCacheSlot cache_slot,
    NGConstraintSpaceBuilder* builder) {
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
  builder->SetIsTableCellHiddenForPaint(is_hidden_for_paint);
  builder->SetIsTableCellWithCollapsedBorders(has_collapsed_borders);
  builder->SetHideTableCellIfEmpty(
      !has_collapsed_borders && cell_style.EmptyCells() == EEmptyCells::kHide);
  builder->SetCacheSlot(cache_slot);
}

// Computes maximum possible number of non-mergeable columns.
wtf_size_t NGTableAlgorithmUtils::ComputeMaximumNonMergeableColumnCount(
    const HeapVector<NGBlockNode>& columns,
    bool is_fixed_layout) {
  // Build column constraints.
  scoped_refptr<NGTableTypes::Columns> column_constraints =
      base::MakeRefCounted<NGTableTypes::Columns>();
  ColumnConstraintsBuilder constraints_builder(column_constraints.get(),
                                               is_fixed_layout);
  VisitLayoutNGTableColumn(columns, UINT_MAX, &constraints_builder);
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

scoped_refptr<NGTableTypes::Columns>
NGTableAlgorithmUtils::ComputeColumnConstraints(
    const NGBlockNode& table,
    const NGTableGroupedChildren& grouped_children,
    const NGTableBorders& table_borders,
    const NGBoxStrut& border_padding) {
  const auto& table_style = table.Style();
  bool is_fixed_layout = table_style.IsFixedTableLayout();

  NGTableTypes::CellInlineConstraints cell_inline_constraints;
  NGTableTypes::ColspanCells colspan_cell_constraints;

  scoped_refptr<NGTableTypes::Columns> column_constraints =
      base::MakeRefCounted<NGTableTypes::Columns>();
  ComputeColumnElementConstraints(grouped_children.columns, is_fixed_layout,
                                  column_constraints.get());

  // Collect section constraints
  bool is_first_section = true;
  wtf_size_t row_index = 0;
  wtf_size_t section_index = 0;
  for (NGBlockNode section : grouped_children) {
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

void NGTableAlgorithmUtils::ComputeSectionMinimumRowBlockSizes(
    const NGBlockNode& section,
    const LayoutUnit cell_percentage_inline_size,
    const bool is_table_block_size_specified,
    const Vector<NGTableColumnLocation>& column_locations,
    const NGTableBorders& table_borders,
    const LayoutUnit block_border_spacing,
    wtf_size_t section_index,
    bool treat_section_as_tbody,
    NGTableTypes::Sections* sections,
    NGTableTypes::Rows* rows,
    NGTableTypes::CellBlockConstraints* cell_block_constraints) {
  // In rare circumstances we need to know the total row count before we've
  // visited all them (for computing effective rowspans). We don't want to
  // perform this unnecessarily.
  absl::optional<wtf_size_t> row_count;
  auto RowCountFunc = [&]() -> wtf_size_t {
    if (!row_count) {
      row_count = 0;
      for (NGBlockNode row = To<NGBlockNode>(section.FirstChild()); row;
           row = To<NGBlockNode>(row.NextSibling())) {
        (*row_count)++;
      }
    }

    return *row_count;
  };

  wtf_size_t start_row = rows->size();
  wtf_size_t current_row = start_row;
  NGTableTypes::RowspanCells rowspan_cells;
  LayoutUnit section_block_size;
  // Used to compute column index.
  NGColspanCellTabulator colspan_cell_tabulator;
  // total_row_percent must be under 100%
  float total_row_percent = 0;
  // Get minimum block size of each row.
  for (NGBlockNode row = To<NGBlockNode>(section.FirstChild()); row;
       row = To<NGBlockNode>(row.NextSibling())) {
    colspan_cell_tabulator.StartRow();
    NGTableTypes::Row row_constraint = ComputeMinimumRowBlockSize(
        RowCountFunc, row, cell_percentage_inline_size,
        is_table_block_size_specified, column_locations, table_borders,
        start_row, current_row++, section_index,
        /* is_section_collapsed */ section.Style().Visibility() ==
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
    NGTableAlgorithmHelpers::DistributeRowspanCellToRows(
        rowspan_cell, block_border_spacing, rows);
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
      NGTableAlgorithmHelpers::DistributeSectionFixedBlockSizeToRows(
          start_row, current_row - start_row, section_fixed_block_size,
          block_border_spacing, section_fixed_block_size, rows);
      section_block_size = section_fixed_block_size;
    }
  }
  sections->push_back(
      NGTableTypes::CreateSection(section, start_row, current_row - start_row,
                                  section_block_size, treat_section_as_tbody));
}

void NGTableAlgorithmUtils::FinalizeTableCellLayout(
    LayoutUnit unconstrained_intrinsic_block_size,
    NGBoxFragmentBuilder* builder) {
  const NGBlockNode& node = builder->Node();
  const NGConstraintSpace& space = builder->ConstraintSpace();
  const bool has_inflow_children = !builder->Children().empty();

  // Hide table-cells if:
  //  - They are within a collapsed column(s).
  //  - They have "empty-cells: hide", non-collapsed borders, and no children.
  builder->SetIsHiddenForPaint(
      space.IsTableCellHiddenForPaint() ||
      (space.HideTableCellIfEmpty() && !has_inflow_children));
  builder->SetHasCollapsedBorders(space.IsTableCellWithCollapsedBorders());
  builder->SetIsTableNGPart();
  builder->SetTableCellColumnIndex(space.TableCellColumnIndex());

  // If we're resuming after a break, there'll be no alignment, since the
  // fragment will start at the block-start edge of the fragmentainer then.
  if (IsBreakInside(builder->PreviousBreakToken()))
    return;

  switch (node.Style().VerticalAlign()) {
    case EVerticalAlign::kTop:
      // Do nothing for 'top' vertical alignment.
      break;
    case EVerticalAlign::kBaselineMiddle:
    case EVerticalAlign::kSub:
    case EVerticalAlign::kSuper:
    case EVerticalAlign::kTextTop:
    case EVerticalAlign::kTextBottom:
    case EVerticalAlign::kLength:
      // All of the above are treated as 'baseline' for the purposes of
      // table-cell vertical alignment.
    case EVerticalAlign::kBaseline:
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
    case EVerticalAlign::kMiddle:
      builder->MoveChildrenInBlockDirection(
          (builder->FragmentBlockSize() - unconstrained_intrinsic_block_size) /
          2);
      break;
    case EVerticalAlign::kBottom:
      builder->MoveChildrenInBlockDirection(builder->FragmentBlockSize() -
                                            unconstrained_intrinsic_block_size);
      break;
  };
}

void NGColspanCellTabulator::StartRow() {
  current_column_ = 0;
}

// Remove colspanned cells that are not spanning any more rows.
void NGColspanCellTabulator::EndRow() {
  for (wtf_size_t i = 0; i < colspanned_cells_.size();) {
    colspanned_cells_[i].remaining_rows--;
    if (colspanned_cells_[i].remaining_rows == 0)
      colspanned_cells_.EraseAt(i);
    else
      ++i;
  }
  std::sort(colspanned_cells_.begin(), colspanned_cells_.end(),
            [](const NGColspanCellTabulator::Cell& a,
               const NGColspanCellTabulator::Cell& b) {
              return a.column_start < b.column_start;
            });
}

// Advance current column to position not occupied by colspanned cells.
void NGColspanCellTabulator::FindNextFreeColumn() {
  for (const Cell& colspanned_cell : colspanned_cells_) {
    if (colspanned_cell.column_start <= current_column_ &&
        colspanned_cell.column_start + colspanned_cell.span > current_column_) {
      current_column_ = colspanned_cell.column_start + colspanned_cell.span;
    }
  }
}

void NGColspanCellTabulator::ProcessCell(const NGBlockNode& cell) {
  wtf_size_t colspan = cell.TableCellColspan();
  wtf_size_t rowspan = cell.TableCellRowspan();
  if (rowspan > 1)
    colspanned_cells_.emplace_back(current_column_, colspan, rowspan);
  current_column_ += colspan;
}

void NGRowBaselineTabulator::ProcessCell(
    const NGBoxFragment& fragment,
    const bool is_baseline_aligned,
    const bool is_rowspanned,
    const bool descendant_depends_on_percentage_block_size) {
  if (is_baseline_aligned && fragment.HasDescendantsForTablePart() &&
      fragment.FirstBaseline()) {
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

LayoutUnit NGRowBaselineTabulator::ComputeRowBlockSize(
    const LayoutUnit max_cell_block_size) {
  if (max_cell_ascent_) {
    return std::max(max_cell_block_size,
                    *max_cell_ascent_ + *max_cell_descent_);
  }
  return max_cell_block_size;
}

LayoutUnit NGRowBaselineTabulator::ComputeBaseline(
    const LayoutUnit row_block_size) {
  if (max_cell_ascent_)
    return *max_cell_ascent_;
  if (fallback_cell_descent_)
    return (row_block_size - *fallback_cell_descent_).ClampNegativeToZero();
  // Empty row's baseline is top.
  return LayoutUnit();
}

bool NGRowBaselineTabulator::BaselineDependsOnPercentageBlockDescendant() {
  if (max_cell_ascent_)
    return max_cell_baseline_depends_on_percentage_block_descendant_;
  if (fallback_cell_descent_)
    return fallback_cell_depends_on_percentage_block_descendant_;
  return false;
}

}  // namespace blink
