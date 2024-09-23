// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/block_child_iterator.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/layout_input_node.h"

namespace blink {

BlockChildIterator::BlockChildIterator(LayoutInputNode first_child,
                                       const BlockBreakToken* break_token,
                                       bool calculate_child_idx)
    : next_unstarted_child_(first_child),
      break_token_(break_token),
      child_token_idx_(0) {
  if (calculate_child_idx) {
    // If we are set up to provide the child index, we also need to visit all
    // siblings, also when processing break tokens.
    child_idx_.emplace(0);
    tracked_child_ = first_child;
  }
  if (break_token_) {
    const auto& child_break_tokens = break_token_->ChildBreakTokens();
    // If there are child break tokens, we don't yet know which one is the the
    // next unstarted child (need to get past the child break tokens first). If
    // we've already seen all children, there will be no unstarted children.
    if (!child_break_tokens.empty() || break_token_->HasSeenAllChildren())
      next_unstarted_child_ = nullptr;
    // We're already done with this parent break token if there are no child
    // break tokens, so just forget it right away.
    if (child_break_tokens.empty())
      break_token_ = nullptr;
  }
}

BlockChildIterator::Entry BlockChildIterator::NextChild(
    const InlineBreakToken* previous_inline_break_token) {
  if (previous_inline_break_token) {
    DCHECK(!child_idx_);
    return Entry(previous_inline_break_token->InputNode(),
                 previous_inline_break_token, std::nullopt);
  }

  if (did_handle_first_child_) {
    if (break_token_) {
      const auto& child_break_tokens = break_token_->ChildBreakTokens();
      if (child_token_idx_ == child_break_tokens.size()) {
        // We reached the last child break token. Prepare for the next unstarted
        // sibling, and forget the parent break token.
        if (!break_token_->HasSeenAllChildren()) {
          AdvanceToNextChild(
              child_break_tokens[child_token_idx_ - 1]->InputNode());
        }
        break_token_ = nullptr;
      }
    } else if (next_unstarted_child_) {
      AdvanceToNextChild(next_unstarted_child_);
    }
  } else {
    did_handle_first_child_ = true;
  }

  const BreakToken* current_child_break_token = nullptr;
  std::optional<wtf_size_t> current_child_idx;
  LayoutInputNode current_child = next_unstarted_child_;
  if (break_token_) {
    // If we're resuming layout after a fragmentainer break, we'll first resume
    // the children that fragmented earlier (represented by one break token
    // each).
    DCHECK(!next_unstarted_child_);
    const auto& child_break_tokens = break_token_->ChildBreakTokens();
    DCHECK_LT(child_token_idx_, child_break_tokens.size());
    current_child_break_token = child_break_tokens[child_token_idx_++];
    current_child = current_child_break_token->InputNode();

    if (child_idx_) {
      while (tracked_child_ != current_child) {
        tracked_child_ = tracked_child_.NextSibling();
        (*child_idx_)++;
      }
      current_child_idx = child_idx_;
    }
  } else if (next_unstarted_child_) {
    current_child_idx = child_idx_;
  }

  // Layout of a preceding sibling may have triggered removal of a
  // later sibling. Container query evaluations may trigger such
  // removals. As long as we just walk the node siblings, we're
  // fine, but if the later sibling was among the incoming
  // child break tokens, we now have a problem (but hopefully an
  // impossible scenario)
#if DCHECK_IS_ON()
  if (const LayoutBox* box = current_child.GetLayoutBox())
    DCHECK(box->IsInDetachedNonDomTree() || box->Parent());
#endif
  return Entry(current_child, current_child_break_token, current_child_idx);
}

void BlockChildIterator::AdvanceToNextChild(const LayoutInputNode& child) {
  next_unstarted_child_ = child.NextSibling();
  if (child_idx_)
    (*child_idx_)++;
}

}  // namespace blink
