// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_space_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

namespace blink {

NGMathSpaceLayoutAlgorithm::NGMathSpaceLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.fragment_geometry.scrollbar.IsEmpty());
  DCHECK(params.space.IsNewFormattingContext());
}

scoped_refptr<const NGLayoutResult> NGMathSpaceLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());

  LayoutUnit intrinsic_block_size = BorderScrollbarPadding().BlockSum();
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(), intrinsic_block_size,
      container_builder_.InitialBorderBoxSize().inline_size);

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  container_builder_.SetBaseline(
      BorderScrollbarPadding().block_start +
      ValueForLength(Style().GetMathBaseline(), LayoutUnit()));
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGMathSpaceLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput&) {
  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  sizes += BorderScrollbarPadding().InlineSum();

  return {sizes, /* depends_on_percentage_block_size */ false};
}

}  // namespace blink
