// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_simplified_oof_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"

namespace blink {

NGSimplifiedOOFLayoutAlgorithm::NGSimplifiedOOFLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params,
    const NGPhysicalBoxFragment& previous_fragment,
    bool is_new_fragment)
    : NGLayoutAlgorithm(params),
      writing_direction_(Style().GetWritingDirection()),
      incoming_break_token_(params.break_token) {
  DCHECK(previous_fragment.IsFragmentainerBox());
  DCHECK(params.space.HasKnownFragmentainerBlockSize());

  container_builder_.SetBoxType(previous_fragment.BoxType());
  container_builder_.SetFragmentBlockSize(
      params.space.FragmentainerBlockSize());

  if (incoming_break_token_)
    break_token_iterator_ = incoming_break_token_->ChildBreakTokens().begin();

  // Don't apply children to new fragments.
  if (is_new_fragment) {
    child_iterator_ = children_.end();
    container_builder_.SetIsFirstForNode(false);
    return;
  }

  container_builder_.SetIsFirstForNode(previous_fragment.IsFirstForNode());

  // We need the previous physical container size to calculate the position of
  // any child fragments.
  previous_physical_container_size_ = previous_fragment.Size();

  // Children (along with any OOF fragments that will be added as children) need
  // to be added in an order that matches the order of any incoming break tokens
  // (as indicated by the order in |break_token_iterator_|). After all incoming
  // break tokens are accounted for, the order will be determined by the
  // remaining children in |child_iterator_|, followed by any newly added OOF
  // children.
  children_ = previous_fragment.Children();
  child_iterator_ = children_.begin();
  AdvanceChildIterator();

  // Inflow-bounds should never exist on a fragmentainer.
  DCHECK(!previous_fragment.InflowBounds());
  container_builder_.SetMayHaveDescendantAboveBlockStart(
      previous_fragment.MayHaveDescendantAboveBlockStart());
}

scoped_refptr<const NGLayoutResult> NGSimplifiedOOFLayoutAlgorithm::Layout() {
  return container_builder_.ToBoxFragment();
}

void NGSimplifiedOOFLayoutAlgorithm::AppendOutOfFlowResult(
    scoped_refptr<const NGLayoutResult> result) {
  container_builder_.AddResult(*result, result->OutOfFlowPositionedOffset());

  // If there is an incoming child break token, make sure that it matches
  // the OOF child that was just added.
  if (incoming_break_token_ &&
      break_token_iterator_ !=
          incoming_break_token_->ChildBreakTokens().end()) {
    DCHECK_EQ(result->PhysicalFragment().GetLayoutObject(),
              (*break_token_iterator_)->InputNode().GetLayoutBox());
    DCHECK(!To<NGPhysicalBoxFragment>(result->PhysicalFragment())
                .IsFirstForNode() ||
           To<NGBlockBreakToken>(*break_token_iterator_)->IsBreakBefore());
    break_token_iterator_++;
    AdvanceChildIterator();
  }
}

void NGSimplifiedOOFLayoutAlgorithm::AddChildFragment(const NGLink& child) {
  const auto* fragment = To<NGPhysicalContainerFragment>(child.get());
  // Determine the previous position in the logical coordinate system.
  LogicalOffset child_offset =
      WritingModeConverter(writing_direction_,
                           previous_physical_container_size_)
          .ToLogical(child.Offset(), fragment->Size());

  // Add the fragment to the builder.
  container_builder_.AddChild(
      *fragment, child_offset, /* inline_container */ nullptr,
      /* margin_strut */ nullptr, /* is_self_collapsing */ false,
      /* offset_includes_relative_position */ true);
}

void NGSimplifiedOOFLayoutAlgorithm::AdvanceChildIterator() {
  while (child_iterator_ != children_.end()) {
    const auto& child_link = *child_iterator_;
    if (incoming_break_token_ &&
        break_token_iterator_ !=
            incoming_break_token_->ChildBreakTokens().end()) {
      // Add the current child if it matches the incoming child break token.
      const auto* break_token = *break_token_iterator_;
      if (child_link.fragment->GetLayoutObject() ==
          break_token->InputNode().GetLayoutBox()) {
        DCHECK(!To<NGPhysicalBoxFragment>(child_link.get())->IsFirstForNode() ||
               To<NGBlockBreakToken>(break_token)->IsBreakBefore());
        AddChildFragment(child_link);
        child_iterator_++;
        break_token_iterator_++;
      } else {
        // The current child does not match the incoming break token. The break
        // token must belong to an OOF positioned element that has not yet been
        // added via AppendOutOfFlowResult().
        DCHECK(break_token->InputNode().IsOutOfFlowPositioned());
        break;
      }
    } else {
      // There are no more incoming child break tokens, so add the remaining
      // children in |child_iterator_|.
      AddChildFragment(child_link);
      child_iterator_++;
    }
  }
}

}  // namespace blink
