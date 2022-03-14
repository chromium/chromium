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
    bool is_new_fragment,
    bool should_break_for_oof)
    : NGLayoutAlgorithm(params),
      writing_direction_(Style().GetWritingDirection()),
      incoming_break_token_(params.break_token) {
  DCHECK(previous_fragment.IsFragmentainerBox());
  DCHECK(params.space.HasKnownFragmentainerBlockSize());

  container_builder_.SetBoxType(previous_fragment.BoxType());
  container_builder_.SetFragmentBlockSize(
      params.space.FragmentainerBlockSize());
  container_builder_.SetDisableOOFDescendantsPropagation();
  container_builder_.SetHasOutOfFlowFragmentChild(true);
  if (incoming_break_token_)
    break_token_iterator_ = incoming_break_token_->ChildBreakTokens().begin();
  old_fragment_break_token_ =
      To<NGBlockBreakToken>(previous_fragment.BreakToken());
  if (old_fragment_break_token_) {
    bool has_column_spanner =
        old_fragment_break_token_->IsCausedByColumnSpanner();
    container_builder_.SetHasColumnSpanner(has_column_spanner);

    // If the previous column broke for a spanner, and we are creating
    // a new column during OOF layout, that means that the multicol hasn't
    // finished layout yet, and we are attempting to lay out the OOF before
    // the spanner. Make sure that the new column creates a break token in this
    // case, even if the OOF child doesn't break. Also, copy over any child
    // break tokens from the previous fragment to ensure that layout for those
    // children is resumed after the spanner.
    if (has_column_spanner && is_new_fragment) {
      should_break_for_oof = true;
      only_copy_break_tokens_ = true;
      AdvanceBreakTokenIterator();
    }
  }
  if (should_break_for_oof)
    container_builder_.SetDidBreakSelf();

  // Don't apply children to new fragments.
  if (is_new_fragment) {
    child_iterator_ = children_.end();
    container_builder_.SetIsFirstForNode(false);
    old_fragment_break_token_ = nullptr;
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

const NGLayoutResult* NGSimplifiedOOFLayoutAlgorithm::Layout() {
  // Any children that had previously broken due to a break before would not
  // have been traversed via the |child_iterator_|, so their break tokens should
  // be added before layout is completed.
  if (old_fragment_break_token_) {
    for (const auto& child_break_token :
         old_fragment_break_token_->ChildBreakTokens()) {
      if (To<NGBlockBreakToken>(child_break_token.Get())->IsBreakBefore())
        container_builder_.AddBreakToken(child_break_token);
    }
  }
  FinishFragmentationForFragmentainer(ConstraintSpace(), &container_builder_);
  return container_builder_.ToBoxFragment();
}

void NGSimplifiedOOFLayoutAlgorithm::AppendOutOfFlowResult(
    const NGLayoutResult* result) {
  container_builder_.AddResult(*result, result->OutOfFlowPositionedOffset());

  // If there is an incoming child break token, make sure that it matches
  // the OOF child that was just added.
  if (incoming_break_token_ &&
      break_token_iterator_ !=
          incoming_break_token_->ChildBreakTokens().end()) {
    DCHECK_EQ(result->PhysicalFragment().GetLayoutObject(),
              (*break_token_iterator_)->InputNode().GetLayoutBox());
    DCHECK(
        !To<NGPhysicalBoxFragment>(result->PhysicalFragment())
             .IsFirstForNode() ||
        To<NGBlockBreakToken>(break_token_iterator_->Get())->IsBreakBefore());
    break_token_iterator_++;
    if (only_copy_break_tokens_)
      AdvanceBreakTokenIterator();
    else
      AdvanceChildIterator();
  }
}

void NGSimplifiedOOFLayoutAlgorithm::AddChildFragment(const NGLink& child) {
  const auto* fragment = child.get();
  // Determine the previous position in the logical coordinate system.
  LogicalOffset child_offset =
      WritingModeConverter(writing_direction_,
                           previous_physical_container_size_)
          .ToLogical(child.Offset(), fragment->Size());
  // Any relative offset will have already been applied, avoid re-adding one.
  absl::optional<LogicalOffset> relative_offset = LogicalOffset();

  // Add the fragment to the builder.
  container_builder_.AddChild(
      *fragment, child_offset, /* margin_strut */ nullptr,
      /* is_self_collapsing */ false, relative_offset,
      /* inline_container */ nullptr,
      /* adjustment_for_oof_propagation */ absl::nullopt);
}

void NGSimplifiedOOFLayoutAlgorithm::AdvanceChildIterator() {
  DCHECK(!only_copy_break_tokens_);
  while (child_iterator_ != children_.end()) {
    const auto& child_link = *child_iterator_;
    if (incoming_break_token_ &&
        break_token_iterator_ !=
            incoming_break_token_->ChildBreakTokens().end()) {
      // Add the current child if it matches the incoming child break token.
      const auto& break_token = *break_token_iterator_;
      if (child_link.fragment->GetLayoutObject() ==
          break_token->InputNode().GetLayoutBox()) {
        DCHECK(!To<NGPhysicalBoxFragment>(child_link.get())->IsFirstForNode() ||
               To<NGBlockBreakToken>(break_token.Get())->IsBreakBefore());
        AddChildFragment(child_link);
        child_iterator_++;
        break_token_iterator_++;
      } else {
        // The current child does not match the incoming break token. The break
        // token must belong to an OOF positioned element that has not yet been
        // added via AppendOutOfFlowResult().
        DCHECK(break_token->InputNode().IsOutOfFlowPositioned());
        // We don't force OOF breaks, unless the preceding block has a forced
        // break and we need to break to get the correct static position.
        // However, the break token that is created for this case should be
        // skipped.
        if (To<NGBlockBreakToken>(break_token.Get())->IsForcedBreak()) {
          break_token_iterator_++;
          continue;
        }
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

// In some cases, for example in the case of a column spanner, we may add a new
// column before the multicol has finished layout, so we will want to carry
// over any child break tokens from the previous fragmentainer without adding
// their associated child fragments.
void NGSimplifiedOOFLayoutAlgorithm::AdvanceBreakTokenIterator() {
  DCHECK(only_copy_break_tokens_);
  while (incoming_break_token_ &&
         break_token_iterator_ !=
             incoming_break_token_->ChildBreakTokens().end()) {
    // Add the current break token if it is not an OOF positioned element.
    const auto& break_token = *break_token_iterator_;
    if (!break_token->InputNode().IsOutOfFlowPositioned()) {
      container_builder_.AddBreakToken(break_token);
      break_token_iterator_++;
    } else {
      // The break token must belong to an OOF positioned element that has not
      // yet been added via AppendOutOfFlowResult().
      break;
    }
  }
}

}  // namespace blink
