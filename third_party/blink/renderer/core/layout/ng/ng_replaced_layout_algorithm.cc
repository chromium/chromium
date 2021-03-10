// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_replaced_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"

namespace blink {

NGReplacedLayoutAlgorithm::NGReplacedLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      // TODO(dgrogan): Use something from NGLayoutInputNode instead of
      // accessing LayoutBox directly.
      natural_size_(PhysicalSize(Node().GetLayoutBox()->IntrinsicSize())
                        .ConvertToLogical(Style().GetWritingMode())) {}

scoped_refptr<const NGLayoutResult> NGReplacedLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken() || BreakToken()->IsBreakBefore());
  // Set this as a legacy root so that legacy painters are used.
  container_builder_.SetIsLegacyLayoutRoot();

  // TODO(dgrogan): |natural_size_.block_size| is frequently not the correct
  // intrinsic block size. Move |ComputeIntrinsicBlockSizeForAspectRatioElement|
  // from NGFlexLayoutAlgorithm to ng_length_utils and call it here.
  LayoutUnit intrinsic_block_size = natural_size_.block_size;
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size +
                                           BorderPadding().BlockSum());

  return container_builder_.ToBoxFragment();
}

// TODO(dgrogan): |natural_size_.inline_size| is frequently not the correct
// intrinsic inline size. Move NGFlexLayoutAlgorithm's
// |ComputeIntrinsicInlineSizeForAspectRatioElement| to here.
MinMaxSizesResult NGReplacedLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& child_input) const {
  MinMaxSizes sizes({natural_size_.inline_size, natural_size_.inline_size});
  sizes += BorderScrollbarPadding().InlineSum();
  return {sizes, false};
}

}  // namespace blink
