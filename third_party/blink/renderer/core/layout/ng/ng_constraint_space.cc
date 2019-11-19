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

static_assert(sizeof(NGConstraintSpace) == sizeof(SameSizeAsNGConstraintSpace),
              "NGConstraintSpace should stay small.");

}  // namespace

NGConstraintSpace NGConstraintSpace::CreateFromLayoutObject(
    const LayoutBlock& block,
    bool is_layout_root) {
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

  auto* previous_result = block.GetCachedLayoutResult();
  if (is_layout_root && previous_result) {
    // Due to layout-roots (starting layout at an arbirary node, instead of the
    // |LayoutView|), we can end up with a situation where we'll miss our cache
    // due to baseline-requests not matching.
    //
    // For the case where we start at a layout-root, the baselines don't
    // particularly matter, so we just request exactly the same as the previous
    // layout.
    builder.AddBaselineRequests(
        previous_result->GetConstraintSpaceForCaching().BaselineRequests());
  } else if (!block.IsWritingModeRoot() || block.IsGridItem()) {
    // Add all types because we don't know which baselines will be requested.
    FontBaseline baseline_type = style.GetFontBaseline();
    bool synthesize_inline_block_baseline =
        block.UseLogicalBottomMarginEdgeForInlineBlockBaseline();
    if (!synthesize_inline_block_baseline) {
      builder.AddBaselineRequest(
          {NGBaselineAlgorithmType::kAtomicInline, baseline_type});
    }
    builder.AddBaselineRequest(
        {NGBaselineAlgorithmType::kFirstLine, baseline_type});
  }

  if (block.IsTableCell()) {
    const LayoutNGTableCellInterface& cell =
        ToInterface<LayoutNGTableCellInterface>(block);
    const ComputedStyle& cell_style = cell.ToLayoutObject()->StyleRef();
    const ComputedStyle& table_style =
        cell.TableInterface()->ToLayoutObject()->StyleRef();
    builder.SetIsTableCell(true);
    builder.SetIsRestrictedBlockSizeTableCell(
        !cell_style.LogicalHeight().IsAuto() ||
        !table_style.LogicalHeight().IsAuto());
    const LayoutBlock& cell_block = To<LayoutBlock>(*cell.ToLayoutObject());
    builder.SetTableCellBorders(
        {cell_block.BorderStart(), cell_block.BorderEnd(),
         cell_block.BorderBefore(), cell_block.BorderAfter()});
    builder.SetTableCellIntrinsicPadding(
        {LayoutUnit(), LayoutUnit(), LayoutUnit(cell.IntrinsicPaddingBefore()),
         LayoutUnit(cell.IntrinsicPaddingAfter())});
    builder.SetHideTableCellIfEmpty(
        cell_style.EmptyCells() == EEmptyCells::kHide &&
        table_style.BorderCollapse() == EBorderCollapse::kSeparate);
  }

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
                        bfc_offset_.line_offset.ToString().Ascii().c_str(),
                        bfc_offset_.block_offset.ToString().Ascii().c_str(),
                        AvailableSize().inline_size.ToString().Ascii().c_str(),
                        AvailableSize().block_size.ToString().Ascii().c_str(),
                        HasClearanceOffset()
                            ? ClearanceOffset().ToString().Ascii().c_str()
                            : "none");
}

}  // namespace blink
