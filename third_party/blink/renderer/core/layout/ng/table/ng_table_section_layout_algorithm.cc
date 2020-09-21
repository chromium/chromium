// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_section_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGTableSectionLayoutAlgorithm::NGTableSectionLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  container_builder_.SetIsNewFormattingContext(
      params.space.IsNewFormattingContext());
}

MinMaxSizesResult NGTableSectionLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput&) const {
  NOTREACHED();  // Table layout does not compute minmax for table row.
  return MinMaxSizesResult();
}

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
scoped_refptr<const NGLayoutResult> NGTableSectionLayoutAlgorithm::Layout() {
  const NGTableConstraintSpaceData& table_data = *ConstraintSpace().TableData();
  wtf_size_t section_index = ConstraintSpace().TableSectionIndex();

  base::Optional<LayoutUnit> section_baseline;

  LogicalOffset offset;
  bool is_first_row = true;
  wtf_size_t row_index = table_data.sections[section_index].start_row_index;
  for (NGBlockNode row = To<NGBlockNode>(Node().FirstChild()); row;
       row = To<NGBlockNode>(row.NextSibling())) {
    DCHECK_LT(row_index, table_data.sections[section_index].start_row_index +
                             table_data.sections[section_index].rowspan);
    NGConstraintSpaceBuilder row_space_builder(
        table_data.table_writing_direction.GetWritingMode(),
        table_data.table_writing_direction.GetWritingMode(),
        /* is_new_fc */ true);
    row_space_builder.SetTextDirection(row.Style().Direction());
    row_space_builder.SetAvailableSize(
        {container_builder_.InlineSize(), kIndefiniteSize});
    row_space_builder.SetIsFixedInlineSize(true);
    row_space_builder.SetPercentageResolutionSize(
        {container_builder_.InlineSize(), kIndefiniteSize});
    row_space_builder.SetNeedsBaseline(true);
    row_space_builder.SetTableRowData(&table_data, row_index);
    NGConstraintSpace row_space = row_space_builder.ToConstraintSpace();
    scoped_refptr<const NGLayoutResult> row_result = row.Layout(row_space);
    if (is_first_row) {
      const NGPhysicalBoxFragment& physical_fragment =
          To<NGPhysicalBoxFragment>(row_result->PhysicalFragment());
      DCHECK(physical_fragment.Baseline());
      section_baseline = physical_fragment.Baseline();
    } else if (!table_data.rows[row_index].is_collapsed) {
      offset.block_offset += table_data.table_border_spacing.block_size;
    }
    container_builder_.AddResult(*row_result, offset);
    offset.block_offset += table_data.rows[row_index].block_size;
    is_first_row = false;
    row_index++;
  }
  container_builder_.SetFragmentBlockSize(offset.block_offset);
  if (section_baseline)
    container_builder_.SetBaseline(*section_baseline);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();
  return container_builder_.ToBoxFragment();
}

}  // namespace blink
