// Copyright 2016 The Chromium Authors
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
  DCHECK(!block.IsTableCell());

  const ComputedStyle& style = block.StyleRef();
  const auto writing_mode = style.GetWritingMode();
  bool adjust_inline_size_if_needed = false;

  LogicalSize available_size;
  bool is_fixed_inline_size = false;
  bool is_fixed_block_size = false;
  if (block.IsSVGChild()) {
    // SVG <text> and <foreignObject> should not refer to its containing block.
  } else if (const LayoutBlock* cb = block.ContainingBlock()) {
    available_size.inline_size =
        LayoutBoxUtils::AvailableLogicalWidth(block, cb);
    available_size.block_size =
        LayoutBoxUtils::AvailableLogicalHeight(block, cb);
    adjust_inline_size_if_needed =
        !IsParallelWritingMode(cb->StyleRef().GetWritingMode(), writing_mode);
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

  // We cannot enter NG layout at an object that isn't a formatting context
  // root. However, even though we're creating a constraint space for an object
  // here, that doesn't have to mean that we're going to lay it out. For
  // instance, if we're laying out an out-of-flow positioned NG object contained
  // by a legacy object, |block| here will be the container of the OOF, not the
  // OOF itself. It's perfectly fine if that one isn't a formatting context
  // root, since it's being laid out by the legacy engine anyway. As for the OOF
  // that we're actually going to lay out, it will always establish a new
  // formatting context, since it's out-of-flow.
  bool is_new_fc = block.CreatesNewFormattingContext();
  NGConstraintSpaceBuilder builder(writing_mode, style.GetWritingDirection(),
                                   is_new_fc, adjust_inline_size_if_needed);

  if (!block.IsWritingModeRoot() || block.IsGridItem()) {
    // We don't know if the parent layout will require our baseline, so always
    // request it.
    builder.SetBaselineAlgorithmType(block.IsInline() &&
                                             block.IsAtomicInlineLevel()
                                         ? NGBaselineAlgorithmType::kInlineBlock
                                         : NGBaselineAlgorithmType::kDefault);
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
