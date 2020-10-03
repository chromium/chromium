// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_row_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"

namespace blink {

NGTableRowLayoutAlgorithm::NGTableRowLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {}

MinMaxSizesResult NGTableRowLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput&) const {
  NOTREACHED();  // Table layout does not compute minmax for table row.
  return MinMaxSizesResult();
}

scoped_refptr<const NGLayoutResult> NGTableRowLayoutAlgorithm::Layout() {
  const NGTableConstraintSpaceData& table_data = *ConstraintSpace().TableData();
  wtf_size_t row_index = ConstraintSpace().TableRowIndex();

  auto CreateCellConstraintSpace = [this, &table_data](
                                       NGBlockNode cell, wtf_size_t cell_index,
                                       base::Optional<LayoutUnit> row_baseline,
                                       bool row_is_collapsed,
                                       wtf_size_t* cell_location_start_column) {
    wtf_size_t start_column = table_data.cells[cell_index].start_column;
    wtf_size_t end_column = std::min(start_column + cell.TableCellColspan() - 1,
                                     table_data.column_locations.size() - 1);
    *cell_location_start_column = start_column;
    // When columns spanned by the cell are collapsed, cell geometry is defined
    // by:
    // - start edge of first non-collapsed column.
    // - end edge of the last non-collapsed column.
    // - if all columns are collapsed, cell_inline_size is defined by edges
    //   of last column. Picking last column is arbitrary, any spanned column
    //   would work, as all spanned columns define the same geometry: same
    //   location, width: 0.
    while (
        table_data.column_locations[*cell_location_start_column].is_collapsed &&
        *cell_location_start_column < end_column)
      (*cell_location_start_column)++;
    wtf_size_t cell_location_end_column = end_column;
    while (table_data.column_locations[cell_location_end_column].is_collapsed &&
           cell_location_end_column > *cell_location_start_column)
      cell_location_end_column--;

    const NGTableConstraintSpaceData::Cell& cell_data =
        table_data.cells[cell_index];
    LayoutUnit cell_inline_size =
        table_data.column_locations[cell_location_end_column].offset +
        table_data.column_locations[cell_location_end_column].inline_size -
        table_data.column_locations[*cell_location_start_column].offset;
    LayoutUnit cell_block_size =
        row_is_collapsed ? LayoutUnit() : cell_data.block_size;

    NGConstraintSpaceBuilder builder(
        table_data.table_writing_direction.GetWritingMode(),
        cell.Style().GetWritingMode(), /* is_new_fc */ true);
    builder.SetAvailableSize({cell_inline_size, cell_block_size});
    builder.SetIsFixedInlineSize(true);
    builder.SetIsFixedBlockSize(true);
    builder.SetTextDirection(Style().Direction());

    PhysicalSize icb_size = Node().InitialContainingBlockSize();
    builder.SetOrthogonalFallbackInlineSize(
        IsHorizontalWritingMode(
            table_data.table_writing_direction.GetWritingMode())
            ? icb_size.height
            : icb_size.width);

    builder.SetPercentageResolutionSize(
        LogicalSize(container_builder_.InlineSize(), kIndefiniteSize));

    // Percentage block resolution size is only valid if
    // cell height is specified, or table height is specified.
    bool is_block_size_indefinite =
        !cell_data.is_constrained &&
        !table_data.treat_table_block_size_as_constrained;
    if (is_block_size_indefinite)
      builder.SetIsFixedBlockSizeIndefinite(true);
    builder.SetIsTableCell(/* is_table_cell */ true,
                           /* is_legacy_table_cell */ false);
    builder.SetTableCellBorders(cell_data.border_box_borders);
    builder.SetTableCellAlignmentBaseline(row_baseline);
    builder.SetTableCellColumnIndex(start_column);
    builder.SetNeedsBaseline(true);
    builder.SetIsTableCellHiddenForPaint(
        table_data.column_locations[*cell_location_start_column].is_collapsed &&
        *cell_location_start_column == cell_location_end_column);
    builder.SetHideTableCellIfEmpty(!table_data.has_collapsed_borders &&
                                    cell.Style().EmptyCells() ==
                                        EEmptyCells::kHide);

    return builder.ToConstraintSpace();
  };

  const NGTableConstraintSpaceData::Row& row = table_data.rows[row_index];
  // Cell with perecentage block size descendants can layout with
  // size that differs from its intrinsic size. This might cause
  // row baseline to move, if cell was baseline-aligned.
  // To compute correct baseline, we need to do an initial layout pass.
  WritingMode table_writing_mode = ConstraintSpace().GetWritingMode();
  LayoutUnit row_baseline = row.baseline;
  wtf_size_t cell_index = row.start_cell_index;
  if (row.has_baseline_aligned_percentage_block_size_descendants) {
    for (NGBlockNode cell = To<NGBlockNode>(Node().FirstChild()); cell;
         cell = To<NGBlockNode>(cell.NextSibling()), ++cell_index) {
      bool is_parallel = IsParallelWritingMode(table_writing_mode,
                                               cell.Style().GetWritingMode());
      if (!NGTableAlgorithmUtils::IsBaseline(cell.Style().VerticalAlign()) ||
          !is_parallel)
        continue;
      wtf_size_t cell_location_start_column;
      NGConstraintSpace cell_constraint_space = CreateCellConstraintSpace(
          cell, cell_index, base::nullopt, row.is_collapsed,
          &cell_location_start_column);
      scoped_refptr<const NGLayoutResult> layout_result =
          cell.Layout(cell_constraint_space);
      const NGBoxFragment fragment(
          table_data.table_writing_direction.GetWritingMode(),
          table_data.table_writing_direction.Direction(),
          To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment()));

      LayoutUnit baseline = fragment.FirstBaselineOrSynthesize();
      row_baseline = std::max(row_baseline, baseline);
    }
  }

  // Generate cell fragments.
  base::Optional<LayoutUnit> reported_row_baseline;
  cell_index = row.start_cell_index;
  for (NGBlockNode cell = To<NGBlockNode>(Node().FirstChild()); cell;
       cell = To<NGBlockNode>(cell.NextSibling()), ++cell_index) {
    wtf_size_t cell_location_start_column;
    NGConstraintSpace cell_constraint_space = CreateCellConstraintSpace(
        cell, cell_index, row_baseline, row.is_collapsed,
        &cell_location_start_column);
    scoped_refptr<const NGLayoutResult> cell_result =
        cell.Layout(cell_constraint_space);
    LogicalOffset cell_offset(
        table_data.column_locations[cell_location_start_column].offset -
            table_data.table_border_spacing.inline_size,
        LayoutUnit());
    container_builder_.AddResult(*cell_result, cell_offset);

    if (NGTableAlgorithmUtils::IsBaseline(cell.Style().VerticalAlign())) {
      const NGBoxFragment fragment(
          table_data.table_writing_direction.GetWritingMode(),
          table_data.table_writing_direction.Direction(),
          To<NGPhysicalBoxFragment>(cell_result->PhysicalFragment()));
      reported_row_baseline =
          std::max(reported_row_baseline.value_or(LayoutUnit::Min()),
                   fragment.FirstBaselineOrSynthesize());
    }
  }
  container_builder_.SetFragmentBlockSize(row.block_size);
  container_builder_.SetBaseline(
      reported_row_baseline.value_or(row.block_size));
  if (row.is_collapsed)
    container_builder_.SetIsHiddenForPaint(true);
  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();
  return container_builder_.ToBoxFragment();
}

}  // namespace blink
