// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/block_child_iterator.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/layout_input_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

BlockChildIterator::BlockChildIterator(
    LayoutInputNode first_child,
    const BlockBreakToken* container_break_token,
    bool calculate_child_idx)
    : container_break_token_(container_break_token), child_token_idx_(0) {
  is_ifc_ = first_child && first_child.IsInline();
  DCHECK(!calculate_child_idx || !is_ifc_);
  if (is_ifc_) {
    return;
  }
  BlockNode first_block_child(To<BlockNode>(first_child));
  next_unstarted_child_ = first_block_child;
  if (calculate_child_idx) {
    // If we are set up to provide the child index, we also need to visit all
    // siblings, also when processing break tokens.
    child_idx_.emplace(0);
    tracked_child_ = first_block_child;
  }
  if (container_break_token_) {
    const auto& child_break_tokens = container_break_token_->ChildBreakTokens();
    // If there are child break tokens, we don't yet know which one is the the
    // next unstarted child (need to get past the child break tokens first). If
    // we've already seen all children, there will be no unstarted children.
    if (!child_break_tokens.empty() ||
        container_break_token_->HasSeenAllChildren()) {
      next_unstarted_child_ = nullptr;
    }
    // We're already done with this parent break token if there are no child
    // break tokens, so just forget it right away.
    if (child_break_tokens.empty())
      container_break_token_ = nullptr;
  }
}

BlockChildIterator::Entry BlockChildIterator::NextChild(
    const InlineBreakToken* previous_inline_break_token) {
  DCHECK(is_ifc_ || !previous_inline_break_token);
  if (is_ifc_) {
    bool is_unstarted_ifc = !did_handle_first_child_;
    did_handle_first_child_ = true;
    if (previous_inline_break_token) {
      // New line in the same block fragment.
      container_break_token_ = nullptr;
      return Entry(previous_inline_break_token);
    }
    if (container_break_token_) {
      // New line in a new block fragment.
      const auto& tokens = container_break_token_->ChildBreakTokens();
      if (tokens.empty() && container_break_token_->HasSeenAllChildren()) {
        return Entry();
      }
      if (child_token_idx_ < tokens.size()) {
        const BreakToken* token = tokens[child_token_idx_++];
        if (const auto* block_token = DynamicTo<BlockBreakToken>(token)) {
          // Out-of-flow positioned node in inline formatting context.
          BlockNode node = block_token->InputNode();
          DCHECK(node.IsOutOfFlowPositioned());
          DCHECK(RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
          return Entry(node, block_token);
        }
        return Entry(To<InlineBreakToken>(token));
      }
    }
    return Entry(/*token=*/nullptr, is_unstarted_ifc);
  }

  if (did_handle_first_child_) {
    if (container_break_token_) {
      const auto& child_break_tokens =
          container_break_token_->ChildBreakTokens();
      if (child_token_idx_ == child_break_tokens.size()) {
        // We reached the last child break token. Prepare for the next unstarted
        // sibling, and forget the container break token.
        if (!container_break_token_->HasSeenAllChildren()) {
          BlockNode last_resumed_child =
              To<BlockBreakToken>(
                  child_break_tokens[child_token_idx_ - 1].Get())
                  ->InputNode();
          AdvanceToNextChild(last_resumed_child);
        }
        container_break_token_ = nullptr;
      }
    } else if (next_unstarted_child_) {
      AdvanceToNextChild(next_unstarted_child_);
    }
  } else {
    did_handle_first_child_ = true;
  }

  const BlockBreakToken* current_child_break_token = nullptr;
  std::optional<wtf_size_t> current_child_idx;
  BlockNode current_child = next_unstarted_child_;
  if (container_break_token_) {
    // If we're resuming layout after a fragmentainer break, we'll first resume
    // the children that fragmented earlier (represented by one break token
    // each).
    DCHECK(!next_unstarted_child_);
    const auto& child_break_tokens = container_break_token_->ChildBreakTokens();
    DCHECK_LT(child_token_idx_, child_break_tokens.size());
    current_child_break_token =
        To<BlockBreakToken>(child_break_tokens[child_token_idx_++].Get());
    current_child = current_child_break_token->InputNode();

    // Unless it's a resumed OOF, update child tracking, if enabled.
    if (child_idx_ && !current_child.IsOutOfFlowPositioned()) {
      while (tracked_child_ != current_child) {
        tracked_child_ = tracked_child_.NextBlockSibling();
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

void BlockChildIterator::AdvanceToNextChild(const BlockNode& child) {
  next_unstarted_child_ = child.NextBlockSibling();
  if (child_idx_)
    (*child_idx_)++;
}

}  // namespace blink
