// Copyright 2020 The Chromium Authors
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
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_row_break_token_data.h"

namespace blink {

struct ResultWithOffset {
  DISALLOW_NEW();

 public:
  Member<const NGLayoutResult> result;
  LogicalOffset offset;

  ResultWithOffset(const NGLayoutResult* result, LogicalOffset offset)
      : result(result), offset(offset) {}

  void Trace(Visitor* visitor) const { visitor->Trace(result); }
};

NGTableRowLayoutAlgorithm::NGTableRowLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {}

const NGLayoutResult* NGTableRowLayoutAlgorithm::Layout() {
  const NGTableConstraintSpaceData& table_data = *ConstraintSpace().TableData();
  const auto& row = table_data.rows[ConstraintSpace().TableRowIndex()];

  auto CreateCellConstraintSpace =
      [this, &table_data](
          NGBlockNode cell, const NGBlockBreakToken* cell_break_token,
          const NGTableConstraintSpaceData::Cell& cell_data,
          LayoutUnit row_block_size, absl::optional<LayoutUnit> row_baseline,
          bool min_block_size_should_encompass_intrinsic_size) {
        bool has_rowspan = cell_data.rowspan_block_size != kIndefiniteSize;
        LayoutUnit cell_block_size =
            has_rowspan ? cell_data.rowspan_block_size : row_block_size;

        if (IsBreakInside(cell_break_token) && IsBreakInside(BreakToken()) &&
            !has_rowspan) {
          // The table row may have consumed more space than the cell, if some
          // sibling cell has overflowed the fragmentainer. Subtract this
          // difference, so that this cell won't overflow the row - unless the
          // cell is rowspanned. In that case it doesn't make sense to
          // compensate against just the current row.
          cell_block_size -= BreakToken()->ConsumedBlockSize() -
                             cell_break_token->ConsumedBlockSize();
        }

        DCHECK_EQ(table_data.table_writing_direction.GetWritingMode(),
                  ConstraintSpace().GetWritingMode());

        NGConstraintSpaceBuilder builder(ConstraintSpace(),
                                         cell.Style().GetWritingDirection(),
                                         /* is_new_fc */ true);

        NGTableAlgorithmUtils::SetupTableCellConstraintSpaceBuilder(
            table_data.table_writing_direction, cell, cell_data.borders,
            table_data.column_locations, cell_block_size,
            container_builder_.InlineSize(), row_baseline,
            cell_data.start_column, cell_data.is_initial_block_size_indefinite,
            table_data.is_table_block_size_specified,
            table_data.has_collapsed_borders, NGCacheSlot::kLayout, &builder);

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

  LayoutUnit max_cell_block_size;
  EBreakBetween row_break_before;
  EBreakBetween row_break_after;
  NGRowBaselineTabulator row_baseline_tabulator;
  HeapVector<ResultWithOffset> results;
  bool has_inflow_break_inside = false;
  auto PlaceCells =
      [&](LayoutUnit row_block_size,
          absl::optional<LayoutUnit> row_baseline) -> NGLayoutResult::EStatus {
    // Reset our state.
    max_cell_block_size = LayoutUnit();
    row_break_before = EBreakBetween::kAuto;
    row_break_after = EBreakBetween::kAuto;
    row_baseline_tabulator = NGRowBaselineTabulator();
    results.clear();
    has_inflow_break_inside = false;

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
          cell, cell_break_token, cell_data, row_block_size, row_baseline,
          min_block_size_should_encompass_intrinsic_size);
      const NGLayoutResult* cell_result =
          cell.Layout(cell_space, cell_break_token);

      if (cell_result->Status() == NGLayoutResult::kOutOfFragmentainerSpace) {
        DCHECK(has_block_fragmentation);
        // If the cell establishes a multicol container (and we're already
        // inside another fragmentation context), it may have failed to produce
        // a fragment, because there wasn't enough space. We now need to
        // propagate the failure, to make some ancestor algorithm handle the
        // problem. We need to break before something further up.
        return NGLayoutResult::kOutOfFragmentainerSpace;
      }
      DCHECK_EQ(cell_result->Status(), NGLayoutResult::kSuccess);

      const LogicalOffset offset(
          table_data.column_locations[cell_data.start_column].offset -
              table_data.table_border_spacing.inline_size,
          LayoutUnit());
      if (has_block_fragmentation || !row_baseline)
        results.emplace_back(cell_result, offset);
      else
        container_builder_.AddResult(*cell_result, offset);

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
      const auto& physical_fragment =
          To<NGPhysicalBoxFragment>(cell_result->PhysicalFragment());
      const NGBoxFragment fragment(table_data.table_writing_direction,
                                   physical_fragment);
      row_baseline_tabulator.ProcessCell(
          fragment,
          NGTableAlgorithmUtils::IsBaseline(cell_style.VerticalAlign()),
          has_rowspan,
          cell_data.has_descendant_that_depends_on_percentage_block_size);
      if (min_block_size_should_encompass_intrinsic_size) {
        max_cell_block_size =
            std::max(max_cell_block_size, fragment.BlockSize());
      }

      if (const auto* outgoing_break_token = physical_fragment.BreakToken();
          outgoing_break_token && !has_inflow_break_inside && !has_rowspan) {
        has_inflow_break_inside = !outgoing_break_token->IsAtBlockEnd();
      }
    }

    return NGLayoutResult::kSuccess;
  };

