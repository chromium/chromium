// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"

#include <ostream>  // NOLINT
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"

namespace blink {

namespace {
template <typename Strategy>
Node* CommonAncestorContainerNode(const Node* container_a,
                                  const Node* container_b) {
  if (!container_a || !container_b)
    return nullptr;
  return Strategy::CommonAncestor(*container_a, *container_b);
}
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end)
    : start_position_(start),
      end_position_(end)
#if DCHECK_IS_ON()
      ,
      dom_tree_version_(start.IsNull() ? 0
                                       : start.GetDocument()->DomTreeVersion())
#endif
{
  if (start_position_.IsNull()) {
    DCHECK(end_position_.IsNull());
    return;
  }
  DCHECK(end_position_.IsNotNull());
  DCHECK(start_position_.IsValidFor(*start_position_.GetDocument()));
  DCHECK(end_position_.IsValidFor(*end_position_.GetDocument()));
  DCHECK_EQ(start_position_.GetDocument(), end_position_.GetDocument());
  DCHECK_LE(start_position_, end_position_);
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate(
    const EphemeralRangeTemplate<Strategy>& other)
    : EphemeralRangeTemplate(other.start_position_, other.end_position_) {
  DCHECK(other.IsValid());
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate(
    const PositionTemplate<Strategy>& position)
    : EphemeralRangeTemplate(position, position) {}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate(const Range* range) {
  if (!range)
    return;
  DCHECK(range->IsConnected());
  start_position_ = FromPositionInDOMTree<Strategy>(range->StartPosition());
  end_position_ = FromPositionInDOMTree<Strategy>(range->EndPosition());
#if DCHECK_IS_ON()
  dom_tree_version_ = range->OwnerDocument().DomTreeVersion();
#endif
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate() = default;

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::~EphemeralRangeTemplate() = default;

template <typename Strategy>
EphemeralRangeTemplate<Strategy>& EphemeralRangeTemplate<Strategy>::operator=(
    const EphemeralRangeTemplate<Strategy>& other) {
  DCHECK(other.IsValid());
  start_position_ = other.start_position_;
  end_position_ = other.end_position_;
#if DCHECK_IS_ON()
  dom_tree_version_ = other.dom_tree_version_;
#endif
  return *this;
}

template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::operator==(
    const EphemeralRangeTemplate<Strategy>& other) const {
  return StartPosition() == other.StartPosition() &&
         EndPosition() == other.EndPosition();
}

template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::operator!=(
    const EphemeralRangeTemplate<Strategy>& other) const {
  return !operator==(other);
}

template <typename Strategy>
Document& EphemeralRangeTemplate<Strategy>::GetDocument() const {
  DCHECK(IsNotNull());
  return *start_position_.GetDocument();
}

template <typename Strategy>
PositionTemplate<Strategy> EphemeralRangeTemplate<Strategy>::StartPosition()
    const {
  DCHECK(IsValid());
  return start_position_;
}

template <typename Strategy>
PositionTemplate<Strategy> EphemeralRangeTemplate<Strategy>::EndPosition()
    const {
  DCHECK(IsValid());
  return end_position_;
}

template <typename Strategy>
Node* EphemeralRangeTemplate<Strategy>::CommonAncestorContainer() const {
  return CommonAncestorContainerNode<Strategy>(
      start_position_.ComputeContainerNode(),
      end_position_.ComputeContainerNode());
}

template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::IsCollapsed() const {
  DCHECK(IsValid());
  return start_position_ == end_position_;
}

template <typename Strategy>
typename EphemeralRangeTemplate<Strategy>::RangeTraversal
EphemeralRangeTemplate<Strategy>::Nodes() const {
  return RangeTraversal(start_position_.NodeAsRangeFirstNode(),
                        end_position_.NodeAsRangePastLastNode());
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>
EphemeralRangeTemplate<Strategy>::RangeOfContents(const Node& node) {
  return EphemeralRangeTemplate<Strategy>(
      PositionTemplate<Strategy>::FirstPositionInNode(node),
      PositionTemplate<Strategy>::LastPositionInNode(node));
}

#if DCHECK_IS_ON()
template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::IsValid() const {
  return start_position_.IsNull() ||
         dom_tree_version_ == start_position_.GetDocument()->DomTreeVersion();
}
#else
template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::IsValid() const {
  return true;
}
#endif

#if DCHECK_IS_ON()

template <typename Strategy>
void EphemeralRangeTemplate<Strategy>::ShowTreeForThis() const {
  if (IsNull()) {
    LOG(INFO) << "<null range>" << std::endl;
    return;
  }
  LOG(INFO) << std::endl
            << StartPosition()
                   .AnchorNode()
                   ->ToMarkedTreeString(StartPosition().AnchorNode(), "S",
                                        EndPosition().AnchorNode(), "E")
                   .Utf8()
            << "start: " << StartPosition().ToAnchorTypeAndOffsetString().Utf8()
            << std::endl
            << "end: " << EndPosition().ToAnchorTypeAndOffsetString().Utf8();
}

#endif

Range* CreateRange(const EphemeralRange& range) {
  if (range.IsNull())
    return nullptr;
  return MakeGarbageCollected<Range>(range.GetDocument(), range.StartPosition(),
                                     range.EndPosition());
}

template <typename Strategy>
static std::ostream& PrintEphemeralRange(
    std::ostream& ostream,
    const EphemeralRangeTemplate<Strategy> range) {
  if (range.IsNull())
    return ostream << "null";
  if (range.IsCollapsed())
    return ostream << range.StartPosition();
  return ostream << '[' << range.StartPosition() << ", " << range.EndPosition()
                 << ']';
}

std::ostream& operator<<(std::ostream& ostream, const EphemeralRange& range) {
  return PrintEphemeralRange(ostream, range);
}

std::ostream& operator<<(std::ostream& ostream,
                         const EphemeralRangeInFlatTree& range) {
  return PrintEphemeralRange(ostream, range);
}

EphemeralRangeInFlatTree ToEphemeralRangeInFlatTree(
    const EphemeralRange& range) {
  PositionInFlatTree start = ToPositionInFlatTree(range.StartPosition());
  PositionInFlatTree end = ToPositionInFlatTree(range.EndPosition());
  if (start.IsNull() || end.IsNull() ||
      start.GetDocument() != end.GetDocument())
    return EphemeralRangeInFlatTree();
  start.AnchorNode()->UpdateDistributionForFlatTreeTraversal();
  end.AnchorNode()->UpdateDistributionForFlatTreeTraversal();
  if (!start.IsValidFor(*start.GetDocument()) ||
      !end.IsValidFor(*end.GetDocument()))
    return EphemeralRangeInFlatTree();
  if (start <= end)
    return EphemeralRangeInFlatTree(start, end);
  return EphemeralRangeInFlatTree(end, start);
}

template class CORE_TEMPLATE_EXPORT EphemeralRangeTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    EphemeralRangeTemplate<EditingInFlatTreeStrategy>;

}  // namespace blink
