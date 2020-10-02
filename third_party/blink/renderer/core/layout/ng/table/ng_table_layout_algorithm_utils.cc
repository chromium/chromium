// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column_visitor.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_helpers.h"

namespace blink {

namespace {

NGTableTypes::Row ComputeMinimumRowBlockSize(
    const NGBlockNode& row,
    const LayoutUnit cell_percentage_inline_size,
    const bool is_restricted_block_size_table,
    const NGTableTypes::ColumnLocations& column_locations,
    const NGTableBorders& table_borders,
    wtf_size_t row_index,
    wtf_size_t section_index,
    bool is_section_collapsed,
    NGTableTypes::CellBlockConstraints* cell_block_constraints,
    NGTableTypes::RowspanCells* rowspan_cells,
    NGColspanCellTabulator* colspan_cell_tabulator) {
  const WritingDirectionMode table_writing_direction =
      row.Style().GetWritingDirection();

  auto CreateCellConstraintSpace = [&column_locations, &table_writing_direction,
                                    &is_restricted_block_size_table,
                                    &cell_percentage_inline_size](
                                       const NGBlockNode& cell,
                                       wtf_size_t start_column_index,
                                       const NGBoxStrut& cell_borders) {
    wtf_size_t start_column = start_column_index;
    wtf_size_t end_column = std::min(start_column + cell.TableCellColspan() - 1,
                                     column_locations.size() - 1);
    LayoutUnit cell_inline_size = column_locations[end_column].offset +
                                  column_locations[end_column].size -
                                  column_locations[start_column].offset;
    // TODO(crbug.com/736072): Support orthogonal table cells.
    // See http://wpt.live/css/css-writing-modes/table-cell-001.html
    NGConstraintSpaceBuilder builder(table_writing_direction.GetWritingMode(),
                                     cell.Style().GetWritingMode(),
                                     /* is_new_fc */ true);
    builder.SetTextDirection(cell.Style().Direction());
    builder.SetTableCellBorders(cell_borders);
    if (!IsParallelWritingMode(table_writing_direction.GetWritingMode(),
                               cell.Style().GetWritingMode())) {
      PhysicalSize icb_size = cell.InitialContainingBlockSize();
      builder.SetOrthogonalFallbackInlineSize(
          IsHorizontalWritingMode(table_writing_direction.GetWritingMode())
              ? icb_size.height
              : icb_size.width);

      builder.SetIsShrinkToFit(cell.Style().LogicalWidth().IsAuto());
    }

    builder.SetAvailableSize(LogicalSize(cell_inline_size, kIndefiniteSize));
    // Standard:
    // https://www.w3.org/TR/css-tables-3/#computing-the-table-height "the
    // computed height (if definite, percentages being considered 0px)"
    LogicalSize percentage_resolution_size(cell_percentage_inline_size,
                                           kIndefiniteSize);
    builder.SetPercentageResolutionSize(percentage_resolution_size);
    builder.SetReplacedPercentageResolutionSize(percentage_resolution_size);
    builder.SetIsFixedInlineSize(true);
    builder.SetIsTableCell(true, /* is_legacy_table_cell */ false);
    builder.SetIsRestrictedBlockSizeTableCell(is_restricted_block_size_table);
    builder.SetNeedsBaseline(true);
    builder.SetCacheSlot(NGCacheSlot::kMeasure);
    return builder.ToConstraintSpace();
  };

  // TODO(layout-ng) Scrollbars should be frozen when computing row sizes.
  // This cannot be done today, because fragments with frozen scrollbars
  // will be cached. Needs to be fixed in NG framework.

  base::Optional<LayoutUnit> max_baseline;
  LayoutUnit max_descent;
  LayoutUnit row_block_size;
  base::Optional<float> row_percent;
  bool is_constrained = false;
  bool baseline_depends_on_percentage_block_size_descendant = false;
  bool has_rowspan_start = false;
  wtf_size_t start_cell_index = cell_block_constraints->size();

  // Gather block sizes of all cells.
  for (NGBlockNode cell = To<NGBlockNode>(row.FirstChild()); cell;
       cell = To<NGBlockNode>(cell.NextSibling())) {
    colspan_cell_tabulator->FindNextFreeColumn();
    const ComputedStyle& cell_style = cell.Style();
    const NGBoxStrut cell_borders = table_borders.CellBorder(
        cell, row_index, colspan_cell_tabulator->CurrentColumn(), section_index,
        table_writing_direction);
    const NGConstraintSpace cell_constraint_space = CreateCellConstraintSpace(
        cell, colspan_cell_tabulator->CurrentColumn(), cell_borders);
    scoped_refptr<const NGLayoutResult> layout_result =
        cell.Layout(cell_constraint_space);
    const NGBoxFragment fragment(
        table_writing_direction.GetWritingMode(),
        table_writing_direction.Direction(),
        To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment()));
    bool is_parallel =
        IsParallelWritingMode(table_writing_direction.GetWritingMode(),
                              cell.Style().GetWritingMode());
    LayoutUnit baseline;
    // https://www.w3.org/TR/css-tables-3/#row-layout "If there is no such
    // line box or table-row, the baseline is the bottom of content edge of
    // the cell box."
    // Only baseline-aligned cells contribute to row baseline.
    if (is_parallel &&
        NGTableAlgorithmUtils::IsBaseline(cell_style.VerticalAlign())) {
      if (layout_result->HasDescendantThatDependsOnPercentageBlockSize())
        baseline_depends_on_percentage_block_size_descendant = true;
      baseline = fragment.FirstBaselineOrSynthesize();
      max_baseline = std::max(max_baseline.value_or(LayoutUnit()), baseline);
    }

    const wtf_size_t rowspan = cell.TableCellRowspan();
    NGTableTypes::CellBlockConstraint cell_block_constraint =
        NGTableTypes::CreateCellBlockConstraint(
            cell, fragment.BlockSize(), baseline, cell_borders, row_index,
            colspan_cell_tabulator->CurrentColumn(), rowspan);
    colspan_cell_tabulator->ProcessCell(cell);
    cell_block_constraints->push_back(cell_block_constraint);
    is_constrained |= cell_block_constraint.is_constrained && rowspan == 1;

    // Compute cell's css block size.
    base::Optional<LayoutUnit> cell_css_block_size;
    base::Optional<float> cell_css_percent;
    const Length& cell_specified_block_length =
        is_parallel ? cell_style.LogicalHeight() : cell_style.LogicalWidth();

    // TODO(1105272) Handle cell_specified_block_length.IsCalculated()
    if (cell_specified_block_length.IsPercent()) {
      cell_css_percent = cell_specified_block_length.Percent();
    } else if (cell_specified_block_length.IsFixed()) {
      // NOTE: Ignore min/max-height for determining the |cell_css_block_size|.
      NGBoxStrut cell_padding =
          ComputePadding(cell_constraint_space, cell_style);
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

    if (rowspan == 1) {
      if (cell_css_block_size || cell_css_percent)
        is_constrained = true;
      if (cell_css_percent)
        row_percent = std::max(row_percent.value_or(0), *cell_css_percent);
      // Cell's block layout ignores CSS block size properties. Row must use it
      // to compute it's minimum block size.
      if (cell_css_block_size)
        row_block_size = std::max(row_block_size, *cell_css_block_size);
      if (NGTableAlgorithmUtils::IsBaseline(
              cell_block_constraint.vertical_align)) {
        max_descent = std::max(max_descent,
                               cell_block_constraint.min_block_size - baseline);
        row_block_size = std::max(
            row_block_size, max_baseline.value_or(LayoutUnit()) + max_descent);
      } else {
        row_block_size =
            std::max(row_block_size, cell_block_constraint.min_block_size);
      }
    } else {
      has_rowspan_start = true;
      rowspan_cells->push_back(NGTableTypes::CreateRowspanCell(
          row_index, rowspan, &cell_block_constraint, cell_css_block_size));
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
    row_block_size = std::max(LayoutUnit(row_specified_block_length.Value()),
                              row_block_size);
  }

  return NGTableTypes::Row{
      row_block_size,
      max_baseline.value_or(row_block_size),
      row_percent,
      start_cell_index,
      cell_block_constraints->size() - start_cell_index,
      is_constrained,
      baseline_depends_on_percentage_block_size_descendant,
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
    NGTableTypes::Column col_constraint = NGTableTypes::CreateColumn(
        column.Style(), !is_fixed_layout_ && colgroup_constraint_
                            ? colgroup_constraint_->max_inline_size
                            : base::nullopt);
    for (wtf_size_t i = 0; i < span; ++i)
      column_constraints_->data.push_back(col_constraint);
    column.GetLayoutBox()->ClearNeedsLayout();
  }

  void EnterColgroup(const NGLayoutInputNode& colgroup,
                     wtf_size_t start_column_index) {
    colgroup_constraint_ =
        NGTableTypes::CreateColumn(colgroup.Style(), base::nullopt);
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
  base::Optional<NGTableTypes::Column> colgroup_constraint_;
};

}  // namespace

void NGTableAlgorithmUtils::ComputeColumnInlineConstraints(
    const Vector<NGBlockNode>& columns,
    bool is_fixed_layout,
    NGTableTypes::Columns* column_constraints) {
  ColumnConstraintsBuilder constraints_builder(column_constraints,
                                               is_fixed_layout);
  // |table_column_count| is UINT_MAX because columns will get trimmed later.
  VisitLayoutNGTableColumn(columns, UINT_MAX, &constraints_builder);
}

void NGTableAlgorithmUtils::ComputeSectionInlineConstraints(
    const NGBlockNode& section,
    bool is_fixed_layout,
    bool is_first_section,
    WritingMode table_writing_mode,
    const NGTableBorders& table_borders,
    wtf_size_t section_index,
    wtf_size_t* row_index,
    NGTableTypes::CellInlineConstraints* cell_inline_constraints,
    NGTableTypes::ColspanCells* colspan_cell_inline_constraints) {
  WritingDirectionMode table_writing_direction =
      section.Style().GetWritingDirection();
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
                cell, table_writing_mode, is_fixed_layout, cell_border,
                cell_padding, table_borders.IsCollapsed());
        if (colspan == 1) {
          base::Optional<NGTableTypes::CellInlineConstraint>& constraint =
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

void NGTableAlgorithmUtils::ComputeSectionMinimumRowBlockSizes(
    const NGBlockNode& section,
    const LayoutUnit cell_percentage_inline_size,
    const bool is_restricted_block_size_table,
    const NGTableTypes::ColumnLocations& column_locations,
    const NGTableBorders& table_borders,
    const LayoutUnit block_border_spacing,
    wtf_size_t section_index,
    NGTableTypes::Sections* sections,
    NGTableTypes::Rows* rows,
    NGTableTypes::CellBlockConstraints* cell_block_constraints) {
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
        row, cell_percentage_inline_size, is_restricted_block_size_table,
        column_locations, table_borders, current_row++, section_index,
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
  const wtf_size_t block_spacing_count =
      current_row == start_row ? 0 : current_row - start_row - 1;
  const LayoutUnit border_spacing_total =
      block_border_spacing * block_spacing_count;
  section_block_size += border_spacing_total;

  // Redistribute rowspanned cell block sizes.
  std::stable_sort(rowspan_cells.begin(), rowspan_cells.end());
  for (NGTableTypes::RowspanCell& rowspan_cell : rowspan_cells) {
    // Spec: rowspan of 0 means all remaining rows.
    if (rowspan_cell.span == 0)
      rowspan_cell.span = current_row - rowspan_cell.start_row;
    // Truncate rows that are too long.
    rowspan_cell.span =
        std::min(current_row - rowspan_cell.start_row, rowspan_cell.span);
    NGTableAlgorithmHelpers::DistributeRowspanCellToRows(
        rowspan_cell, block_border_spacing, rows);
  }

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
  sections->push_back(NGTableTypes::CreateSection(
      section, start_row, current_row - start_row, section_block_size));
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

}  // namespace blink
