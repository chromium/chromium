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
                                 bool is_new_fragment,
                                 bool should_break_for_oof = false);

  scoped_refptr<const NGLayoutResult> Layout() override;
  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) override {
    NOTREACHED();
    return MinMaxSizesResult();
  }

  void AppendOutOfFlowResult(scoped_refptr<const NGLayoutResult> child);

 private:
  void AddChildFragment(const NGLink& old_fragment);
  void AdvanceChildIterator();
  void AdvanceBreakTokenIterator();

  const WritingDirectionMode writing_direction_;
  PhysicalSize previous_physical_container_size_;

  base::span<const NGLink> children_;
  base::span<const NGLink>::iterator child_iterator_;
  const NGBlockBreakToken* incoming_break_token_;
  const NGBlockBreakToken* old_fragment_break_token_;
  base::span<const Member<const NGBreakToken>>::iterator break_token_iterator_;
  bool only_copy_break_tokens_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_SIMPLIFIED_OOF_LAYOUT_ALGORITHM_H_
