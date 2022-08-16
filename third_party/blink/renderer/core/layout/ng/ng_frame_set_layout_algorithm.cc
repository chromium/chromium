// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_frame_set_layout_algorithm.h"

namespace blink {

NGFrameSetLayoutAlgorithm::NGFrameSetLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm<NGBlockNode, NGBoxFragmentBuilder, NGBlockBreakToken>(
          params) {}

const NGLayoutResult* NGFrameSetLayoutAlgorithm::Layout() {
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGFrameSetLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  return MinMaxSizesResult(MinMaxSizes(), false);
}

}  // namespace blink
