/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/editing/position_iterator.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position.h"

namespace blink {

namespace {

// TODO(editing-dev): We should replace usages of |hasChildren()| in
// |PositionIterator| to |shouldTraverseChildren()|.
template <typename Strategy>
bool ShouldTraverseChildren(const Node& node) {
  return Strategy::HasChildren(node) && !IsUserSelectContain(node);
}

template <typename Strategy>
int LastOffsetForPositionIterator(const Node* node) {
  return IsUserSelectContain(*node) ? 1 : Strategy::LastOffsetForEditing(node);
}

// TODO(editing-dev): We should replace usages of |parent()| in
// |PositionIterator| to |selectableParentOf()|.
template <typename Strategy>
ContainerNode* SelectableParentOf(const Node& node) {
  ContainerNode* const parent = Strategy::Parent(node);
  return parent && !IsUserSelectContain(*parent) ? parent : nullptr;
}

}  // namespace

static constexpr int kInvalidOffset = -1;

template <typename Strategy>
SlowPositionIteratorAlgorithm<Strategy>::SlowPositionIteratorAlgorithm(
    const PositionTemplate<Strategy>& pos) {
  if (pos.IsNull())
    return;
  anchor_node_ = pos.AnchorNode();
  const int offset_in_anchor = pos.ComputeEditingOffset();

  node_after_position_in_anchor_ =
      Strategy::ChildAt(*anchor_node_, offset_in_anchor);
  offset_in_anchor_ = node_after_position_in_anchor_ ? 0 : offset_in_anchor;
  dom_tree_version_ = anchor_node_->GetDocument().DomTreeVersion();

  for (Node* node = SelectableParentOf<Strategy>(*anchor_node_); node;
       node = SelectableParentOf<Strategy>(*node)) {
    // Each offsets_in_anchor_node_[offset] should be an index of node in
    // parent, but delay to calculate the index until it is needed for
    // performance.
    offsets_in_anchor_node_.push_back(kInvalidOffset);
    ++depth_to_anchor_node_;
  }
  if (node_after_position_in_anchor_)
    offsets_in_anchor_node_.push_back(offset_in_anchor);
}

template <typename Strategy>
PositionTemplate<Strategy>
SlowPositionIteratorAlgorithm<Strategy>::DeprecatedComputePosition() const {
  // TODO(yoichio): Share code to check domTreeVersion with EphemeralRange.
  DCHECK(IsValid());
  if (node_after_position_in_anchor_) {
    DCHECK(anchor_node_);
    DCHECK_EQ(Strategy::Parent(*node_after_position_in_anchor_), anchor_node_);
    DCHECK_NE(offsets_in_anchor_node_[depth_to_anchor_node_], kInvalidOffset);
    // FIXME: This check is inadaquete because any ancestor could be ignored by
    // editing
    if (EditingIgnoresContent(
            *Strategy::Parent(*node_after_position_in_anchor_)))
      return PositionTemplate<Strategy>::BeforeNode(*anchor_node_);
    return PositionTemplate<Strategy>(
        anchor_node_, offsets_in_anchor_node_[depth_to_anchor_node_]);
  }
  if (!anchor_node_)
    return PositionTemplate<Strategy>();
  if (Strategy::HasChildren(*anchor_node_)) {
    return PositionTemplate<Strategy>::LastPositionInOrAfterNode(*anchor_node_);
  }
  return PositionTemplate<Strategy>::EditingPositionOf(anchor_node_,
                                                       offset_in_anchor_);
}

template <typename Strategy>
PositionTemplate<Strategy>
SlowPositionIteratorAlgorithm<Strategy>::ComputePosition() const {
  DCHECK(IsValid());
  // Assume that we have the following DOM tree:
  // A
  // |-B
  // | |-E
  // | +-F
  // |
  // |-C
  // +-D
  //   |-G
  //   +-H
  if (node_after_position_in_anchor_) {
    // For example, position is before E, F.
    DCHECK(anchor_node_);
    DCHECK_EQ(Strategy::Parent(*node_after_position_in_anchor_), anchor_node_);
    DCHECK_NE(offsets_in_anchor_node_[depth_to_anchor_node_], kInvalidOffset);
    // TODO(yoichio): This should be equivalent to PositionTemplate<Strategy>(
    // anchor_node_, PositionAnchorType::kBeforeAnchor).
    return PositionTemplate<Strategy>(
        anchor_node_, offsets_in_anchor_node_[depth_to_anchor_node_]);
  }
  if (!anchor_node_)
    return PositionTemplate<Strategy>();
  if (ShouldTraverseChildren<Strategy>(*anchor_node_)) {
    // For example, position is the end of B.
    return PositionTemplate<Strategy>::LastPositionInOrAfterNode(*anchor_node_);
  }
  if (anchor_node_->IsTextNode())
    return PositionTemplate<Strategy>(anchor_node_, offset_in_anchor_);
  if (offset_in_anchor_)
    // For example, position is after G.
    return PositionTemplate<Strategy>(anchor_node_,
                                      PositionAnchorType::kAfterAnchor);
  // For example, position is before G.
  return PositionTemplate<Strategy>(anchor_node_,
                                    PositionAnchorType::kBeforeAnchor);
}

template <typename Strategy>
void SlowPositionIteratorAlgorithm<Strategy>::Increment() {
  DCHECK(IsValid());
  if (!anchor_node_)
    return;

  // Assume that we have the following DOM tree:
  // A
  // |-B
  // | |-E
  // | +-F
  // |
  // |-C
  // +-D
  //   |-G
  //   +-H
  // Let |anchor| as |anchor_node_| and
  // |child| as |node_after_position_in_anchor_|.
  if (node_after_position_in_anchor_) {
    // Case #1: Move to position before the first child of
    // |node_after_position_in_anchor_|.
    // This is a point just before |child|.
    // Let |anchor| is A and |child| is B,
    // then next |anchor| is B and |child| is E.
    anchor_node_ = node_after_position_in_anchor_;
    node_after_position_in_anchor_ =
        ShouldTraverseChildren<Strategy>(*anchor_node_)
            ? Strategy::FirstChild(*anchor_node_)
            : nullptr;
    offset_in_anchor_ = 0;
    // Increment depth intializing with 0.
    ++depth_to_anchor_node_;
    if (depth_to_anchor_node_ == offsets_in_anchor_node_.size())
      offsets_in_anchor_node_.push_back(0);
    else
      offsets_in_anchor_node_[depth_to_anchor_node_] = 0;
    return;
  }

  if (anchor_node_->GetLayoutObject() &&
      !ShouldTraverseChildren<Strategy>(*anchor_node_) &&
      offset_in_anchor_ <
          LastOffsetForPositionIterator<Strategy>(anchor_node_)) {
    // Case #2. This is the next of Case #1 or #2 itself.
    // Position is (|anchor|, |offset_in_anchor_|).
    // In this case |anchor| is a leaf(E,F,C,G or H) and
    // |offset_in_anchor_| is not on the end of |anchor|.
    // Then just increment |offset_in_anchor_|.
    offset_in_anchor_ =
        NextGraphemeBoundaryOf(*anchor_node_, offset_in_anchor_);
  } else {
    // Case #3. This is the next of Case #2 or #3.
    // Position is the end of |anchor|.
    // 3-a. If |anchor| has next sibling (let E),
    //      next |anchor| is B and |child| is F (next is Case #1.)
    // 3-b. If |anchor| doesn't have next sibling (let F),
    //      next |anchor| is B and |child| is null. (next is Case #3.)
    node_after_position_in_anchor_ = anchor_node_;
    anchor_node_ =
        SelectableParentOf<Strategy>(*node_after_position_in_anchor_);
    if (!anchor_node_)
      return;
    DCHECK_GT(depth_to_anchor_node_, 0u);
    --depth_to_anchor_node_;
    // Increment offset of |child| or initialize if it have never been
    // used.
    if (offsets_in_anchor_node_[depth_to_anchor_node_] == kInvalidOffset)
      offsets_in_anchor_node_[depth_to_anchor_node_] =
          Strategy::Index(*node_after_position_in_anchor_) + 1;
    else
      ++offsets_in_anchor_node_[depth_to_anchor_node_];
    node_after_position_in_anchor_ =
        Strategy::NextSibling(*node_after_position_in_anchor_);
    offset_in_anchor_ = offsets_in_anchor_node_[depth_to_anchor_node_];
  }
}

template <typename Strategy>
void SlowPositionIteratorAlgorithm<Strategy>::Decrement() {
  DCHECK(IsValid());
  if (!anchor_node_)
    return;

  // Assume that we have the following DOM tree:
  // A
  // |-B
  // | |-E
  // | +-F
  // |
  // |-C
  // +-D
  //   |-G
  //   +-H
  // Let |anchor| as |anchor_node_| and
  // |child| as |node_after_position_in_anchor_|.
  // Decrement() is complex but logically reverse of Increment(), of course:)
  if (node_after_position_in_anchor_) {
    anchor_node_ = Strategy::PreviousSibling(*node_after_position_in_anchor_);
    if (anchor_node_) {
      // Case #1-a. This is a revese of Increment()::Case#3-a.
      // |child| has a previous sibling.
      // Let |anchor| is B and |child| is F,
      // next |anchor| is E and |child| is null.
      node_after_position_in_anchor_ = nullptr;
      offset_in_anchor_ =
          ShouldTraverseChildren<Strategy>(*anchor_node_)
              ? 0
              : LastOffsetForPositionIterator<Strategy>(anchor_node_);
      // Decrement offset of |child| or initialize if it have never been
      // used.
      if (offsets_in_anchor_node_[depth_to_anchor_node_] == kInvalidOffset)
        offsets_in_anchor_node_[depth_to_anchor_node_] =
            Strategy::Index(*node_after_position_in_anchor_);
      else
        --offsets_in_anchor_node_[depth_to_anchor_node_];
      DCHECK_GE(offsets_in_anchor_node_[depth_to_anchor_node_], 0);
      // Increment depth intializing with last offset.
      ++depth_to_anchor_node_;
      if (depth_to_anchor_node_ >= offsets_in_anchor_node_.size())
        offsets_in_anchor_node_.push_back(offset_in_anchor_);
      else
        offsets_in_anchor_node_[depth_to_anchor_node_] = offset_in_anchor_;
      return;
    } else {
      // Case #1-b. This is a revese of Increment()::Case#1.
      // |child| doesn't have a previous sibling.
      // Let |anchor| is B and |child| is E,
      // next |anchor| is A and |child| is B.
      node_after_position_in_anchor_ =
          Strategy::Parent(*node_after_position_in_anchor_);
      anchor_node_ =
          SelectableParentOf<Strategy>(*node_after_position_in_anchor_);
      if (!anchor_node_)
        return;
      offset_in_anchor_ = 0;
      // Decrement depth and intialize if needs.
      DCHECK_GT(depth_to_anchor_node_, 0u);
      --depth_to_anchor_node_;
      if (offsets_in_anchor_node_[depth_to_anchor_node_] == kInvalidOffset)
        offsets_in_anchor_node_[depth_to_anchor_node_] =
            Strategy::Index(*node_after_position_in_anchor_);
    }
    return;
  }

  if (ShouldTraverseChildren<Strategy>(*anchor_node_)) {
    // Case #2. This is a reverse of increment()::Case3-b.
    // Let |anchor| is B, next |anchor| is F.
    anchor_node_ = Strategy::LastChild(*anchor_node_);
    offset_in_anchor_ =
        ShouldTraverseChildren<Strategy>(*anchor_node_)
            ? 0
            : LastOffsetForPositionIterator<Strategy>(anchor_node_);
    // Decrement depth initializing with -1 because
    // |node_after_position_in_anchor_| is null so still unneeded.
    if (depth_to_anchor_node_ >= offsets_in_anchor_node_.size())
      offsets_in_anchor_node_.push_back(kInvalidOffset);
    else
      offsets_in_anchor_node_[depth_to_anchor_node_] = kInvalidOffset;
    ++depth_to_anchor_node_;
    return;
  }
  if (offset_in_anchor_ && anchor_node_->GetLayoutObject()) {
    // Case #3-a. This is a reverse of Increment()::Case#2.
    // In this case |anchor| is a leaf(E,F,C,G or H) and
    // |offset_in_anchor_| is not on the beginning of |anchor|.
    // Then just decrement |offset_in_anchor_|.
    offset_in_anchor_ =
        PreviousGraphemeBoundaryOf(*anchor_node_, offset_in_anchor_);
    return;
  }
  // Case #3-b. This is a reverse of Increment()::Case#1.
  // In this case |anchor| is a leaf(E,F,C,G or H) and
  // |offset_in_anchor_| is on the beginning of |anchor|.
  // Let |anchor| is E,
  // next |anchor| is B and |child| is E.
  node_after_position_in_anchor_ = anchor_node_;
  anchor_node_ = SelectableParentOf<Strategy>(*anchor_node_);
  if (!anchor_node_)
    return;
  DCHECK_GT(depth_to_anchor_node_, 0u);
  --depth_to_anchor_node_;
  if (offsets_in_anchor_node_[depth_to_anchor_node_] != kInvalidOffset)
    return;
  offset_in_anchor_ = Strategy::Index(*node_after_position_in_anchor_);
  offsets_in_anchor_node_[depth_to_anchor_node_] = offset_in_anchor_;
}

template <typename Strategy>
bool SlowPositionIteratorAlgorithm<Strategy>::AtStart() const {
  DCHECK(IsValid());
  if (!anchor_node_)
    return true;
  if (Strategy::Parent(*anchor_node_))
    return false;
  return (!Strategy::HasChildren(*anchor_node_) && !offset_in_anchor_) ||
         (node_after_position_in_anchor_ &&
          !Strategy::PreviousSibling(*node_after_position_in_anchor_));
}

template <typename Strategy>
bool SlowPositionIteratorAlgorithm<Strategy>::AtEnd() const {
  DCHECK(IsValid());
  if (!anchor_node_)
    return true;
  if (node_after_position_in_anchor_)
    return false;
  return !Strategy::Parent(*anchor_node_) &&
         (Strategy::HasChildren(*anchor_node_) ||
          offset_in_anchor_ >= Strategy::LastOffsetForEditing(anchor_node_));
}

template <typename Strategy>
bool SlowPositionIteratorAlgorithm<Strategy>::AtStartOfNode() const {
  DCHECK(IsValid());
  if (!anchor_node_)
    return true;
  if (!node_after_position_in_anchor_) {
    return !ShouldTraverseChildren<Strategy>(*anchor_node_) &&
           !offset_in_anchor_;
  }
  return !Strategy::PreviousSibling(*node_after_position_in_anchor_);
}

template <typename Strategy>
bool SlowPositionIteratorAlgorithm<Strategy>::AtEndOfNode() const {
  DCHECK(IsValid());
  if (!anchor_node_)
    return true;
  if (node_after_position_in_anchor_)
    return false;
  return Strategy::HasChildren(*anchor_node_) ||
         offset_in_anchor_ >= Strategy::LastOffsetForEditing(anchor_node_);
}

template class CORE_TEMPLATE_EXPORT
    SlowPositionIteratorAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    SlowPositionIteratorAlgorithm<EditingInFlatTreeStrategy>;

// ---

// static
template <typename Strategy>
typename FastPositionIteratorAlgorithm<Strategy>::ContainerType
FastPositionIteratorAlgorithm<Strategy>::ContainerToContainerType(
    const Node* node) {
  if (!node)
    return kNullNode;
  if (IsA<Text>(node) && node->GetLayoutObject())
    return kTextNode;
  if (IsA<CharacterData>(node))
    return kCharacterData;
  if (!Strategy::HasChildren(*node))
    return kNoChildren;
  if (::blink::IsUserSelectContain(*node))
    return kUserSelectContainNode;
  return kContainerNode;
}

template <typename Strategy>
FastPositionIteratorAlgorithm<Strategy>::FastPositionIteratorAlgorithm(
    const PositionType& position) {
  Initialize(position);
  AssertOffsetInContainerIsValid();
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::Initialize(
    const PositionType& position) {
  container_node_ = position.AnchorNode();
  if (!container_node_)
    return;
  dom_tree_version_ = container_node_->GetDocument().DomTreeVersion();
  container_type_ = ContainerToContainerType(container_node_);

  switch (container_type_) {
    case kNullNode:
      NOTREACHED_IN_MIGRATION();
      return;
    case kNoChildren:
      switch (position.AnchorType()) {
        case PositionAnchorType::kAfterChildren:
        case PositionAnchorType::kAfterAnchor:
          offset_in_container_ = IgnoresChildren() ? 1 : 0;
          return;
        case PositionAnchorType::kBeforeAnchor:
          offset_in_container_ = 0;
          return;
        case PositionAnchorType::kOffsetInAnchor:
          DCHECK(!position.OffsetInContainerNode());
          offset_in_container_ = 0;
          return;
      }
      NOTREACHED_IN_MIGRATION() << "Invalid PositionAnchorType";
      return;
    case kCharacterData:
    case kTextNode:
      // Note: `Position::ComputeOffsetInContainer()` for `kAfterAnchor`
      // returns `container_node_->Index() + 1` instead of `Text::length()`.
      switch (position.AnchorType()) {
        case PositionAnchorType::kAfterChildren:
          NOTREACHED_IN_MIGRATION();
          break;
        case PositionAnchorType::kAfterAnchor:
          offset_in_container_ = To<CharacterData>(container_node_)->length();
          return;
        case PositionAnchorType::kBeforeAnchor:
          offset_in_container_ = 0;
          return;
        case PositionAnchorType::kOffsetInAnchor:
          offset_in_container_ = position.OffsetInContainerNode();
          return;
      }
      NOTREACHED_IN_MIGRATION() << "Invalid PositionAnchorType";
      return;
    case kContainerNode:
    case kUserSelectContainNode:
      container_type_ = kContainerNode;
      switch (position.AnchorType()) {
        case PositionAnchorType::kAfterChildren:
        case PositionAnchorType::kAfterAnchor:
          child_before_position_ = Strategy::LastChild(*container_node_);
          offset_in_container_ = child_before_position_ ? kInvalidOffset : 0;
          container_type_ = kContainerNode;
          return;
        case PositionAnchorType::kBeforeAnchor:
          child_before_position_ = nullptr;
          offset_in_container_ = 0;
          container_type_ = kContainerNode;
          return;
        case PositionAnchorType::kOffsetInAnchor:
          // This takes `O(position.OffsetInContainerNode())`.
          child_before_position_ = position.ComputeNodeBeforePosition();
          offset_in_container_ = position.OffsetInContainerNode();
          container_type_ = kContainerNode;
          return;
      }
      NOTREACHED_IN_MIGRATION()
          << " Invalid PositionAnchorType=" << position.AnchorType();
  }
  NOTREACHED_IN_MIGRATION() << " Invalid container_type_=" << container_type_;
}

template <typename Strategy>
FastPositionIteratorAlgorithm<Strategy>::FastPositionIteratorAlgorithm() =
    default;

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::AssertOffsetInContainerIsValid()
    const {
#if DCHECK_IS_ON()
  switch (container_type_) {
    case kNullNode:
      DCHECK(!child_before_position_);
      DCHECK_EQ(offset_in_container_, kInvalidOffset);
      return;
    case kNoChildren:
      DCHECK(!child_before_position_);
      DCHECK(offset_in_container_ == 0 || offset_in_container_ == 1);
      return;
    case kCharacterData:
    case kTextNode:
      DCHECK(!child_before_position_);
      DCHECK_LE(offset_in_container_,
                To<CharacterData>(container_node_)->length());
      return;
    case kContainerNode:
    case kUserSelectContainNode:
      if (!child_before_position_) {
        DCHECK(!offset_in_container_);
        return;
      }
      if (offset_in_container_ == kInvalidOffset)
        return;
      DCHECK_EQ(offset_in_container_,
                Strategy::Index(*child_before_position_) + 1);
      return;
  }
  NOTREACHED_IN_MIGRATION() << " Invalid container_type_=" << container_type_;
#endif
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::AssertOffsetStackIsValid() const {
#if DCHECK_IS_ON()
  auto it = offset_stack_.begin();
  for (const Node& ancestor : Strategy::AncestorsOf(*container_node_)) {
    if (it == offset_stack_.end())
      break;
    DCHECK_EQ(*it, Strategy::Index(ancestor)) << " " << ancestor;
    ++it;
  }
#endif
}

template <typename Strategy>
bool FastPositionIteratorAlgorithm<Strategy>::IsValid() const {
  if (container_node_ &&
      container_node_->GetDocument().DomTreeVersion() != dom_tree_version_)
    return false;
  AssertOffsetInContainerIsValid();
  return true;
}

template <typename Strategy>
Node* FastPositionIteratorAlgorithm<Strategy>::ChildAfterPosition() const {
  DCHECK(container_type_ == kContainerNode ||
         container_type_ == kUserSelectContainNode);
  return child_before_position_ ? Strategy::NextSibling(*child_before_position_)
                                : Strategy::FirstChild(*container_node_);
}

template <typename Strategy>
bool FastPositionIteratorAlgorithm<Strategy>::HasChildren() const {
  DCHECK(container_node_);
  return Strategy::HasChildren(*container_node_);
}

template <typename Strategy>
bool FastPositionIteratorAlgorithm<Strategy>::IgnoresChildren() const {
  DCHECK(container_node_);
  return EditingIgnoresContent(*container_node_);
}

template <typename Strategy>
bool FastPositionIteratorAlgorithm<Strategy>::IsUserSelectContain() const {
  DCHECK(container_node_);
  return ::blink::IsUserSelectContain(*container_node_);
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::Decrement() {
  AssertOffsetInContainerIsValid();
  DecrementInternal();
  AssertOffsetInContainerIsValid();
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::Increment() {
  AssertOffsetInContainerIsValid();
  IncrementInternal();
  AssertOffsetInContainerIsValid();
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::DecrementInternal() {
  DCHECK(IsValid());
  switch (container_type_) {
    case kNullNode:
      return;
    case kNoChildren:
      if (!offset_in_container_ || !container_node_->GetLayoutObject())
        return MoveToPreviousContainer();
      offset_in_container_ = 0;
      return;
    case kCharacterData:
      return MoveToPreviousContainer();
    case kContainerNode:
      if (!child_before_position_)
        return MoveToPreviousContainer();

      if (IsUserSelectContain()) {
        if (!container_node_->GetLayoutObject())
          return MoveToPreviousContainer();
        if (!ChildAfterPosition()) {
          container_type_ = kUserSelectContainNode;
          return MoveToPreviousSkippingChildren();
        }
        // TODO(crbug.com/1132412): We should move to before children.
      }

      MoveOffsetInContainerBy(-1);
      SetContainer(child_before_position_);
      switch (container_type_) {
        case kNoChildren:
          child_before_position_ = nullptr;
          PushThenSetOffset(IgnoresChildren() ? 1 : 0);
          return;
        case kCharacterData:
        case kTextNode:
          child_before_position_ = nullptr;
          PushThenSetOffset(To<CharacterData>(container_node_)->length());
          return;
        case kContainerNode:
          child_before_position_ = Strategy::LastChild(*container_node_);
          PushThenSetOffset(kInvalidOffset);
          return;
        case kUserSelectContainNode:
          // TODO(crbug.com/1132412): We should move to before children.
          child_before_position_ = Strategy::FirstChild(*container_node_);
          PushThenSetOffset(child_before_position_ ? 1 : 0);
          return;
        case kNullNode:
          NOTREACHED_IN_MIGRATION()
              << " Unexpected container_type_=" << container_type_;
          return;
      }
      NOTREACHED_IN_MIGRATION()
          << " Invalid container_type_=" << container_type_;
      return;

    case kTextNode:
      if (!offset_in_container_)
        return MoveToPreviousContainer();
      offset_in_container_ =
          PreviousGraphemeBoundaryOf(*container_node_, offset_in_container_);
      return;
    case kUserSelectContainNode:
      // TODO(crbug.com/1132412): We should move to next container
      // unconditionally.
      if (!container_node_->GetLayoutObject())
        return MoveToPreviousContainer();
      return MoveToPreviousSkippingChildren();
  }
  NOTREACHED_IN_MIGRATION() << " Invalid container_type_=" << container_type_;
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::IncrementInternal() {
  DCHECK(IsValid());
  switch (container_type_) {
    case kNullNode:
      return;
    case kNoChildren:
      if (offset_in_container_ || !container_node_->GetLayoutObject() ||
          !IgnoresChildren())
        return MoveToNextContainer();
      offset_in_container_ = 1;
      return;
    case kCharacterData:
      return MoveToNextContainer();
    case kContainerNode:
      if (!ChildAfterPosition())
        return MoveToNextContainer();
      MoveOffsetInContainerBy(1);
      child_before_position_ = ChildAfterPosition();
      SetContainer(child_before_position_);
      child_before_position_ = nullptr;
      return PushThenSetOffset(0);
    case kTextNode:
      if (offset_in_container_ == To<Text>(container_node_)->length())
        return MoveToNextContainer();
      offset_in_container_ =
          NextGraphemeBoundaryOf(*container_node_, offset_in_container_);
      return;
    case kUserSelectContainNode:
      // TODO(crbug.com/1132412): We should move to next container
      // unconditionally.
      if (!container_node_->GetLayoutObject())
        return MoveToNextContainer();
      // Note: We should skip to next container after visiting first child,
      // because `LastOffsetForPositionIterator()` returns 1.
      if (child_before_position_ == Strategy::FirstChild(*container_node_))
        return MoveToNextContainer();
      return MoveToNextSkippingChildren();
  }
  NOTREACHED_IN_MIGRATION() << " Invalid container_type_=" << container_type_;
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::MoveToNextContainer() {
  PopOffsetStack();
  child_before_position_ = container_node_;
  SetContainer(SelectableParentOf<Strategy>(*container_node_));
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::MoveToNextSkippingChildren() {
  if (child_before_position_ == Strategy::LastChild(*container_node_)) {
    PopOffsetStack();
    child_before_position_ = container_node_;
    return SetContainer(SelectableParentOf<Strategy>(*container_node_));
  }
  MoveOffsetInContainerBy(1);
  child_before_position_ = ChildAfterPosition();
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::MoveToPreviousContainer() {
  PopOffsetStack();
  SetChildBeforePositionToPreviosuSigblingOf(*container_node_);
  SetContainer(SelectableParentOf<Strategy>(*container_node_));
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::MoveToPreviousSkippingChildren() {
  if (!child_before_position_) {
    PopOffsetStack();
    SetChildBeforePositionToPreviosuSigblingOf(*container_node_);
    return SetContainer(SelectableParentOf<Strategy>(*container_node_));
  }
  MoveOffsetInContainerBy(-1);
  SetChildBeforePositionToPreviosuSigblingOf(*child_before_position_);
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<
    Strategy>::SetChildBeforePositionToPreviosuSigblingOf(const Node& node) {
  child_before_position_ = Strategy::PreviousSibling(node);
  if (child_before_position_) {
    DCHECK(offset_in_container_);
    return;
  }
  DCHECK(offset_in_container_ == kInvalidOffset || !offset_in_container_);
  offset_in_container_ = 0;
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::SetContainer(Node* node) {
  container_node_ = node;
  container_type_ = ContainerToContainerType(node);
  if (container_type_ == kNullNode) {
    child_before_position_ = nullptr;
    offset_in_container_ = kInvalidOffset;
    container_type_ = kNullNode;
  }
}

template <typename Strategy>
PositionTemplate<Strategy>
FastPositionIteratorAlgorithm<Strategy>::BeforeOrAfterPosition() const {
  DCHECK(IsValid());
  return IsBeforePosition() ? PositionType::BeforeNode(*container_node_)
                            : PositionType::AfterNode(*container_node_);
}

template <typename Strategy>
bool FastPositionIteratorAlgorithm<Strategy>::IsBeforePosition() const {
  DCHECK(IsValid());
  switch (container_type_) {
    case kNullNode:
    case kTextNode:
      NOTREACHED_IN_MIGRATION()
          << " Unexpected container_type_=" << container_type_;
      return false;
    case kNoChildren:
    case kCharacterData:
    case kUserSelectContainNode:
      return !offset_in_container_;
    case kContainerNode:
      return !child_before_position_;
  }
  NOTREACHED_IN_MIGRATION() << " Invalid container_type_=" << container_type_;
  return false;
}

template <typename Strategy>
PositionTemplate<Strategy>
FastPositionIteratorAlgorithm<Strategy>::DeprecatedComputePosition() const {
  DCHECK(IsValid());
  switch (container_type_) {
    case kNullNode:
      return PositionType();
    case kNoChildren:
      if (IgnoresChildren())
        return BeforeOrAfterPosition();
      DCHECK(!offset_in_container_);
      return PositionType(*container_node_, 0);
    case kCharacterData:
      if (IsA<Text>(*container_node_))
        return PositionType(*container_node_, offset_in_container_);
      return BeforeOrAfterPosition();
    case kContainerNode:
      if (Node* child_after_position = ChildAfterPosition()) {
        if (EditingIgnoresContent(*Strategy::Parent(*child_after_position)))
          return PositionType::BeforeNode(*container_node_);
        EnsureOffsetInContainer();
        return PositionType(*container_node_, offset_in_container_);
      }
      return PositionType::LastPositionInOrAfterNode(*container_node_);
    case kTextNode:
      return PositionType(*container_node_, offset_in_container_);
    case kUserSelectContainNode:
      return PositionType::LastPositionInOrAfterNode(*container_node_);
  }
  NOTREACHED_IN_MIGRATION() << " Invalid container_type_=" << container_type_;
}

template <typename Strategy>
PositionTemplate<Strategy>
FastPositionIteratorAlgorithm<Strategy>::ComputePosition() const {
  DCHECK(IsValid());
  switch (container_type_) {
    case kNullNode:
      return PositionType();
    case kNoChildren:
      return BeforeOrAfterPosition();
    case kCharacterData:
      if (IsA<Text>(*container_node_))
        return PositionType(*container_node_, offset_in_container_);
      return BeforeOrAfterPosition();
    case kContainerNode:
      if (ChildAfterPosition()) {
        EnsureOffsetInContainer();
        return PositionType(*container_node_, offset_in_container_);
      }
      if (IsUserSelectContain())
        return BeforeOrAfterPosition();
      return PositionType::LastPositionInOrAfterNode(*container_node_);
    case kTextNode:
      return PositionType(*container_node_, offset_in_container_);
    case kUserSelectContainNode:
      return BeforeOrAfterPosition();
  }
  NOTREACHED_IN_MIGRATION() << " Invalid container_type_=" << container_type_;
}

template <typename Strategy>
int FastPositionIteratorAlgorithm<Strategy>::OffsetInTextNode() const {
  DCHECK(IsValid());
  //`VisiblePositionTest.PlaceholderBRWithCollapsedSpace` calls this function
  // with `kCharacterData`.
  DCHECK(container_type_ == kTextNode || container_type_ == kCharacterData)
      << container_type_;
  DCHECK(IsA<Text>(container_node_)) << container_node_;
  return base::saturated_cast<int>(offset_in_container_);
}

template <typename Strategy>
bool FastPositionIteratorAlgorithm<Strategy>::AtStart() const {
  DCHECK(IsValid());
  if (!container_node_)
    return true;
  return !Strategy::Parent(*container_node_) && AtStartOfNode();
}

template <typename Strategy>
bool FastPositionIteratorAlgorithm<Strategy>::AtEnd() const {
  DCHECK(IsValid());
  if (!container_node_)
    return true;
  return !Strategy::Parent(*container_node_) && AtEndOfNode();
}

template <typename Strategy>
bool FastPositionIteratorAlgorithm<Strategy>::AtStartOfNode() const {
  DCHECK(IsValid());
  switch (container_type_) {
    case kNullNode:
      return true;
    case kContainerNode:
      return !child_before_position_;
    case kNoChildren:
    case kCharacterData:
    case kTextNode:
      return !offset_in_container_;
    case kUserSelectContainNode:
      return !child_before_position_;
  }
  NOTREACHED_IN_MIGRATION() << " Invalid container_type_=" << container_type_;
}

template <typename Strategy>
bool FastPositionIteratorAlgorithm<Strategy>::AtEndOfNode() const {
  DCHECK(IsValid());
  switch (container_type_) {
    case kNullNode:
      return true;
    case kContainerNode:
      return !ChildAfterPosition();
    case kNoChildren:
      return !IgnoresChildren() || offset_in_container_;
    case kCharacterData:
    case kTextNode:
      return offset_in_container_ ==
             To<CharacterData>(container_node_)->length();
    case kUserSelectContainNode:
      return HasChildren() || !ChildAfterPosition();
  }
  NOTREACHED_IN_MIGRATION() << " Invalid container_type_=" << container_type_;
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::EnsureOffsetInContainer() const {
  DCHECK(container_type_ == kContainerNode ||
         container_type_ == kUserSelectContainNode);
  if (offset_in_container_ != kInvalidOffset)
    return;
  offset_in_container_ = Strategy::Index(*child_before_position_) + 1;
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::MoveOffsetInContainerBy(
    int delta) {
  DCHECK(delta == 1 || delta == -1) << delta;
  if (offset_in_container_ == kInvalidOffset)
    return;
  offset_in_container_ += delta;
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::PopOffsetStack() {
  if (offset_stack_.empty()) {
    offset_in_container_ = kInvalidOffset;
    return;
  }
  offset_in_container_ = offset_stack_.back();
  offset_stack_.pop_back();
}

template <typename Strategy>
void FastPositionIteratorAlgorithm<Strategy>::PushThenSetOffset(
    unsigned offset_in_container) {
  offset_stack_.push_back(offset_in_container_);
  offset_in_container_ = offset_in_container;
  AssertOffsetInContainerIsValid();
}

template class CORE_TEMPLATE_EXPORT
    FastPositionIteratorAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    FastPositionIteratorAlgorithm<EditingInFlatTreeStrategy>;

// ---

template <typename Strategy>
PositionIteratorAlgorithm<Strategy>::PositionIteratorAlgorithm(
    const PositionTemplate<Strategy>& position)
    : fast_(!RuntimeEnabledFeatures::FastPositionIteratorEnabled()
                ? PositionTemplate<Strategy>()
                : position),

      slow_(RuntimeEnabledFeatures::FastPositionIteratorEnabled()
                ? PositionTemplate<Strategy>()
                : position) {}

template <typename Strategy>
PositionIteratorAlgorithm<Strategy>::PositionIteratorAlgorithm(
    const PositionIteratorAlgorithm& other)
    : fast_(other.fast_), slow_(other.slow_) {}

template <typename Strategy>
PositionIteratorAlgorithm<Strategy>&
PositionIteratorAlgorithm<Strategy>::operator=(
    const PositionIteratorAlgorithm& other) {
  fast_ = other.fast_;
  slow_ = other.slow_;
  return *this;
}

template <typename Strategy>
PositionTemplate<Strategy>
PositionIteratorAlgorithm<Strategy>::DeprecatedComputePosition() const {
  if (!RuntimeEnabledFeatures::FastPositionIteratorEnabled())
    return slow_.DeprecatedComputePosition();
  return fast_.DeprecatedComputePosition();
}

template <typename Strategy>
PositionTemplate<Strategy>
PositionIteratorAlgorithm<Strategy>::ComputePosition() const {
  if (!RuntimeEnabledFeatures::FastPositionIteratorEnabled())
    return slow_.ComputePosition();
  return fast_.ComputePosition();
}

template <typename Strategy>
void PositionIteratorAlgorithm<Strategy>::Decrement() {
  if (!RuntimeEnabledFeatures::FastPositionIteratorEnabled())
    return slow_.Decrement();
  fast_.Decrement();
}

template <typename Strategy>
void PositionIteratorAlgorithm<Strategy>::Increment() {
  if (!RuntimeEnabledFeatures::FastPositionIteratorEnabled())
    return slow_.Increment();
  fast_.Increment();
}

template <typename Strategy>
Node* PositionIteratorAlgorithm<Strategy>::GetNode() const {
  if (!RuntimeEnabledFeatures::FastPositionIteratorEnabled())
    return slow_.GetNode();
  return fast_.GetNode();
}

template <typename Strategy>
int PositionIteratorAlgorithm<Strategy>::OffsetInTextNode() const {
  if (!RuntimeEnabledFeatures::FastPositionIteratorEnabled())
    return slow_.OffsetInTextNode();
  return fast_.OffsetInTextNode();
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::AtStart() const {
  if (!RuntimeEnabledFeatures::FastPositionIteratorEnabled())
    return slow_.AtStart();
  return fast_.AtStart();
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::AtEnd() const {
  if (!RuntimeEnabledFeatures::FastPositionIteratorEnabled())
    return slow_.AtEnd();
  return fast_.AtEnd();
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::AtStartOfNode() const {
  if (!RuntimeEnabledFeatures::FastPositionIteratorEnabled())
    return slow_.AtStartOfNode();
  return fast_.AtStartOfNode();
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::AtEndOfNode() const {
  if (!RuntimeEnabledFeatures::FastPositionIteratorEnabled())
    return slow_.AtEndOfNode();
  return fast_.AtEndOfNode();
}

template class CORE_TEMPLATE_EXPORT PositionIteratorAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    PositionIteratorAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink
