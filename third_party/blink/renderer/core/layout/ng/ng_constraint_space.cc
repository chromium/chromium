// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell_interface.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_interface.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsNGConstraintSpace {
  LogicalSize available_size;
  union {
    NGBfcOffset bfc_offset;
    void* rare_data;
  };
  NGExclusionSpace exclusion_space;
  unsigned bitfields[1];
};

ASSERT_SIZE(NGConstraintSpace, SameSizeAsNGConstraintSpace);

}  // namespace

NGConstraintSpace NGConstraintSpace::CreateFromLayoutObject(
    const LayoutBlock& block) {
  // We should only ever create a constraint space from legacy layout if the
  // object is a new formatting context.
  DCHECK(block.CreatesNewFormattingContext());
  DCHECK(!block.IsTableCell());

  const LayoutBlock* cb = block.ContainingBlock();
  LogicalSize available_size;
  bool is_fixed_inline_size = false;
  bool is_fixed_block_size = false;
  if (cb) {
    available_size.inline_size =
        LayoutBoxUtils::AvailableLogicalWidth(block, cb);
    available_size.block_size =
        LayoutBoxUtils::AvailableLogicalHeight(block, cb);
  } else {
    DCHECK(block.IsLayoutView());
    available_size = To<LayoutView>(block).InitialContainingBlockSize();
    is_fixed_inline_size = true;
    is_fixed_block_size = true;
  }

  LogicalSize percentage_size = available_size;

  bool is_initial_block_size_definite = true;
  if (block.HasOverrideLogicalWidth()) {
    available_size.inline_size = block.OverrideLogicalWidth();
    is_fixed_inline_size = true;
  }
  if (block.HasOverrideLogicalHeight()) {
    available_size.block_size = block.OverrideLogicalHeight();
    is_fixed_block_size = true;
  }
  if (block.IsFlexItem() && is_fixed_block_size) {
    // The flexbox-specific behavior is in addition to regular definite-ness, so
    // if the flex item would normally have a definite height it should keep it.
    is_initial_block_size_definite =
        To<LayoutFlexibleBox>(block.Parent())
            ->UseOverrideLogicalHeightForPerentageResolution(block) ||
        block.HasDefiniteLogicalHeight();
  }

  const ComputedStyle& style = block.StyleRef();
  const auto writing_mode = style.GetWritingMode();
  bool parallel_containing_block = IsParallelWritingMode(
      cb ? cb->StyleRef().GetWritingMode() : writing_mode, writing_mode);
  NGConstraintSpaceBuilder builder(writing_mode, style.GetWritingDirection(),
                                   /* is_new_fc */ true,
                                   !parallel_containing_block);

  if (!block.IsWritingModeRoot() || block.IsGridItem()) {
    // We don't know if the parent layout will require our baseline, so always
    // request it.
    builder.SetBaselineAlgorithmType(block.IsInline() &&
                                             block.IsAtomicInlineLevel()
                                         ? NGBaselineAlgorithmType::kInlineBlock
                                         : NGBaselineAlgorithmType::kFirstLine);
  }

  if (block.IsAtomicInlineLevel() || block.IsFlexItem() || block.IsGridItem() ||
      block.IsFloating())
    builder.SetIsPaintedAtomically(true);

  builder.SetAvailableSize(available_size);
  builder.SetPercentageResolutionSize(percentage_size);
  builder.SetIsFixedInlineSize(is_fixed_inline_size);
  builder.SetIsFixedBlockSize(is_fixed_block_size);
  builder.SetIsInitialBlockSizeIndefinite(!is_initial_block_size_definite);
  // HTML element with display:table is shrink-to-fit.
  bool shrink_to_fit =
      block.SizesLogicalWidthToFitContent(style.LogicalWidth()) ||
      (block.IsTable() && block.Parent() && block.Parent()->IsLayoutView());
  builder.SetInlineAutoBehavior(shrink_to_fit
                                    ? NGAutoBehavior::kFitContent
                                    : NGAutoBehavior::kStretchImplicit);
  return builder.ToConstraintSpace();
}

String NGConstraintSpace::ToString() const {
  return String::Format("Offset: %s,%s Size: %sx%s Clearance: %s",
                        BfcOffset().line_offset.ToString().Ascii().c_str(),
                        BfcOffset().block_offset.ToString().Ascii().c_str(),
                        AvailableSize().inline_size.ToString().Ascii().c_str(),
                        AvailableSize().block_size.ToString().Ascii().c_str(),
                        HasClearanceOffset()
                            ? ClearanceOffset().ToString().Ascii().c_str()
                            : "none");
}

}  // namespace blink
