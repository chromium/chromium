// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/masonry/masonry_node.h"

namespace blink {

class GridSizingTrackCollection;

class CORE_EXPORT MasonryLayoutAlgorithm
    : public LayoutAlgorithm<MasonryNode, BoxFragmentBuilder, BlockBreakToken> {
 public:
  explicit MasonryLayoutAlgorithm(const LayoutAlgorithmParams& params);

  const LayoutResult* Layout();
  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&);

 private:
  friend class MasonryLayoutAlgorithmTest;

  GridSizingTrackCollection ComputeCrossAxisTrackSizes() const;

  wtf_size_t ComputeAutomaticRepetitions() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_LAYOUT_ALGORITHM_H_
