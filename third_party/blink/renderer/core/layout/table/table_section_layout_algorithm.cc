// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/table_section_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

TableSectionLayoutAlgorithm::TableSectionLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {}

// Generated fragment structure:
// +-----section--------------+
// |       vspacing           |
// |  +--------------------+  |
// |  |      row           |  |
// |  +--------------------+  |
// |       vspacing           |
// |  +--------------------+  |
// |  |      row           |  |
// |  +--------------------+  |
// |       vspacing           |
// +--------------------------+
const NGLayoutResult* TableSectionLayoutAlgorithm::Layout() {
  const TableConstraintSpaceData& table_data = *ConstraintSpace().TableData();
  const auto& section =
      table_data.sections[ConstraintSpace().TableSectionIndex()];
  const wtf_size_t start_row_index = section.start_row_index;
  const LogicalSize available_size = {container_builder_.InlineSize(),
                                      kIndefiniteSize};

  absl::optional<LayoutUnit> first_baseline;
  absl::optional<LayoutUnit> last_baseline;
  LogicalOffset offset;
  LayoutUnit intrinsic_block_size;
  bool is_first_non_collapsed_row = true;

  Vector<LayoutUnit> row_offsets = {LayoutUnit()};
  wtf_size_t actual_start_row_index = 0u;

  NGBlockChildIterator child_iterator(Node().FirstChild(), BreakToken(),
                                      /* calculate_child_idx */ true);
  for (auto entry = child_iterator.NextChild();
       NGBlockNode row = To<NGBlockNode>(entry.node);
       entry = child_iterator.NextChild()) {
    const auto* row_break_token = To<NGBlockBreakToken>(entry.token);
    wtf_size_t row_index = start_row_index + *entry.index;
    DCHECK_LT(row_index, start_row_index + section.row_count);
    bool is_row_collapsed = table_data.rows[row_index].is_collapsed;

    if (UNLIKELY(early_break_ &&
                 IsEarlyBreakTarget(*early_break_, container_builder_, row))) {
      container_builder_.AddBreakBeforeChild(row, kBreakAppealPerfect,
                                             /* is_forced_break */ false);
      break;
    }

    if (!is_first_non_collapsed_row && !is_row_collapsed)
      offset.block_offset += table_data.table_border_spacing.block_size;

    DCHECK_EQ(table_data.table_writing_direction.GetWritingMode(),
              ConstraintSpace().GetWritingMode());

    NGConstraintSpaceBuilder row_space_builder(
        ConstraintSpace(), table_data.table_writing_direction,
        /* is_new_fc */ true);
    row_space_builder.SetAvailableSize(available_size);
    row_space_builder.SetPercentageResolutionSize(available_size);
    row_space_builder.SetIsFixedInlineSize(true);
    row_space_builder.SetTableRowData(&table_data, row_index);

    if (ConstraintSpace().HasBlockFragmentation()) {
      SetupSpaceBuilderForFragmentation(
          ConstraintSpace(), row, offset.block_offset, &row_space_builder,
          /* is_new_fc */ true,
          container_builder_.RequiresContentBeforeBreaking());
    }

    NGConstraintSpace row_space = row_space_builder.ToConstraintSpace();
    const NGLayoutResult* row_result = row.Layout(row_space, row_break_token);

    if (ConstraintSpace().HasBlockFragmentation()) {
      LayoutUnit fragmentainer_block_offset =
          ConstraintSpace().FragmentainerOffset() + offset.block_offset;
      NGBreakStatus break_status = BreakBeforeChildIfNeeded(
          ConstraintSpace(), row, *row_result, fragmentainer_block_offset,
          !is_first_non_collapsed_row, &container_builder_);
      if (break_status == NGBreakStatus::kNeedsEarlierBreak) {
        return RelayoutAndBreakEarlier<TableSectionLayoutAlgorithm>(
            container_builder_.EarlyBreak());
      }
      if (break_status == NGBreakStatus::kBrokeBefore)
        break;
      DCHECK_EQ(break_status, NGBreakStatus::kContinue);
    }

    const auto& physical_fragment =
        To<NGPhysicalBoxFragment>(row_result->PhysicalFragment());
    const NGBoxFragment fragment(table_data.table_writing_direction,
                                 physical_fragment);

    // TODO(crbug.com/736093): Due to inconsistent writing-direction of
    // table-parts these DCHECKs may fail. When the above bug is fixed use the
    // logical fragment instead of the physical.
    DCHECK(fragment.FirstBaseline());
    DCHECK(fragment.LastBaseline());
    if (!first_baseline)
      first_baseline = offset.block_offset + *physical_fragment.FirstBaseline();
    last_baseline = offset.block_offset + *physical_fragment.LastBaseline();

    container_builder_.AddResult(*row_result, offset);
    offset.block_offset += fragment.BlockSize();
    is_first_non_collapsed_row &= is_row_collapsed;

    if (table_data.has_collapsed_borders &&
        (!row_break_token || !row_break_token->IsAtBlockEnd())) {
      // Determine the start row-index for this section.
      if (row_offsets.size() == 1u)
        actual_start_row_index = row_index;
      row_offsets.emplace_back(offset.block_offset);
    }
    intrinsic_block_size = offset.block_offset;
  }

  if (!child_iterator.NextChild().node)
    container_builder_.SetHasSeenAllChildren();

  LayoutUnit block_size;
  if (ConstraintSpace().IsFixedBlockSize()) {
    // A fixed block-size should only occur for a section without children.
    DCHECK_EQ(section.row_count, 0u);
    block_size = ConstraintSpace().AvailableSize().block_size;
  } else {
    block_size = offset.block_offset;
    if (BreakToken())
      block_size += BreakToken()->ConsumedBlockSize();
  }
  container_builder_.SetFragmentsTotalBlockSize(block_size);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);

  if (first_baseline)
    container_builder_.SetFirstBaseline(*first_baseline);
  if (last_baseline)
    container_builder_.SetLastBaseline(*last_baseline);
  container_builder_.SetIsTablePart();

  // Store the collapsed-borders row geometry on this section fragment.
  if (table_data.has_collapsed_borders && row_offsets.size() > 1u) {
    container_builder_.SetTableSectionCollapsedBordersGeometry(
        actual_start_row_index, std::move(row_offsets));
  }

  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    NGBreakStatus status = FinishFragmentation(
        Node(), ConstraintSpace(), /* trailing_border_padding */ LayoutUnit(),
        FragmentainerSpaceLeft(ConstraintSpace()), &container_builder_);
    DCHECK_EQ(status, NGBreakStatus::kContinue);
  }

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();
  return container_builder_.ToBoxFragment();
}

}  // namespace blink
