// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_row_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"

namespace blink {

NGTableRowLayoutAlgorithm::NGTableRowLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {}

const NGLayoutResult* NGTableRowLayoutAlgorithm::Layout() {
  const NGTableConstraintSpaceData& table_data = *ConstraintSpace().TableData();
  const auto& row = table_data.rows[ConstraintSpace().TableRowIndex()];

  auto CreateCellConstraintSpace = [this, &row, &table_data](
                                       NGBlockNode cell, wtf_size_t cell_index,
                                       absl::optional<LayoutUnit> row_baseline,
                                       LayoutUnit* cell_inline_offset = nullptr,
                                       bool use_block_fragmentation = false) {
    const wtf_size_t start_column = table_data.cells[cell_index].start_column;
    const wtf_size_t end_column =
        std::min(start_column + cell.TableCellColspan() - 1,
                 table_data.column_locations.size() - 1);

    // When columns spanned by the cell are collapsed, the cell geometry is
    // defined by:
    // - The start edge of the first non-collapsed column.
    // - The end edge of the last non-collapsed column.
    // - If all columns are collapsed, the |cell_inline_size| is defined by the
    //   edges of the last column. Picking last column is arbitrary, any
    //   spanned column would work, as all spanned columns define the same
    //   geometry: same location, zero width.
    wtf_size_t cell_location_start_column = start_column;
    while (
        table_data.column_locations[cell_location_start_column].is_collapsed &&
        cell_location_start_column < end_column)
      cell_location_start_column++;
    wtf_size_t cell_location_end_column = end_column;
    while (table_data.column_locations[cell_location_end_column].is_collapsed &&
           cell_location_end_column > cell_location_start_column)
      cell_location_end_column--;

    if (cell_inline_offset) {
      *cell_inline_offset =
          table_data.column_locations[cell_location_start_column].offset;
    }

    const NGTableConstraintSpaceData::Cell& cell_data =
        table_data.cells[cell_index];
    const LayoutUnit cell_inline_size =
        table_data.column_locations[cell_location_end_column].offset +
        table_data.column_locations[cell_location_end_column].inline_size -
        table_data.column_locations[cell_location_start_column].offset;
    const LayoutUnit cell_block_size =
        row.is_collapsed ? LayoutUnit() : cell_data.block_size;

    // Our initial block-size is definite if this cell has a fixed block-size,
    // or we have grown and the table has a specified block-size.
    const bool is_initial_block_size_definite =
        cell_data.is_constrained ||
        (cell_data.has_grown && table_data.is_table_block_size_specified);

    const bool is_hidden_for_paint =
        table_data.column_locations[cell_location_start_column].is_collapsed &&
        cell_location_start_column == cell_location_end_column;

    NGConstraintSpaceBuilder builder =
        NGTableAlgorithmUtils::CreateTableCellConstraintSpaceBuilder(
            table_data.table_writing_direction, cell, cell_data.borders,
            {cell_inline_size, cell_block_size},
            container_builder_.InlineSize(), row_baseline, start_column,
            !is_initial_block_size_definite,
            table_data.is_table_block_size_specified, is_hidden_for_paint,
            table_data.has_collapsed_borders, NGCacheSlot::kLayout);

    if (use_block_fragmentation) {
      SetupSpaceBuilderForFragmentation(
          ConstraintSpace(), cell,
          /* fragmentainer_offset_delta */ LayoutUnit(), &builder,
          /* is_new_fc */ true,
          container_builder_.RequiresContentBeforeBreaking());
    }

    return builder.ToConstraintSpace();
  };

  // A cell with perecentage block-size descendants can layout with size that
  // differs from its intrinsic size. This might cause row baseline to move, if
  // cell was baseline-aligned.
  // To compute correct baseline, we need to do an initial layout pass.
  LayoutUnit row_baseline = row.baseline;
  if (row.has_baseline_aligned_percentage_block_size_descendants) {
    wtf_size_t cell_index = row.start_cell_index;
    NGRowBaselineTabulator row_baseline_tabulator;
    for (NGBlockNode cell = To<NGBlockNode>(Node().FirstChild()); cell;
         cell = To<NGBlockNode>(cell.NextSibling()), ++cell_index) {
      NGConstraintSpace cell_constraint_space =
          CreateCellConstraintSpace(cell, cell_index, absl::nullopt);
      const NGLayoutResult* layout_result = cell.Layout(cell_constraint_space);
      NGBoxFragment fragment(
          table_data.table_writing_direction,
          To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment()));
      row_baseline_tabulator.ProcessCell(
          fragment,
          NGTableAlgorithmUtils::IsBaseline(cell.Style().VerticalAlign()),
          cell.TableCellRowspan() > 1,
          layout_result->HasDescendantThatDependsOnPercentageBlockSize());
    }
    row_baseline = row_baseline_tabulator.ComputeBaseline(row.block_size);
  }

  // Generate cell fragments.
  NGRowBaselineTabulator row_baseline_tabulator;
  NGBlockChildIterator child_iterator(Node().FirstChild(), BreakToken(),
                                      /* calculate_child_idx */ true);
  for (auto entry = child_iterator.NextChild();
       NGBlockNode cell = To<NGBlockNode>(entry.node);
       entry = child_iterator.NextChild()) {
    const auto* cell_break_token = To<NGBlockBreakToken>(entry.token);
    wtf_size_t cell_index = row.start_cell_index + *entry.index;
    LayoutUnit cell_inline_offset;
    NGConstraintSpace cell_constraint_space = CreateCellConstraintSpace(
        cell, cell_index, row_baseline, &cell_inline_offset,
        ConstraintSpace().HasBlockFragmentation());
    const NGLayoutResult* cell_result =
        cell.Layout(cell_constraint_space, cell_break_token);
    // TODO(mstensho): Propagate break-before and break-after values to the row.
    container_builder_.AddResult(
        *cell_result,
        {cell_inline_offset - table_data.table_border_spacing.inline_size,
         LayoutUnit()});
    NGBoxFragment fragment(
        table_data.table_writing_direction,
        To<NGPhysicalBoxFragment>(cell_result->PhysicalFragment()));
    row_baseline_tabulator.ProcessCell(
        fragment,
        NGTableAlgorithmUtils::IsBaseline(cell.Style().VerticalAlign()),
        cell.TableCellRowspan() > 1,
        cell_result->HasDescendantThatDependsOnPercentageBlockSize());
  }

  // Since we always visit all cells in a row (cannot break halfway through;
  // each cell establishes a parallel flows that needs to be examined
  // separately), we have seen all children by now.
  container_builder_.SetHasSeenAllChildren();

  container_builder_.SetFragmentsTotalBlockSize(row.block_size);

  container_builder_.SetBaseline(
      row_baseline_tabulator.ComputeBaseline(row.block_size));
  if (row.is_collapsed)
    container_builder_.SetIsHiddenForPaint(true);
  container_builder_.SetIsTableNGPart();

  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    NGBreakStatus status = FinishFragmentation(
        Node(), ConstraintSpace(), BorderPadding().block_end,
        FragmentainerSpaceAtBfcStart(ConstraintSpace()), &container_builder_);
    // TODO(mstensho): Deal with early-breaks.
    DCHECK_EQ(status, NGBreakStatus::kContinue);
  }

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();
  return container_builder_.ToBoxFragment();
}

}  // namespace blink
