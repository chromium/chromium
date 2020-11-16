// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_child_iterator.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

namespace blink {

NGBlockChildIterator::NGBlockChildIterator(NGLayoutInputNode first_child,
                                           const NGBlockBreakToken* break_token)
    : child_(first_child), break_token_(break_token), child_token_idx_(0) {}

NGBlockChildIterator::Entry NGBlockChildIterator::NextChild(
    const NGInlineBreakToken* previous_inline_break_token) {
  const NGBreakToken* child_break_token = nullptr;
  if (previous_inline_break_token) {
    return Entry(previous_inline_break_token->InputNode(),
                 previous_inline_break_token);
  }

  bool next_is_from_break_token = false;
  if (break_token_) {
    // If we're resuming layout after a fragmentainer break, we'll first resume
    // the children that fragmented earlier (represented by one break token
    // each).
    const auto& child_break_tokens = break_token_->ChildBreakTokens();

    while (child_token_idx_ < child_break_tokens.size()) {
      child_break_token = child_break_tokens[child_token_idx_++];
      break;
    }

    next_is_from_break_token = child_token_idx_ < child_break_tokens.size();

    if (child_break_token) {
      // If we have a child break token to resume at, that's the source of
      // truth.
      child_ = child_break_token->InputNode();
    } else if (break_token_->HasSeenAllChildren()) {
      // If there are no break tokens left to resume, the iterator machinery
      // (see further below) will by default just continue at the next sibling.
      // The last break token would be the last node that previously got
      // fragmented. However, there may be parallel flows caused by visible
      // overflow, established by descendants of our children, and these may go
      // on, fragmentainer after fragmentainer, even if we're done with our
      // direct children. When this happens, we need to prevent the machinery
      // from continuing iterating, if we're already done with those siblings.
      child_ = nullptr;
    }
  }

  NGLayoutInputNode child = child_;

  // Unless we're going to grab the next child off a break token, we'll use the
  // next sibling of the current child. Prepare it now.
  if (child_ && !next_is_from_break_token)
    child_ = child_.NextSibling();

  return Entry(child, child_break_token);
}

}  // namespace blink
