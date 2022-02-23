// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_section_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGTableSectionLayoutAlgorithm::NGTableSectionLayoutAlgorithm(
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
const NGLayoutResult* NGTableSectionLayoutAlgorithm::Layout() {
  const NGTableConstraintSpaceData& table_data = *ConstraintSpace().TableData();
  wtf_size_t section_index = ConstraintSpace().TableSectionIndex();

  absl::optional<LayoutUnit> section_baseline;

  const LogicalSize available_size = {container_builder_.InlineSize(),
                                      kIndefiniteSize};
  LogicalOffset offset;
  bool is_first_row = true;
  const wtf_size_t start_row_index =
      table_data.sections[section_index].start_row_index;
  NGBlockChildIterator child_iterator(Node().FirstChild(), BreakToken(),
                                      /* calculate_child_idx */ true);
  for (auto entry = child_iterator.NextChild();
       NGBlockNode row = To<NGBlockNode>(entry.node);
       entry = child_iterator.NextChild()) {
    const auto* row_break_token = To<NGBlockBreakToken>(entry.token);
    wtf_size_t row_index = start_row_index + *entry.index;
    DCHECK_LT(row_index, table_data.sections[section_index].start_row_index +
                             table_data.sections[section_index].row_count);

    if (!is_first_row && !table_data.rows[row_index].is_collapsed)
      offset.block_offset += table_data.table_border_spacing.block_size;

    NGConstraintSpaceBuilder row_space_builder(
        table_data.table_writing_direction.GetWritingMode(),
        table_data.table_writing_direction,
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

    LayoutUnit previously_consumed_row_block_size;
    if (ConstraintSpace().HasBlockFragmentation()) {
      if (row_break_token) {
        previously_consumed_row_block_size =
            row_break_token->ConsumedBlockSize();
      }
      LayoutUnit fragmentainer_block_offset =
          ConstraintSpace().FragmentainerOffsetAtBfc() + offset.block_offset;
      NGBreakStatus break_status = BreakBeforeChildIfNeeded(
          ConstraintSpace(), row, *row_result, fragmentainer_block_offset,
          !is_first_row, &container_builder_);
      if (break_status != NGBreakStatus::kContinue)
        break;
    }

    if (is_first_row) {
      const NGPhysicalBoxFragment& physical_fragment =
          To<NGPhysicalBoxFragment>(row_result->PhysicalFragment());
      DCHECK(physical_fragment.Baseline());
      section_baseline = physical_fragment.Baseline();
    }
    container_builder_.AddResult(*row_result, offset);
    offset.block_offset += table_data.rows[row_index].block_size -
                           previously_consumed_row_block_size;
    is_first_row = false;

    if (container_builder_.HasInflowChildBreakInside())
      break;
  }

  if (!child_iterator.NextChild().node)
    container_builder_.SetHasSeenAllChildren();

  LayoutUnit block_size;
  if (ConstraintSpace().IsFixedBlockSize()) {
    // A fixed block-size should only occur for a section without children.
    DCHECK_EQ(table_data.sections[section_index].row_count, 0u);
    block_size = ConstraintSpace().AvailableSize().block_size;
  } else {
    block_size = offset.block_offset;
    if (BreakToken())
      block_size += BreakToken()->ConsumedBlockSize();
  }
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  if (section_baseline)
    container_builder_.SetBaseline(*section_baseline);
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
