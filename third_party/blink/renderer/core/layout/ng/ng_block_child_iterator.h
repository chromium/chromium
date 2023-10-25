// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_CHILD_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_CHILD_ITERATOR_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

namespace blink {

class InlineBreakToken;
class NGBreakToken;
class NGBlockBreakToken;

// A utility class for block-flow layout which given the first child and a
// break token will iterate through unfinished children.
//
// NextChild() is used to iterate through the children. This will be done in
// node order. If there are child break tokens, though, their nodes will be
// processed first, in break token order (which may or may not be the same as
// node order). When we're through those, we proceed to the next sibling node of
// that of the last break token - unless we have already seen and started all
// children (in which case the parent break token will be marked as such;
// |HasSeenAllChildren()| will return true).
//
// This class does not handle modifications to its arguments after it has been
// constructed.
class CORE_EXPORT NGBlockChildIterator {
  STACK_ALLOCATED();

 public:
  NGBlockChildIterator(NGLayoutInputNode first_child,
                       const NGBlockBreakToken* break_token,
                       bool calculate_child_idx = false);

  // Returns the next input node which should be laid out, along with its
  // respective break token.
  // @param previous_inline_break_token The previous inline break token is
  //    needed as multiple line-boxes can exist within the same parent
  //    fragment, unlike blocks.
  struct Entry;
  Entry NextChild(
      const InlineBreakToken* previous_inline_break_token = nullptr);

 private:
  void AdvanceToNextChild(const NGLayoutInputNode&);

  NGLayoutInputNode next_unstarted_child_;
  NGLayoutInputNode tracked_child_ = nullptr;
  const NGBlockBreakToken* break_token_;

  // An index into break_token_'s ChildBreakTokens() vector. Used for keeping
  // track of the next child break token to inspect.
  wtf_size_t child_token_idx_;

  absl::optional<wtf_size_t> child_idx_;

  bool did_handle_first_child_ = false;
};

struct NGBlockChildIterator::Entry {
  STACK_ALLOCATED();

 public:
  Entry() : node(nullptr), token(nullptr) {}
  Entry(NGLayoutInputNode node,
        const NGBreakToken* token,
        absl::optional<wtf_size_t> index = absl::nullopt)
      : node(node), token(token), index(index) {}

  NGLayoutInputNode node;
  const NGBreakToken* token;
  absl::optional<wtf_size_t> index;

  bool operator==(const NGBlockChildIterator::Entry& other) const {
    return node == other.node && token == other.token;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_CHILD_ITERATOR_H_
