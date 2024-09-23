// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"

#include "third_party/blink/renderer/core/layout/constraint_space.h"

namespace blink {

namespace {

ConstraintSpace::PercentageStorage GetPercentageStorage(
    LayoutUnit percentage_size,
    LayoutUnit available_size) {
  if (percentage_size == available_size)
    return ConstraintSpace::kSameAsAvailable;

  if (percentage_size == kIndefiniteSize)
    return ConstraintSpace::kIndefinite;

  if (percentage_size == LayoutUnit())
    return ConstraintSpace::kZero;

  return ConstraintSpace::kRareDataPercentage;
}

}  // namespace

void ConstraintSpaceBuilder::SetPercentageResolutionSize(
    LogicalSize percentage_resolution_size) {
#if DCHECK_IS_ON()
  DCHECK(is_available_size_set_);
  is_percentage_resolution_size_set_ = true;
#endif
  if (is_in_parallel_flow_) [[likely]] {
    space_.bitfields_.percentage_inline_storage =
        GetPercentageStorage(percentage_resolution_size.inline_size,
                             space_.available_size_.inline_size);
    if (space_.bitfields_.percentage_inline_storage ==
        ConstraintSpace::kRareDataPercentage) [[unlikely]] {
      space_.EnsureRareData()->percentage_resolution_size.inline_size =
          percentage_resolution_size.inline_size;
    }

    space_.bitfields_.percentage_block_storage =
        GetPercentageStorage(percentage_resolution_size.block_size,
                             space_.available_size_.block_size);
    if (space_.bitfields_.percentage_block_storage ==
        ConstraintSpace::kRareDataPercentage) {
      space_.EnsureRareData()->percentage_resolution_size.block_size =
          percentage_resolution_size.block_size;
    }
  } else {
    if (adjust_inline_size_if_needed_)
      AdjustInlineSizeIfNeeded(&percentage_resolution_size.block_size);

    space_.bitfields_.percentage_inline_storage =
        GetPercentageStorage(percentage_resolution_size.block_size,
                             space_.available_size_.inline_size);
    if (space_.bitfields_.percentage_inline_storage ==
        ConstraintSpace::kRareDataPercentage) {
      space_.EnsureRareData()->percentage_resolution_size.inline_size =
          percentage_resolution_size.block_size;
    }

    space_.bitfields_.percentage_block_storage =
        GetPercentageStorage(percentage_resolution_size.inline_size,
                             space_.available_size_.block_size);
    if (space_.bitfields_.percentage_block_storage ==
        ConstraintSpace::kRareDataPercentage) {
      space_.EnsureRareData()->percentage_resolution_size.block_size =
          percentage_resolution_size.inline_size;
    }
  }
}

void ConstraintSpaceBuilder::SetReplacedPercentageResolutionSize(
    LogicalSize replaced_percentage_resolution_size) {
#if DCHECK_IS_ON()
  DCHECK(is_available_size_set_);
  DCHECK(is_percentage_resolution_size_set_);
#endif
  if (is_in_parallel_flow_) [[likely]] {
    // We don't store the replaced percentage resolution inline size, so we need
    // it to be the same as the regular percentage resolution inline size.
    DCHECK_EQ(replaced_percentage_resolution_size.inline_size,
              space_.PercentageResolutionInlineSize());

    space_.bitfields_.replaced_percentage_block_storage =
        GetPercentageStorage(replaced_percentage_resolution_size.block_size,
                             space_.available_size_.block_size);
    if (space_.bitfields_.replaced_percentage_block_storage ==
        ConstraintSpace::kRareDataPercentage) {
      space_.EnsureRareData()->replaced_percentage_resolution_block_size =
          replaced_percentage_resolution_size.block_size;
    }
  } else {
    // There should be no need to handle quirky percentage block-size resolution
    // if this is an orthogonal writing mode root. The quirky percentage
    // block-size resolution size that may have been calculated on an ancestor
    // will be used to resolve inline-sizes of the child, and will therefore now
    // be lost (since we don't store the quirky replaced percentage resolution
    // *inline* size, only the *block* size). Just copy whatever was set as a
    // regular percentage resolution block-size.
    LayoutUnit block_size = space_.PercentageResolutionBlockSize();

    space_.bitfields_.replaced_percentage_block_storage =
        GetPercentageStorage(block_size, space_.available_size_.block_size);
    if (space_.bitfields_.replaced_percentage_block_storage ==
        ConstraintSpace::kRareDataPercentage) {
      space_.EnsureRareData()->replaced_percentage_resolution_block_size =
          block_size;
    }
  }
}

}  // namespace blink
