// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fragment_child_iterator.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

NGFragmentChildIterator::NGFragmentChildIterator(
    const NGPhysicalBoxFragment& parent,
    const NGBlockBreakToken* parent_break_token)
    : parent_fragment_(&parent),
      parent_break_token_(parent_break_token),
      is_fragmentation_context_root_(parent.IsFragmentationContextRoot()) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  current_.link_.fragment = nullptr;
  if (parent_break_token)
    child_break_tokens_ = parent_break_token->ChildBreakTokens();
  if (parent.HasItems()) {
    current_.cursor_.emplace(parent);
    current_.block_break_token_ = parent_break_token;
    UpdateSelfFromCursor();
  } else {
    UpdateSelfFromFragment();
  }
}

NGFragmentChildIterator::NGFragmentChildIterator(
    const NGInlineCursor& parent,
    const NGBlockBreakToken* parent_break_token,
    base::span<const NGBreakToken* const> child_break_tokens)
    : parent_break_token_(parent_break_token),
      child_break_tokens_(child_break_tokens) {
  current_.block_break_token_ = parent_break_token;
  current_.link_.fragment = nullptr;
  current_.cursor_ = parent.CursorForDescendants();
  UpdateSelfFromCursor();
}

NGFragmentChildIterator NGFragmentChildIterator::Descend() const {
  if (current_.cursor_) {
    const NGFragmentItem* item = current_.cursor_->CurrentItem();
    // Descend using the cursor if the current item doesn't establish a new
    // formatting context.
    if (!item->IsFormattingContextRoot()) {
      return NGFragmentChildIterator(
          *current_.cursor_,
          current_.BlockBreakToken() ? parent_break_token_ : nullptr,
          child_break_tokens_.subspan(child_break_token_idx_));
    }
  }
  DCHECK(current_.BoxFragment());
  return NGFragmentChildIterator(*current_.BoxFragment(),
                                 current_.BlockBreakToken());
}

bool NGFragmentChildIterator::AdvanceChildFragment() {
  DCHECK(parent_fragment_);
  const auto children = parent_fragment_->Children();
  const NGPhysicalBoxFragment* previous_fragment =
      To<NGPhysicalBoxFragment>(current_.link_.fragment);
  DCHECK(previous_fragment);
  if (child_fragment_idx_ < children.size())
    child_fragment_idx_++;
  // There may be line box fragments among the children, and we're not
  // interested in them (lines will already have been handled by the inline
  // cursor).
  SkipToBoxFragment();
  if (child_fragment_idx_ >= children.size())
    return false;
  if (child_break_token_idx_ < child_break_tokens_.size())
    child_break_token_idx_++;
  UpdateSelfFromFragment(previous_fragment);
  return true;
}

void NGFragmentChildIterator::UpdateSelfFromFragment(
    const NGPhysicalBoxFragment* previous_fragment) {
  DCHECK(parent_fragment_);
  const auto children = parent_fragment_->Children();
  if (child_fragment_idx_ >= children.size())
    return;
  current_.link_ = children[child_fragment_idx_];
  DCHECK(current_.link_.fragment);
  SkipToBlockBreakToken();
  if (child_break_token_idx_ < child_break_tokens_.size()) {
    current_.block_break_token_ =
        To<NGBlockBreakToken>(child_break_tokens_[child_break_token_idx_]);
    // TODO(mstensho): Clean up this. What we're trying to do here is to detect
    // whether the incoming break token matches the current fragment or not.
    // Figuring out if a fragment is generated from a given node is currently
    // not possible without checking the LayoutObject associated.
    const auto* layout_object = current_.link_.fragment->GetLayoutObject();
    if (layout_object &&
        layout_object !=
            current_.block_break_token_->InputNode().GetLayoutBox()) {
      DCHECK(current_.link_.fragment->IsColumnSpanAll());
      current_.break_token_for_fragmentainer_only_ = true;
    } else {
      current_.break_token_for_fragmentainer_only_ = false;
    }
  } else if (is_fragmentation_context_root_ && previous_fragment) {
    if (previous_fragment->IsFragmentainerBox()) {
      // The outgoing break token from one fragmentainer is the incoming break
      // token to the next one. This is also true when there are column spanners
      // between two columns (fragmentainers); the outgoing break token from the
      // former column will be ignored by any intervening spanners, and then fed
      // into the first column that comes after them, as an incoming break
      // token.
      current_.block_break_token_ =
          To<NGBlockBreakToken>(previous_fragment->BreakToken());
      current_.break_token_for_fragmentainer_only_ = true;
    } else {
      // This is a column spanner, or in the case of a fieldset, this could be a
      // rendered legend. We'll leave |current_block_break_token_| alone here,
      // as it will be used as in incoming break token when we get to the next
      // column.
      DCHECK(previous_fragment->IsRenderedLegend() ||
             previous_fragment->IsColumnSpanAll());
    }
  } else {
    current_.block_break_token_ = nullptr;
  }
}

bool NGFragmentChildIterator::AdvanceWithCursor() {
  DCHECK(current_.cursor_);
  current_.cursor_->MoveToNextSkippingChildren();
  UpdateSelfFromCursor();
  if (current_.cursor_->CurrentItem())
    return true;
  // If there are more items, proceed and see if we have box fragment
  // children. There may be out-of-flow positioned child fragments.
  if (!parent_fragment_)
    return false;
  current_.cursor_.reset();
  SkipToBoxFragment();
  UpdateSelfFromFragment();
  return !IsAtEnd();
}

void NGFragmentChildIterator::UpdateSelfFromCursor() {
  DCHECK(current_.cursor_);
  // For inline items we just use the incoming break token to the containing
  // block.
  current_.block_break_token_ = parent_break_token_;
  const NGFragmentItem* item = current_.cursor_->CurrentItem();
  if (!item) {
    current_.link_.fragment = nullptr;
    return;
  }
  current_.link_ = {item->BoxFragment(), item->OffsetInContainerFragment()};
}

void NGFragmentChildIterator::SkipToBoxFragment() {
  for (const auto children = parent_fragment_->Children();
       child_fragment_idx_ < children.size(); child_fragment_idx_++) {
    if (children[child_fragment_idx_].fragment->IsBox())
      break;
  }
}

void NGFragmentChildIterator::SkipToBlockBreakToken() {
  // There may be inline break tokens here. Ignore them.
  while (child_break_token_idx_ < child_break_tokens_.size() &&
         !child_break_tokens_[child_break_token_idx_]->IsBlockType())
    child_break_token_idx_++;
}

}  // namespace blink