  // Determine the baseline for the table-row if we haven't been provided a
  // cached one. This can happen if we have a %-block-size descendant which may
  // adjust the position of the baseline.
  //
  // We also don't perform baseline alignment if block-fragmentation is
  // present, as the alignment baseline may end up in another fragmentainer.
  absl::optional<LayoutUnit> row_baseline;
  if (!has_block_fragmentation) {
    row_baseline = row.baseline;
    if (!row_baseline) {
      PlaceCells(row.block_size, absl::nullopt);
      row_baseline = row_baseline_tabulator.ComputeBaseline(row.block_size);
    }
  }

  {
    NGLayoutResult::EStatus status = PlaceCells(row.block_size, row_baseline);
    if (status == NGLayoutResult::kOutOfFragmentainerSpace)
      return container_builder_.Abort(status);
    DCHECK_EQ(status, NGLayoutResult::kSuccess);
  }

  LayoutUnit previous_consumed_row_block_size;
  if (IsBreakInside(BreakToken())) {
    const auto* table_row_data =
        To<NGTableRowBreakTokenData>(BreakToken()->TokenData());
    previous_consumed_row_block_size =
        table_row_data->previous_consumed_row_block_size;
  }

  // The total block-size of the row is (at a minimum) the size which we
  // calculated while defining the table-grid, but also allowing for any
  // expansion due to fragmentation.
  LayoutUnit row_block_size =
      max_cell_block_size + previous_consumed_row_block_size;
  row_block_size = std::max(row_block_size, row.block_size);

  if (has_block_fragmentation) {
    // If we've expanded due to fragmentation, relayout with the new block-size.
    if (row.block_size != row_block_size) {
      NGLayoutResult::EStatus status =
          PlaceCells(row_block_size, absl::nullopt);
      if (status == NGLayoutResult::kOutOfFragmentainerSpace)
        return container_builder_.Abort(status);
      DCHECK_EQ(status, NGLayoutResult::kSuccess);
    }

    for (auto& result : results)
      container_builder_.AddResult(*result.result, result.offset);
  }

  // Since we always visit all cells in a row (cannot break halfway through;
  // each cell establishes a parallel flows that needs to be examined
  // separately), we have seen all children by now.
  container_builder_.SetHasSeenAllChildren();

  container_builder_.SetIsKnownToFitInFragmentainer(!has_inflow_break_inside);
  container_builder_.SetIntrinsicBlockSize(max_cell_block_size);
  container_builder_.SetFragmentsTotalBlockSize(row_block_size);
  if (row.is_collapsed)
    container_builder_.SetIsHiddenForPaint(true);
  container_builder_.SetIsTableNGPart();

  if (should_propagate_child_break_values) {
    container_builder_.SetInitialBreakBefore(row_break_before);
    container_builder_.SetPreviousBreakAfter(row_break_after);
  }

  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    NGBreakStatus status = FinishFragmentation(
        Node(), ConstraintSpace(), /* trailing_border_padding */ LayoutUnit(),
        FragmentainerSpaceLeft(ConstraintSpace()), &container_builder_);

    // TODO(mstensho): Deal with early-breaks.
    DCHECK_EQ(status, NGBreakStatus::kContinue);

    container_builder_.SetBreakTokenData(
        MakeGarbageCollected<NGTableRowBreakTokenData>(
            container_builder_.GetBreakTokenData(),
            previous_consumed_row_block_size +
                container_builder_.FragmentBlockSize()));
  }

  // NOTE: When we support "align-content: last baseline" for tables there may
  // be two baseline alignment contexts.
  container_builder_.SetBaselines(row_baseline_tabulator.ComputeBaseline(
      container_builder_.FragmentBlockSize()));

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();
  return container_builder_.ToBoxFragment();
}

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::ResultWithOffset)
