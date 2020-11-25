// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_SIMPLIFIED_OOF_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_SIMPLIFIED_OOF_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

namespace blink {

struct NGLink;
class NGPhysicalContainerFragment;

// This is more a copy-and-append algorithm than a layout algorithm.
// This algorithm will only run when we are trying to add OOF-positioned
// elements to an already laid out fragmentainer. It performs a copy of the
// previous |NGPhysicalFragment| and appends the OOF-positioned elements to the
// |container_builder_|.
class CORE_EXPORT NGSimplifiedOOFLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  NGSimplifiedOOFLayoutAlgorithm(const NGLayoutAlgorithmParams&,
                                 const NGPhysicalBoxFragment&,
                                 bool is_new_fragment);

  scoped_refptr<const NGLayoutResult> Layout() override;
  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const override {
    NOTREACHED();
    return {MinMaxSizes(), /* depends_on_percentage_block_size */ true};
  }

  void AppendOutOfFlowResult(scoped_refptr<const NGLayoutResult> child,
                             LogicalOffset offset);

 private:
  void AddChildFragment(const NGLink& old_fragment,
                        const NGPhysicalContainerFragment& new_fragment);

  const WritingDirectionMode writing_direction_;
  PhysicalSize previous_physical_container_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_SIMPLIFIED_OOF_LAYOUT_ALGORITHM_H_
