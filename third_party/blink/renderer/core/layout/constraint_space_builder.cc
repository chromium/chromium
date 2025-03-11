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

}  // namespace blink
