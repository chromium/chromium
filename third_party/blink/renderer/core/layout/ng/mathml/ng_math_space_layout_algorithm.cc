// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_space_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

namespace blink {

NGMathSpaceLayoutAlgorithm::NGMathSpaceLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

const NGLayoutResult* NGMathSpaceLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());

  LayoutUnit intrinsic_block_size = BorderScrollbarPadding().BlockSum();
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(), intrinsic_block_size,
      container_builder_.InitialBorderBoxSize().inline_size);

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  container_builder_.SetBaselines(
      BorderScrollbarPadding().block_start +
      ValueForLength(Style().GetMathBaseline(), LayoutUnit()));
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGMathSpaceLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  sizes += BorderScrollbarPadding().InlineSum();
  return MinMaxSizesResult(sizes, /* depends_on_block_constraints */ false);
}

}  // namespace blink
