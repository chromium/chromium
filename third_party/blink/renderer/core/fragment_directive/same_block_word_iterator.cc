// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/same_block_word_iterator.h"

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

template <typename Direction>
SameBlockWordIterator<Direction>::SameBlockWordIterator(
    const PositionInFlatTree& position)
    : current_node_text_(Direction::NodeTextFromOffset(position)),
      current_text_offset_(Direction::FirstPosition(current_node_text_)),
      current_node_(*position.ComputeContainerNode()),
      start_position_(position) {
  current_node_text_.Ensure16Bit();
}

template <typename Direction>
String SameBlockWordIterator<Direction>::TextFromStart() const {
  String range_text;
  if (start_position_.ComputeContainerNode() != current_node_) {
    // If current node is not the node the iterator started with include any
    // text that came before the current node.
    range_text = Direction::RangeText(
        start_position_,
        PositionInFlatTree(current_node_,
                           Direction::FirstPosition(current_node_text_)));
  }

  // The text from the current node should be extracted from the text as the
  // offset is a text offset which might not match a position in a node.
  String current_node_words = Direction::Substring(
      current_node_text_, Direction::FirstPosition(current_node_text_),
      current_text_offset_);

  return Direction::Concat(range_text, current_node_words).StripWhiteSpace();
}

template <typename Direction>
bool SameBlockWordIterator<Direction>::AdvanceNextWord() {
  do {
    int pos =
        Direction::FindNextWordPos(current_node_text_, current_text_offset_);
    unsigned next_word_stripped_length =
        Direction::Substring(current_node_text_, current_text_offset_, pos)
            .LengthWithStrippedWhiteSpace();
    if (next_word_stripped_length > 0) {
      current_text_offset_ = pos;
      return true;
    }
  } while (NextNode());
  return false;
}

template <typename Direction>
bool SameBlockWordIterator<Direction>::NextNode() {
  Node* next_node = NextVisibleTextNodeWithinBlock(*current_node_);
  if (next_node == nullptr) {
    return false;
  }

  current_node_text_ = PlainText(EphemeralRange::RangeOfContents(*next_node));
  current_node_text_.Ensure16Bit();
  current_text_offset_ = Direction::FirstPosition(current_node_text_);
  current_node_ = next_node;
  return true;
}

// Returns the next/previous node within same block as |start_node| without
// crossing block boundaries.
template <typename Direction>
Node* SameBlockWordIterator<Direction>::NextVisibleTextNodeWithinBlock(
    Node& start_node) {
  if (!start_node.GetLayoutObject())
    return nullptr;

  // Move forward/backward until no next/previous node is available within same
  // |block_ancestor|.
  Node* node = &start_node;
  do {
    node = Direction::Next(*node);
    if (node) {
      node = Direction::AdvanceUntilVisibleTextNode(*node);
    }
  } while (node && !node->GetLayoutObject());

  // Stop, if crossed block boundaries.
  if (!node || !Direction::IsInSameUninterruptedBlock(start_node, *node))
    return nullptr;

  return node;
}

template <typename Direction>
void SameBlockWordIterator<Direction>::Trace(Visitor* visitor) const {
  visitor->Trace(current_node_);
  visitor->Trace(start_position_);
}

template class CORE_TEMPLATE_EXPORT SameBlockWordIterator<ForwardDirection>;
template class CORE_TEMPLATE_EXPORT SameBlockWordIterator<BackwardDirection>;

}  // namespace blink
