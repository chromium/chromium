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

  auto CreateCellConstraintSpace =
      [this, &row, &table_data](
          NGBlockNode cell, const NGTableConstraintSpaceData::Cell& cell_data,
          bool min_block_size_should_encompass_intrinsic_size) {
        const LayoutUnit cell_block_size =
            cell_data.rowspan_block_size != kIndefiniteSize
                ? cell_data.rowspan_block_size
                : row.block_size;

        NGConstraintSpaceBuilder builder =
            NGTableAlgorithmUtils::CreateTableCellConstraintSpaceBuilder(
                table_data.table_writing_direction, cell, cell_data.borders,
                table_data.column_locations, cell_block_size,
                container_builder_.InlineSize(), row.baseline,
                cell_data.start_column,
                cell_data.is_initial_block_size_indefinite,
                table_data.is_table_block_size_specified,
                table_data.has_collapsed_borders, NGCacheSlot::kLayout);

        if (ConstraintSpace().HasBlockFragmentation()) {
          SetupSpaceBuilderForFragmentation(
              ConstraintSpace(), cell,
              /* fragmentainer_offset_delta */ LayoutUnit(), &builder,
              /* is_new_fc */ true,
              container_builder_.RequiresContentBeforeBreaking());

          if (min_block_size_should_encompass_intrinsic_size)
            builder.SetMinBlockSizeShouldEncompassIntrinsicSize();
        }

        return builder.ToConstraintSpace();
      };

  bool has_block_fragmentation = ConstraintSpace().HasBlockFragmentation();
  bool should_propagate_child_break_values =
      ConstraintSpace().ShouldPropagateChildBreakValues();

  auto MinBlockSizeShouldEncompassIntrinsicSize =
      [&](const NGBlockNode& cell,
          const NGTableConstraintSpaceData::Cell& cell_data) -> bool {
    if (!has_block_fragmentation)
      return false;

    if (cell.IsMonolithic())
      return false;

    // If this item has (any) descendant that is percentage based, we can end
    // up in a situation where we'll constantly try and expand the row. E.g.
    // <div style="display: table-cell; height: 100px;">
    //   <div style="height: 200%;"></div>
    // </div>
    if (cell_data.has_descendant_that_depends_on_percentage_block_size)
      return false;

    // If we have a cell which has rowspan - only disable encompassing if it
    // (actually) spans more than one non-empty row.
    bool has_rowspan = cell_data.rowspan_block_size != kIndefiniteSize;
    if (has_rowspan) {
      if (cell_data.rowspan_block_size != row.block_size)
        return false;
    }

    return true;
  };

  EBreakBetween row_break_before = EBreakBetween::kAuto;
  EBreakBetween row_break_after = EBreakBetween::kAuto;

  // Generate cell fragments.
  NGRowBaselineTabulator row_baseline_tabulator;
  NGBlockChildIterator child_iterator(Node().FirstChild(), BreakToken(),
                                      /* calculate_child_idx */ true);
  for (auto entry = child_iterator.NextChild();
       NGBlockNode cell = To<NGBlockNode>(entry.node);
       entry = child_iterator.NextChild()) {
    const auto* cell_break_token = To<NGBlockBreakToken>(entry.token);
    const auto& cell_style = cell.Style();
    const wtf_size_t cell_index = row.start_cell_index + *entry.index;
    const NGTableConstraintSpaceData::Cell& cell_data =
        table_data.cells[cell_index];

    bool min_block_size_should_encompass_intrinsic_size =
        MinBlockSizeShouldEncompassIntrinsicSize(cell, cell_data);

    const auto cell_space = CreateCellConstraintSpace(
        cell, cell_data, min_block_size_should_encompass_intrinsic_size);
    const NGLayoutResult* cell_result =
        cell.Layout(cell_space, cell_break_token);

    const LayoutUnit inline_offset =
        table_data.column_locations[cell_data.start_column].offset -
        table_data.table_border_spacing.inline_size;
    container_builder_.AddResult(*cell_result, {inline_offset, LayoutUnit()});

    if (should_propagate_child_break_values) {
      auto cell_break_before = JoinFragmentainerBreakValues(
          cell_style.BreakBefore(), cell_result->InitialBreakBefore());
      auto cell_break_after = JoinFragmentainerBreakValues(
          cell_style.BreakAfter(), cell_result->FinalBreakAfter());
      row_break_before =
          JoinFragmentainerBreakValues(row_break_before, cell_break_before);
      row_break_after =
          JoinFragmentainerBreakValues(row_break_after, cell_break_after);
    }

    bool has_rowspan = cell_data.rowspan_block_size != kIndefiniteSize;
    NGBoxFragment fragment(
        table_data.table_writing_direction,
        To<NGPhysicalBoxFragment>(cell_result->PhysicalFragment()));
    row_baseline_tabulator.ProcessCell(
        fragment, NGTableAlgorithmUtils::IsBaseline(cell_style.VerticalAlign()),
        has_rowspan,
        cell_data.has_descendant_that_depends_on_percentage_block_size);
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

  if (should_propagate_child_break_values) {
    container_builder_.SetInitialBreakBefore(row_break_before);
    container_builder_.SetPreviousBreakAfter(row_break_after);
  }

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
