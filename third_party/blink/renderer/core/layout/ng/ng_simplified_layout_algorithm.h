// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_SIMPLIFIED_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_SIMPLIFIED_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

namespace blink {

class NGBlockBreakToken;
struct NGLink;
class NGPhysicalContainerFragment;

// The "simplified" layout algorithm will run in the following circumstances:
//  - An OOF-positioned descendant of this node (this node is its containing
//    block) has its constraints changed.
//  - A child requires "simplified" layout, i.e. an indirect-descendant
//    OOF-positioned child has its constraints changed.
//  - The block-size of the fragment has changed, and we know that it won't
//    affect any inflow children (no %-block-size descendants).
//
// This algorithm effectively performs a (convoluted) "copy" of the previous
// layout result. It will:
//  1. Copy data from the previous |NGLayoutResult| into the
//     |NGBoxFragmentBuilder|, (e.g. flags, end margin strut, etc).
//  2. Iterate through all the children and:
//    a. If OOF-positioned determine the static-position and add it as an
//       OOF-positioned candidate.
//    b. Otherwise perform layout on the inflow child (which may trigger
//       "simplified" layout on its children).
//  3. Run the |NGOutOfFlowLayoutPart|.
class CORE_EXPORT NGSimplifiedLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  NGSimplifiedLayoutAlgorithm(const NGLayoutAlgorithmParams&,
                              const NGLayoutResult&);

  scoped_refptr<const NGLayoutResult> Layout() override;
  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const override {
    NOTREACHED();
    return {MinMaxSizes(), /* depends_on_percentage_block_size */ true};
  }

  NOINLINE scoped_refptr<const NGLayoutResult> LayoutWithItemsBuilder();

 private:
  void AddChildFragment(const NGLink& old_fragment,
                        const NGPhysicalContainerFragment& new_fragment);

  const NGLayoutResult& previous_result_;
  NGBoxStrut border_scrollbar_padding_;

  const WritingDirectionMode writing_direction_;

  PhysicalSize previous_physical_container_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_SIMPLIFIED_LAYOUT_ALGORITHM_H_
