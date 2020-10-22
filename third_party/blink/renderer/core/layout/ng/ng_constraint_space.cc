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

  const LayoutBlock* cb = block.ContainingBlock();
  LayoutUnit available_logical_width =
      LayoutBoxUtils::AvailableLogicalWidth(block, cb);
  LayoutUnit available_logical_height =
      LayoutBoxUtils::AvailableLogicalHeight(block, cb);
  LogicalSize percentage_size = {available_logical_width,
                                 available_logical_height};
  LogicalSize available_size = percentage_size;

  bool fixed_inline = false, fixed_block = false;
  bool fixed_block_is_definite = true;
  if (block.HasOverrideLogicalWidth()) {
    available_size.inline_size = block.OverrideLogicalWidth();
    fixed_inline = true;
  }
  if (block.HasOverrideLogicalHeight()) {
    available_size.block_size = block.OverrideLogicalHeight();
    fixed_block = true;
  }
  if (block.IsFlexItem() && fixed_block) {
    // The flexbox-specific behavior is in addition to regular definite-ness, so
    // if the flex item would normally have a definite height it should keep it.
    fixed_block_is_definite =
        ToLayoutFlexibleBox(block.Parent())
            ->UseOverrideLogicalHeightForPerentageResolution(block) ||
        block.HasDefiniteLogicalHeight();
  }

  const ComputedStyle& style = block.StyleRef();
  auto writing_mode = style.GetWritingMode();
  bool parallel_containing_block = IsParallelWritingMode(
      cb ? cb->StyleRef().GetWritingMode() : writing_mode, writing_mode);
  NGConstraintSpaceBuilder builder(writing_mode, writing_mode,
                                   /* is_new_fc */ true,
                                   !parallel_containing_block);

  if (!block.IsWritingModeRoot() || block.IsGridItem()) {
    // We don't know if the parent layout will require our baseline, so always
    // request it.
    builder.SetNeedsBaseline(true);
    builder.SetBaselineAlgorithmType(block.IsInline() &&
                                             block.IsAtomicInlineLevel()
                                         ? NGBaselineAlgorithmType::kInlineBlock
                                         : NGBaselineAlgorithmType::kFirstLine);
  }

  if (block.IsTableCell()) {
    const LayoutNGTableCellInterface& cell =
        ToInterface<LayoutNGTableCellInterface>(block);
    const ComputedStyle& cell_style = cell.ToLayoutObject()->StyleRef();
    const ComputedStyle& table_style =
        cell.TableInterface()->ToLayoutObject()->StyleRef();
    DCHECK(block.IsTableCellLegacy());
    builder.SetIsTableCell(true, /* is_table_cell_legacy */ true);
    builder.SetIsRestrictedBlockSizeTableCell(
        !cell_style.LogicalHeight().IsAuto() ||
        !table_style.LogicalHeight().IsAuto());
    const LayoutBlock& cell_block = To<LayoutBlock>(*cell.ToLayoutObject());
    if (RuntimeEnabledFeatures::TableCellNewPercentsEnabled() && fixed_block) {
      fixed_block_is_definite = cell_block.HasDefiniteLogicalHeight() ||
                                !table_style.LogicalHeight().IsAuto();
    }
    builder.SetTableCellBorders(
        {cell_block.BorderStart(), cell_block.BorderEnd(),
         cell_block.BorderBefore(), cell_block.BorderAfter()});
    builder.SetTableCellIntrinsicPadding(
        {LayoutUnit(), LayoutUnit(), LayoutUnit(cell.IntrinsicPaddingBefore()),
         LayoutUnit(cell.IntrinsicPaddingAfter())});
    builder.SetHideTableCellIfEmpty(
        cell_style.EmptyCells() == EEmptyCells::kHide &&
        table_style.BorderCollapse() == EBorderCollapse::kSeparate);
    builder.SetHasTableCellCollapsedBorder(
        cell_block.Parent()->Parent()->Parent()->StyleRef().BorderCollapse() ==
        EBorderCollapse::kCollapse);
  }

  if (block.IsAtomicInlineLevel() || block.IsFlexItem() || block.IsGridItem() ||
      block.IsFloating())
    builder.SetIsPaintedAtomically(true);

  builder.SetAvailableSize(available_size);
  builder.SetPercentageResolutionSize(percentage_size);
  builder.SetIsFixedInlineSize(fixed_inline);
  builder.SetIsFixedBlockSize(fixed_block);
  builder.SetIsFixedBlockSizeIndefinite(!fixed_block_is_definite);
  builder.SetIsShrinkToFit(
      style.LogicalWidth().IsAuto() &&
      block.SizesLogicalWidthToFitContent(style.LogicalWidth()));
  builder.SetTextDirection(style.Direction());
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
