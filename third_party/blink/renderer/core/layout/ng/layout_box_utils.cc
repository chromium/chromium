// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

LayoutUnit LayoutBoxUtils::AvailableLogicalHeight(const LayoutBox& box,
                                                  const LayoutBlock* cb) {
  // SVG <text> and <foreignObject> should not refer to its containing block.
  DCHECK(!box.IsSVGChild());
  auto writing_mode = box.StyleRef().GetWritingMode();
  bool parallel_containing_block = IsParallelWritingMode(
      cb ? cb->StyleRef().GetWritingMode() : writing_mode, writing_mode);

  // Grid layout sets OverrideContainingBlockContentLogicalWidth.
  if (!parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalWidth())
    return box.OverrideContainingBlockContentLogicalWidth();

  if (!box.Parent())
    return box.View()->ViewLogicalHeightForPercentages();

  DCHECK(cb);
  if (parallel_containing_block)
    return box.ContainingBlockLogicalHeightForPercentageResolution();

  return box.ContainingBlockLogicalWidthForContent();
}

bool LayoutBoxUtils::SkipContainingBlockForPercentHeightCalculation(
    const LayoutBlock* cb) {
  return LayoutBox::SkipContainingBlockForPercentHeightCalculation(cb);
}

LayoutUnit LayoutBoxUtils::InlineSize(const LayoutBox& box) {
  DCHECK_GT(box.PhysicalFragmentCount(), 0u);

  // TODO(almaher): We can't assume all fragments will have the same inline
  // size.
  return box.GetPhysicalFragment(0u)
      ->Size()
      .ConvertToLogical(box.StyleRef().GetWritingMode())
      .inline_size;
}

LayoutUnit LayoutBoxUtils::TotalBlockSize(const LayoutBox& box) {
  wtf_size_t num_fragments = box.PhysicalFragmentCount();
  DCHECK_GT(num_fragments, 0u);

  // Calculate the total block size by looking at the last two block fragments
  // with a non-zero block-size.
  LayoutUnit total_block_size;
  while (num_fragments > 0) {
    LayoutUnit block_size =
        box.GetPhysicalFragment(num_fragments - 1)
            ->Size()
            .ConvertToLogical(box.StyleRef().GetWritingMode())
            .block_size;
    if (block_size > LayoutUnit()) {
      total_block_size += block_size;
      break;
    }
    num_fragments--;
  }

  if (num_fragments > 1) {
    total_block_size += box.GetPhysicalFragment(num_fragments - 2)
                            ->BreakToken()
                            ->ConsumedBlockSize();
  }
  return total_block_size;
}

// static
LayoutPoint LayoutBoxUtils::ComputeLocation(
    const NGPhysicalBoxFragment& child_fragment,
    PhysicalOffset offset,
    const NGPhysicalBoxFragment& container_fragment,
    const NGBlockBreakToken* previous_container_break_token) {
  if (UNLIKELY(container_fragment.Style().IsFlippedBlocksWritingMode())) {
    // Move the physical offset to the right side of the child fragment,
    // relative to the right edge of the container fragment. This is the
    // block-start offset in vertical-rl, and the legacy engine expects always
    // expects the block offset to be relative to block-start.
    offset.left = container_fragment.Size().width - offset.left -
                  child_fragment.Size().width;
  }

  if (UNLIKELY(previous_container_break_token)) {
    // Add the amount of block-size previously (in previous fragmentainers)
    // consumed by the container fragment. This will map the child's offset
    // nicely into the flow thread coordinate system used by the legacy engine.
    LayoutUnit consumed =
        previous_container_break_token->ConsumedBlockSizeForLegacy();
    if (container_fragment.Style().IsHorizontalWritingMode()) {
      offset.top += consumed;
    } else {
      offset.left += consumed;
    }
  }

  return offset.ToLayoutPoint();
}

}  // namespace blink
