/*
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/position.h"

#include <stdio.h>
#include <ostream>  // NOLINT
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

#if DCHECK_IS_ON()
template <typename Strategy>
static bool CanBeAnchorNode(Node*);

template <>
bool CanBeAnchorNode<EditingStrategy>(Node* node) {
  return !node || !node->IsPseudoElement();
}

template <>
bool CanBeAnchorNode<EditingInFlatTreeStrategy>(Node* node) {
  return CanBeAnchorNode<EditingStrategy>(node) &&
         node->CanParticipateInFlatTree();
}
#endif

template <typename Strategy>
void PositionTemplate<Strategy>::Trace(Visitor* visitor) {
  visitor->Trace(anchor_node_);
}

template <typename Strategy>
const TreeScope* PositionTemplate<Strategy>::CommonAncestorTreeScope(
    const PositionTemplate<Strategy>& a,
    const PositionTemplate<Strategy>& b) {
  if (!a.ComputeContainerNode() || !b.ComputeContainerNode())
    return nullptr;
  return a.ComputeContainerNode()->GetTreeScope().CommonAncestorTreeScope(
      b.ComputeContainerNode()->GetTreeScope());
}

template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::EditingPositionOf(
    const Node* anchor_node,
    int offset) {
  if (!anchor_node || anchor_node->IsTextNode())
    return PositionTemplate<Strategy>(anchor_node, offset);

  if (!EditingIgnoresContent(*anchor_node)) {
    return PositionTemplate<Strategy>::CreateWithoutValidationDeprecated(
        *anchor_node, offset);
  }

  if (offset == 0)
    return PositionTemplate<Strategy>(anchor_node,
                                      PositionAnchorType::kBeforeAnchor);

  // Note: |offset| can be >= 1, if |anchorNode| have child nodes, e.g.
  // using Node.appendChild() to add a child node TEXTAREA.
  DCHECK_GE(offset, 1);
  return PositionTemplate<Strategy>(anchor_node,
                                    PositionAnchorType::kAfterAnchor);
}

// TODO(editing-dev): Once we change type of |anchor_node_| to
// |Member<const Node>|, we should get rid of |const_cast<Node*>()|.
// See http://crbug.com/735327
template <typename Strategy>
PositionTemplate<Strategy>::PositionTemplate(const Node* anchor_node,
                                             PositionAnchorType anchor_type)
    : anchor_node_(const_cast<Node*>(anchor_node)),
      offset_(0),
      anchor_type_(anchor_type) {
#if DCHECK_IS_ON()
  DCHECK(anchor_node_);
  DCHECK_NE(anchor_type_, PositionAnchorType::kOffsetInAnchor);
  DCHECK(CanBeAnchorNode<Strategy>(anchor_node_.Get())) << anchor_node_;
  if (anchor_node_->IsTextNode()) {
    DCHECK(anchor_type_ == PositionAnchorType::kBeforeAnchor ||
           anchor_type_ == PositionAnchorType::kAfterAnchor)
        << *this;
    return;
  }
  if (!Strategy::Parent(*anchor_node_)) {
    // Before/After |anchor_node_| should have a parent node for converting
    // to offset in anchor position.
    DCHECK(IsBeforeChildren() || IsAfterChildren()) << *this;
    return;
  }
#endif
}

// TODO(editing-dev): Once we change type of |anchor_node_| to
// |Member<const Node>|, we should get rid of |const_cast<Node*>()|.
// See http://crbug.com/735327
template <typename Strategy>
PositionTemplate<Strategy>::PositionTemplate(const Node* anchor_node,
                                             int offset)
    : anchor_node_(const_cast<Node*>(anchor_node)),
      offset_(offset),
      anchor_type_(PositionAnchorType::kOffsetInAnchor) {
#if DCHECK_IS_ON()
  DCHECK(CanBeAnchorNode<Strategy>(anchor_node_.Get())) << anchor_node_;
  if (!anchor_node_) {
    DCHECK_EQ(offset, 0);
    return;
  }
  if (auto* data = DynamicTo<CharacterData>(anchor_node_.Get())) {
    DCHECK_GE(offset, 0);
    DCHECK_LE(static_cast<unsigned>(offset), data->length()) << anchor_node_;
    return;
  }
  DCHECK_GE(offset, 0);
  DCHECK_LE(static_cast<unsigned>(offset),
            Strategy::CountChildren(*anchor_node))
      << anchor_node_;
#endif
}

template <typename Strategy>
PositionTemplate<Strategy>::PositionTemplate(const Node& anchor_node,
                                             int offset)
    : PositionTemplate(&anchor_node, offset) {}

template <typename Strategy>
PositionTemplate<Strategy>::PositionTemplate(const PositionTemplate& other)
    : anchor_node_(other.anchor_node_),
      offset_(other.offset_),
      anchor_type_(other.anchor_type_) {}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::CreateWithoutValidation(
    const Node& container,
    int offset) {
  PositionTemplate<Strategy> result(container, 0);
  result.offset_ = offset;
  return result;
}

// static
template <typename Strategy>
PositionTemplate<Strategy>
PositionTemplate<Strategy>::CreateWithoutValidationDeprecated(
    const Node& container,
    int offset) {
  return CreateWithoutValidation(container, offset);
}

// --

template <typename Strategy>
Node* PositionTemplate<Strategy>::ComputeContainerNode() const {
  if (!anchor_node_)
    return nullptr;

  switch (AnchorType()) {
    case PositionAnchorType::kBeforeChildren:
    case PositionAnchorType::kAfterChildren:
    case PositionAnchorType::kOffsetInAnchor:
      return anchor_node_.Get();
    case PositionAnchorType::kBeforeAnchor:
    case PositionAnchorType::kAfterAnchor: {
      Node* const parent = Strategy::Parent(*anchor_node_);
      // TODO(https://crbug.com/889737), Once we fix the issue, we should have
      // |DCHECK(parent)|.
      return parent;
    }
  }
  NOTREACHED();
  return nullptr;
}

template <typename Strategy>
static int MinOffsetForNode(Node* anchor_node, int offset) {
  if (auto* data = DynamicTo<CharacterData>(anchor_node))
    return std::min(offset, static_cast<int>(data->length()));

  int new_offset = 0;
  for (Node* node = Strategy::FirstChild(*anchor_node);
       node && new_offset < offset; node = Strategy::NextSibling(*node))
    new_offset++;

  return new_offset;
}

template <typename Strategy>
int PositionTemplate<Strategy>::ComputeOffsetInContainerNode() const {
  if (!anchor_node_)
    return 0;

  switch (AnchorType()) {
    case PositionAnchorType::kBeforeChildren:
      return 0;
    case PositionAnchorType::kAfterChildren:
      return LastOffsetInNode(*anchor_node_);
    case PositionAnchorType::kOffsetInAnchor:
      return MinOffsetForNode<Strategy>(anchor_node_.Get(), offset_);
    case PositionAnchorType::kBeforeAnchor:
      return Strategy::Index(*anchor_node_);
    case PositionAnchorType::kAfterAnchor:
      return Strategy::Index(*anchor_node_) + 1;
  }
  NOTREACHED();
  return 0;
}

// Neighbor-anchored positions are invalid DOM positions, so they need to be
// fixed up before handing them off to the Range object.
template <typename Strategy>
PositionTemplate<Strategy>
PositionTemplate<Strategy>::ParentAnchoredEquivalent() const {
  if (!anchor_node_)
    return PositionTemplate<Strategy>();

  // FIXME: This should only be necessary for legacy positions, but is also
  // needed for positions before and after Tables
  if (offset_ == 0 && !IsAfterAnchorOrAfterChildren()) {
    if (Strategy::Parent(*anchor_node_) &&
        (EditingIgnoresContent(*anchor_node_) ||
         IsDisplayInsideTable(anchor_node_.Get())))
      return InParentBeforeNode(*anchor_node_);
    return PositionTemplate<Strategy>(anchor_node_.Get(), 0);
  }
  if (!anchor_node_->IsCharacterDataNode() &&
      (IsAfterAnchorOrAfterChildren() ||
       static_cast<unsigned>(offset_) == anchor_node_->CountChildren()) &&
      (EditingIgnoresContent(*anchor_node_) ||
       IsDisplayInsideTable(anchor_node_.Get())) &&
      ComputeContainerNode()) {
    return InParentAfterNode(*anchor_node_);
  }

  return PositionTemplate<Strategy>(ComputeContainerNode(),
                                    ComputeOffsetInContainerNode());
}

template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::ToOffsetInAnchor()
    const {
  if (IsNull())
    return PositionTemplate<Strategy>();

  return PositionTemplate<Strategy>(ComputeContainerNode(),
                                    ComputeOffsetInContainerNode());
}

template <typename Strategy>
int PositionTemplate<Strategy>::ComputeEditingOffset() const {
  if (IsAfterAnchorOrAfterChildren())
    return Strategy::LastOffsetForEditing(anchor_node_.Get());
  return offset_;
}

template <typename Strategy>
Node* PositionTemplate<Strategy>::ComputeNodeBeforePosition() const {
  if (!anchor_node_)
    return nullptr;
  switch (AnchorType()) {
    case PositionAnchorType::kBeforeChildren:
      return nullptr;
    case PositionAnchorType::kAfterChildren:
      return Strategy::LastChild(*anchor_node_);
    case PositionAnchorType::kOffsetInAnchor:
      return offset_ ? Strategy::ChildAt(*anchor_node_, offset_ - 1) : nullptr;
    case PositionAnchorType::kBeforeAnchor:
      return Strategy::PreviousSibling(*anchor_node_);
    case PositionAnchorType::kAfterAnchor:
      return anchor_node_.Get();
  }
  NOTREACHED();
  return nullptr;
}

template <typename Strategy>
Node* PositionTemplate<Strategy>::ComputeNodeAfterPosition() const {
  if (!anchor_node_)
    return nullptr;

  switch (AnchorType()) {
    case PositionAnchorType::kBeforeChildren:
      return Strategy::FirstChild(*anchor_node_);
    case PositionAnchorType::kAfterChildren:
      return nullptr;
    case PositionAnchorType::kOffsetInAnchor:
      return Strategy::ChildAt(*anchor_node_, offset_);
    case PositionAnchorType::kBeforeAnchor:
      return anchor_node_.Get();
    case PositionAnchorType::kAfterAnchor:
      return Strategy::NextSibling(*anchor_node_);
  }
  NOTREACHED();
  return nullptr;
}

// An implementation of |Range::firstNode()|.
template <typename Strategy>
Node* PositionTemplate<Strategy>::NodeAsRangeFirstNode() const {
  if (!anchor_node_)
    return nullptr;
  if (!IsOffsetInAnchor())
    return ToOffsetInAnchor().NodeAsRangeFirstNode();
  if (anchor_node_->IsCharacterDataNode())
    return anchor_node_.Get();
  if (Node* child = Strategy::ChildAt(*anchor_node_, offset_))
    return child;
  if (!offset_)
    return anchor_node_.Get();
  return Strategy::NextSkippingChildren(*anchor_node_);
}

template <typename Strategy>
Node* PositionTemplate<Strategy>::NodeAsRangeLastNode() const {
  if (IsNull())
    return nullptr;
  if (Node* past_last_node = NodeAsRangePastLastNode())
    return Strategy::Previous(*past_last_node);
  return &Strategy::LastWithinOrSelf(*ComputeContainerNode());
}

// An implementation of |Range::pastLastNode()|.
template <typename Strategy>
Node* PositionTemplate<Strategy>::NodeAsRangePastLastNode() const {
  if (!anchor_node_)
    return nullptr;
  if (!IsOffsetInAnchor())
    return ToOffsetInAnchor().NodeAsRangePastLastNode();
  if (anchor_node_->IsCharacterDataNode())
    return Strategy::NextSkippingChildren(*anchor_node_);
  if (Node* child = Strategy::ChildAt(*anchor_node_, offset_))
    return child;
  return Strategy::NextSkippingChildren(*anchor_node_);
}

template <typename Strategy>
Node* PositionTemplate<Strategy>::CommonAncestorContainer(
    const PositionTemplate<Strategy>& other) const {
  return Strategy::CommonAncestor(*ComputeContainerNode(),
                                  *other.ComputeContainerNode());
}

static bool IsPositionConnected(const Position& position) {
  return position.AnchorNode() && position.AnchorNode()->isConnected();
}

static bool IsPositionConnected(const PositionInFlatTree& position) {
  if (position.IsNull())
    return false;
  return FlatTreeTraversal::Contains(*position.GetDocument(),
                                     *position.AnchorNode());
}

template <typename Strategy>
bool PositionTemplate<Strategy>::IsConnected() const {
  return IsPositionConnected(*this);
}

template <typename Strategy>
bool PositionTemplate<Strategy>::IsValidFor(const Document& document) const {
  if (IsNull())
    return true;
  if (GetDocument() != document)
    return false;
  if (!IsConnected())
    return false;
  return !IsOffsetInAnchor() ||
         OffsetInContainerNode() <= LastOffsetInNode(*AnchorNode());
}

int16_t ComparePositions(const PositionInFlatTree& position_a,
                         const PositionInFlatTree& position_b) {
  DCHECK(position_a.IsNotNull());
  DCHECK(position_b.IsNotNull());

  position_a.AnchorNode()->UpdateDistributionForFlatTreeTraversal();
  Node* container_a = position_a.ComputeContainerNode();
  position_b.AnchorNode()->UpdateDistributionForFlatTreeTraversal();
  Node* container_b = position_b.ComputeContainerNode();
  int offset_a = position_a.ComputeOffsetInContainerNode();
  int offset_b = position_b.ComputeOffsetInContainerNode();
  return ComparePositionsInFlatTree(container_a, offset_a, container_b,
                                    offset_b);
}

template <typename Strategy>
int16_t PositionTemplate<Strategy>::CompareTo(
    const PositionTemplate<Strategy>& other) const {
  return ComparePositions(*this, other);
}

template <typename Strategy>
bool PositionTemplate<Strategy>::operator<(
    const PositionTemplate<Strategy>& other) const {
  return ComparePositions(*this, other) < 0;
}

template <typename Strategy>
bool PositionTemplate<Strategy>::operator<=(
    const PositionTemplate<Strategy>& other) const {
  return ComparePositions(*this, other) <= 0;
}

template <typename Strategy>
bool PositionTemplate<Strategy>::operator>(
    const PositionTemplate<Strategy>& other) const {
  return ComparePositions(*this, other) > 0;
}

template <typename Strategy>
bool PositionTemplate<Strategy>::operator>=(
    const PositionTemplate<Strategy>& other) const {
  return ComparePositions(*this, other) >= 0;
}

template <typename Strategy>
bool PositionTemplate<Strategy>::IsEquivalent(
    const PositionTemplate<Strategy>& other) const {
  if (IsNull())
    return other.IsNull();
  if (anchor_type_ == other.anchor_type_)
    return *this == other;
  return ToOffsetInAnchor() == other.ToOffsetInAnchor();
}

template <typename Strategy>
bool PositionTemplate<Strategy>::AtFirstEditingPositionForNode() const {
  if (IsNull())
    return true;
  // FIXME: Position before anchor shouldn't be considered as at the first
  // editing position for node since that position resides outside of the node.
  switch (anchor_type_) {
    case PositionAnchorType::kOffsetInAnchor:
      return offset_ == 0;
    case PositionAnchorType::kBeforeChildren:
    case PositionAnchorType::kBeforeAnchor:
      return true;
    case PositionAnchorType::kAfterChildren:
    case PositionAnchorType::kAfterAnchor:
      // TODO(yosin) We should use |Strategy::lastOffsetForEditing()| instead
      // of DOM tree version.
      return !EditingStrategy::LastOffsetForEditing(AnchorNode());
  }
  NOTREACHED();
  return false;
}

template <typename Strategy>
bool PositionTemplate<Strategy>::AtLastEditingPositionForNode() const {
  if (IsNull())
    return true;
  // TODO(yosin): Position after anchor shouldn't be considered as at the
  // first editing position for node since that position resides outside of
  // the node.
  // TODO(yosin) We should use |Strategy::lastOffsetForEditing()| instead of
  // DOM tree version.
  return IsAfterAnchorOrAfterChildren() ||
         offset_ >= EditingStrategy::LastOffsetForEditing(AnchorNode());
}

template <typename Strategy>
bool PositionTemplate<Strategy>::AtStartOfTree() const {
  if (IsNull())
    return true;
  return !Strategy::Parent(*AnchorNode()) && offset_ == 0;
}

template <typename Strategy>
bool PositionTemplate<Strategy>::AtEndOfTree() const {
  if (IsNull())
    return true;
  // TODO(yosin) We should use |Strategy::lastOffsetForEditing()| instead of
  // DOM tree version.
  return !Strategy::Parent(*AnchorNode()) &&
         offset_ >= EditingStrategy::LastOffsetForEditing(AnchorNode());
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::InParentBeforeNode(
    const Node& node) {
  // FIXME: This should DCHECK(node.parentNode()). At least one caller currently
  // hits this DCHECK though, which indicates that the caller is trying to make
  // a position relative to a disconnected node (which is likely an error)
  // Specifically, editing/deleting/delete-ligature-001.html crashes with
  // DCHECK(node->parentNode())
  return PositionTemplate<Strategy>(Strategy::Parent(node),
                                    Strategy::Index(node));
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::InParentAfterNode(
    const Node& node) {
  DCHECK(node.parentNode()) << node;
  return PositionTemplate<Strategy>(Strategy::Parent(node),
                                    Strategy::Index(node) + 1);
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::BeforeNode(
    const Node& anchor_node) {
  return PositionTemplate<Strategy>(&anchor_node,
                                    PositionAnchorType::kBeforeAnchor);
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::AfterNode(
    const Node& anchor_node) {
  return PositionTemplate<Strategy>(&anchor_node,
                                    PositionAnchorType::kAfterAnchor);
}

// static
template <typename Strategy>
int PositionTemplate<Strategy>::LastOffsetInNode(const Node& node) {
  if (auto* data = DynamicTo<CharacterData>(node))
    return static_cast<int>(data->length());

  return static_cast<int>(Strategy::CountChildren(node));
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::FirstPositionInNode(
    const Node& anchor_node) {
  if (anchor_node.IsTextNode())
    return PositionTemplate<Strategy>(anchor_node, 0);
  return PositionTemplate<Strategy>(&anchor_node,
                                    PositionAnchorType::kBeforeChildren);
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::LastPositionInNode(
    const Node& anchor_node) {
  if (anchor_node.IsTextNode()) {
    return PositionTemplate<Strategy>(anchor_node,
                                      LastOffsetInNode(anchor_node));
  }
  return PositionTemplate<Strategy>(&anchor_node,
                                    PositionAnchorType::kAfterChildren);
}

// static
template <typename Strategy>
PositionTemplate<Strategy>
PositionTemplate<Strategy>::FirstPositionInOrBeforeNode(const Node& node) {
  return EditingIgnoresContent(node) ? BeforeNode(node)
                                     : FirstPositionInNode(node);
}

// static
template <typename Strategy>
PositionTemplate<Strategy>
PositionTemplate<Strategy>::LastPositionInOrAfterNode(const Node& node) {
  return EditingIgnoresContent(node) ? AfterNode(node)
                                     : LastPositionInNode(node);
}

PositionInFlatTree ToPositionInFlatTree(const Position& pos) {
  if (pos.IsNull())
    return PositionInFlatTree();

  Node* const anchor = pos.AnchorNode();
  if (pos.IsOffsetInAnchor()) {
    if (anchor->IsCharacterDataNode())
      return PositionInFlatTree(anchor, pos.ComputeOffsetInContainerNode());
    DCHECK(!anchor->IsElementNode() || anchor->CanParticipateInFlatTree());
    int offset = pos.ComputeOffsetInContainerNode();
    Node* child = NodeTraversal::ChildAt(*anchor, offset);
    if (!child) {
      if (anchor->IsShadowRoot())
        return PositionInFlatTree(anchor->OwnerShadowHost(),
                                  PositionAnchorType::kAfterChildren);
      return PositionInFlatTree(anchor, PositionAnchorType::kAfterChildren);
    }
    child->UpdateDistributionForFlatTreeTraversal();
    if (!child->CanParticipateInFlatTree()) {
      if (anchor->IsShadowRoot())
        return PositionInFlatTree(anchor->OwnerShadowHost(), offset);
      return PositionInFlatTree(anchor, offset);
    }
    if (Node* parent = FlatTreeTraversal::Parent(*child))
      return PositionInFlatTree(parent, FlatTreeTraversal::Index(*child));
    // When |pos| isn't appeared in flat tree, we map |pos| to after
    // children of shadow host.
    // e.g. "foo",0 in <progress>foo</progress>
    if (anchor->IsShadowRoot())
      return PositionInFlatTree(anchor->OwnerShadowHost(),
                                PositionAnchorType::kAfterChildren);
    return PositionInFlatTree(anchor, PositionAnchorType::kAfterChildren);
  }

  if (anchor->IsShadowRoot())
    return PositionInFlatTree(anchor->OwnerShadowHost(), pos.AnchorType());
  if (pos.IsBeforeAnchor() || pos.IsAfterAnchor()) {
    if (anchor->CanParticipateInFlatTree() &&
        !FlatTreeTraversal::Parent(*anchor)) {
      // For Before/AfterAnchor, if |anchor| doesn't have parent in the flat
      // tree, there is no valid corresponding PositionInFlatTree.
      // Since this function is a primitive function, we do not adjust |pos|
      // to somewhere else in flat tree.
      // Reached by unit test
      // FrameSelectionTest.SelectInvalidPositionInFlatTreeDoesntCrash.
      return PositionInFlatTree();
    }
  }
  // TODO(yosin): Once we have a test case for SLOT or active insertion point,
  // this function should handle it.
  return PositionInFlatTree(anchor, pos.AnchorType());
}

Position ToPositionInDOMTree(const Position& position) {
  return position;
}

Position ToPositionInDOMTree(const PositionInFlatTree& position) {
  if (position.IsNull())
    return Position();

  Node* anchor_node = position.AnchorNode();

  switch (position.AnchorType()) {
    case PositionAnchorType::kAfterChildren:
      // FIXME: When anchorNode is <img>, assertion fails in the constructor.
      return Position(anchor_node, PositionAnchorType::kAfterChildren);
    case PositionAnchorType::kAfterAnchor:
      return Position::AfterNode(*anchor_node);
    case PositionAnchorType::kBeforeChildren:
      return Position(anchor_node, PositionAnchorType::kBeforeChildren);
    case PositionAnchorType::kBeforeAnchor:
      return Position::BeforeNode(*anchor_node);
    case PositionAnchorType::kOffsetInAnchor: {
      int offset = position.OffsetInContainerNode();
      if (anchor_node->IsCharacterDataNode())
        return Position(anchor_node, offset);
      Node* child = FlatTreeTraversal::ChildAt(*anchor_node, offset);
      if (child)
        return Position(child->parentNode(), child->NodeIndex());
      if (!position.OffsetInContainerNode())
        return Position(anchor_node, PositionAnchorType::kBeforeChildren);

      // |child| is null when the position is at the end of the children.
      // <div>foo|</div>
      return Position(anchor_node, PositionAnchorType::kAfterChildren);
    }
    default:
      NOTREACHED();
      return Position();
  }
}

template <typename Strategy>
String PositionTemplate<Strategy>::ToAnchorTypeAndOffsetString() const {
  switch (AnchorType()) {
    case PositionAnchorType::kOffsetInAnchor: {
      StringBuilder builder;
      builder.Append("offsetInAnchor[");
      builder.AppendNumber(offset_);
      builder.Append("]");
      return builder.ToString();
    }
    case PositionAnchorType::kBeforeChildren:
      return "beforeChildren";
    case PositionAnchorType::kAfterChildren:
      return "afterChildren";
    case PositionAnchorType::kBeforeAnchor:
      return "beforeAnchor";
    case PositionAnchorType::kAfterAnchor:
      return "afterAnchor";
  }
  NOTREACHED();
  return g_empty_string;
}

#if DCHECK_IS_ON()

template <typename Strategy>
void PositionTemplate<Strategy>::ShowTreeForThis() const {
  if (!AnchorNode()) {
    LOG(INFO) << "\nposition is null";
    return;
  }
  LOG(INFO) << "\n"
            << AnchorNode()->ToTreeStringForThis().Utf8()
            << ToAnchorTypeAndOffsetString().Utf8();
}

template <typename Strategy>
void PositionTemplate<Strategy>::ShowTreeForThisInFlatTree() const {
  if (!AnchorNode()) {
    LOG(INFO) << "\nposition is null";
    return;
  }
  LOG(INFO) << "\n"
            << AnchorNode()->ToFlatTreeStringForThis().Utf8()
            << ToAnchorTypeAndOffsetString().Utf8();
}

#endif  // DCHECK_IS_ON()

template <typename PositionType>
static std::ostream& PrintPosition(std::ostream& ostream,
                                   const PositionType& position) {
  if (position.IsNull())
    return ostream << "null";
  return ostream << position.AnchorNode() << "@"
                 << position.ToAnchorTypeAndOffsetString().Utf8();
}

std::ostream& operator<<(std::ostream& ostream,
                         PositionAnchorType anchor_type) {
  switch (anchor_type) {
    case PositionAnchorType::kAfterAnchor:
      return ostream << "afterAnchor";
    case PositionAnchorType::kAfterChildren:
      return ostream << "afterChildren";
    case PositionAnchorType::kBeforeAnchor:
      return ostream << "beforeAnchor";
    case PositionAnchorType::kBeforeChildren:
      return ostream << "beforeChildren";
    case PositionAnchorType::kOffsetInAnchor:
      return ostream << "offsetInAnchor";
  }
  NOTREACHED();
  return ostream << "anchorType=" << static_cast<int>(anchor_type);
}

std::ostream& operator<<(std::ostream& ostream, const Position& position) {
  return PrintPosition(ostream, position);
}

std::ostream& operator<<(std::ostream& ostream,
                         const PositionInFlatTree& position) {
  return PrintPosition(ostream, position);
}

template class CORE_TEMPLATE_EXPORT PositionTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT PositionTemplate<EditingInFlatTreeStrategy>;

}  // namespace blink

#if DCHECK_IS_ON()

void showTree(const blink::Position& pos) {
  pos.ShowTreeForThis();
}

void showTree(const blink::Position* pos) {
  if (pos)
    pos->ShowTreeForThis();
  else
    LOG(INFO) << "Cannot showTree for <null>";
}

#endif
