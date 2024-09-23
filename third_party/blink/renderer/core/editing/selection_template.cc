// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/selection_template.h"

#include <ostream>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"

namespace blink {

template <typename Strategy>
SelectionTemplate<Strategy>::SelectionTemplate(const SelectionTemplate& other)
    : anchor_(other.anchor_),
      focus_(other.focus_),
      affinity_(other.affinity_),
      direction_(other.direction_)
#if DCHECK_IS_ON()
      ,
      dom_tree_version_(other.dom_tree_version_)
#endif
{
  DCHECK(other.AssertValid());
}

template <typename Strategy>
SelectionTemplate<Strategy>::SelectionTemplate() = default;

template <typename Strategy>
bool SelectionTemplate<Strategy>::operator==(
    const SelectionTemplate& other) const {
  DCHECK(AssertValid());
  DCHECK(other.AssertValid());
  if (IsNone())
    return other.IsNone();
  if (other.IsNone())
    return false;
  DCHECK_EQ(anchor_.GetDocument(), other.GetDocument())
      << *this << ' ' << other;
  return anchor_ == other.anchor_ && focus_ == other.focus_ &&
         affinity_ == other.affinity_;
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::operator!=(
    const SelectionTemplate& other) const {
  return !operator==(other);
}

template <typename Strategy>
void SelectionTemplate<Strategy>::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_);
  visitor->Trace(focus_);
}

template <typename Strategy>
const PositionTemplate<Strategy>& SelectionTemplate<Strategy>::Anchor() const {
  DCHECK(AssertValid());
  DCHECK(!anchor_.IsOrphan()) << anchor_;
  return anchor_;
}

template <typename Strategy>
Document* SelectionTemplate<Strategy>::GetDocument() const {
  DCHECK(AssertValid());
  return anchor_.GetDocument();
}

template <typename Strategy>
const PositionTemplate<Strategy>& SelectionTemplate<Strategy>::Focus() const {
  DCHECK(AssertValid());
  DCHECK(!focus_.IsOrphan()) << focus_;
  return focus_;
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::IsCaret() const {
  return anchor_.IsNotNull() && anchor_ == focus_;
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::IsRange() const {
  return anchor_ != focus_;
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::IsValidFor(const Document& document) const {
  if (IsNone())
    return true;
  return anchor_.IsValidFor(document) && focus_.IsValidFor(document);
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::AssertValidFor(
    const Document& document) const {
  if (!AssertValid())
    return false;
  if (anchor_.IsNull()) {
    return true;
  }
  DCHECK_EQ(anchor_.GetDocument(), document) << *this;
  return true;
}

#if DCHECK_IS_ON()
template <typename Strategy>
bool SelectionTemplate<Strategy>::AssertValid() const {
  if (anchor_.IsNull()) {
    return true;
  }
  DCHECK_EQ(anchor_.GetDocument()->DomTreeVersion(), dom_tree_version_)
      << *this;
  DCHECK(!anchor_.IsOrphan()) << *this;
  DCHECK(!focus_.IsOrphan()) << *this;
  DCHECK_EQ(anchor_.GetDocument(), focus_.GetDocument());
  return true;
}
#else
template <typename Strategy>
bool SelectionTemplate<Strategy>::AssertValid() const {
  return true;
}
#endif

#if DCHECK_IS_ON()
template <typename Strategy>
void SelectionTemplate<Strategy>::ShowTreeForThis() const {
  if (anchor_.IsNull()) {
    LOG(INFO) << "\nanchor is null";
    return;
  }

  LOG(INFO) << "\n"
            << anchor_.AnchorNode()
                   ->ToMarkedTreeString(anchor_.AnchorNode(), "B",
                                        focus_.AnchorNode(), "E")
                   .Utf8()
            << "anchor: " << anchor_.ToAnchorTypeAndOffsetString().Utf8()
            << "\n"
            << "focus: " << focus_.ToAnchorTypeAndOffsetString().Utf8();
}
#endif

template <typename Strategy>
const PositionTemplate<Strategy>&
SelectionTemplate<Strategy>::ComputeEndPosition() const {
  return IsAnchorFirst() ? focus_ : anchor_;
}

template <typename Strategy>
const PositionTemplate<Strategy>&
SelectionTemplate<Strategy>::ComputeStartPosition() const {
  return IsAnchorFirst() ? anchor_ : focus_;
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy> SelectionTemplate<Strategy>::ComputeRange()
    const {
  return EphemeralRangeTemplate<Strategy>(ComputeStartPosition(),
                                          ComputeEndPosition());
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::IsAnchorFirst() const {
  DCHECK(AssertValid());
  if (anchor_ == focus_) {
    DCHECK_EQ(direction_, Direction::kForward);
    return true;
  }
  if (direction_ == Direction::kForward) {
    DCHECK_LE(anchor_, focus_);
    return true;
  }
  if (direction_ == Direction::kBackward) {
    DCHECK_GT(anchor_, focus_);
    return false;
  }
  // Note: Since same position can be represented in different anchor type,
  // e.g. Position(div, 0) and BeforeNode(first-child), we use |<=| to check
  // forward selection.
  DCHECK_EQ(direction_, Direction::kNotComputed);
  direction_ = anchor_ <= focus_ ? Direction::kForward : Direction::kBackward;
  return direction_ == Direction::kForward;
}

template <typename Strategy>
void SelectionTemplate<Strategy>::ResetDirectionCache() const {
  direction_ =
      anchor_ == focus_ ? Direction::kForward : Direction::kNotComputed;
}

template <typename Strategy>
void SelectionTemplate<Strategy>::PrintTo(std::ostream* ostream,
                                          const char* type) const {
  if (IsNone()) {
    *ostream << "()";
    return;
  }
  *ostream << type << '(';
#if DCHECK_IS_ON()
  if (dom_tree_version_ != anchor_.GetDocument()->DomTreeVersion()) {
    *ostream << "Dirty: " << dom_tree_version_;
    *ostream << " != " << anchor_.GetDocument()->DomTreeVersion() << ' ';
  }
#endif
  *ostream << "anchor: " << anchor_ << ", focus: " << focus_ << ')';
}

std::ostream& operator<<(std::ostream& ostream,
                         const SelectionInDOMTree& selection) {
  selection.PrintTo(&ostream, "Selection");
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const SelectionInFlatTree& selection) {
  selection.PrintTo(&ostream, "SelectionInFlatTree");
  return ostream;
}

// --

template <typename Strategy>
SelectionTemplate<Strategy>::Builder::Builder(
    const SelectionTemplate<Strategy>& selection)
    : selection_(selection) {}

template <typename Strategy>
SelectionTemplate<Strategy>::Builder::Builder() = default;

template <typename Strategy>
SelectionTemplate<Strategy> SelectionTemplate<Strategy>::Builder::Build()
    const {
  DCHECK(selection_.AssertValid());
  if (selection_.direction_ == Direction::kBackward) {
    DCHECK_LE(selection_.focus_, selection_.anchor_);
    return selection_;
  }
  if (selection_.direction_ == Direction::kForward) {
    if (selection_.IsNone())
      return selection_;
    DCHECK_LE(selection_.anchor_, selection_.focus_);
    return selection_;
  }
  DCHECK_EQ(selection_.direction_, Direction::kNotComputed);
  selection_.ResetDirectionCache();
  return selection_;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::Collapse(
    const PositionTemplate<Strategy>& position) {
  DCHECK(position.IsConnected()) << position;
  selection_.anchor_ = position;
  selection_.focus_ = position;
#if DCHECK_IS_ON()
  selection_.dom_tree_version_ = position.GetDocument()->DomTreeVersion();
#endif
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::Collapse(
    const PositionWithAffinityTemplate<Strategy>& position_with_affinity) {
  Collapse(position_with_affinity.GetPosition());
  SetAffinity(position_with_affinity.Affinity());
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::Extend(
    const PositionTemplate<Strategy>& position) {
  DCHECK(position.IsConnected()) << position;
  DCHECK_EQ(selection_.GetDocument(), position.GetDocument());
  DCHECK(selection_.Anchor().IsConnected()) << selection_.Anchor();
  DCHECK(selection_.AssertValid());
  if (selection_.focus_.IsEquivalent(position)) {
    return *this;
  }
  selection_.focus_ = position;
  selection_.direction_ = Direction::kNotComputed;
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SelectAllChildren(const Node& node) {
  DCHECK(node.CanContainRangeEndPoint()) << node;
  return SetBaseAndExtent(
      EphemeralRangeTemplate<Strategy>::RangeOfContents(node));
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SetAffinity(TextAffinity affinity) {
  selection_.affinity_ = affinity;
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SetAsBackwardSelection(
    const EphemeralRangeTemplate<Strategy>& range) {
  DCHECK(range.IsNotNull());
  DCHECK(!range.IsCollapsed());
  DCHECK(selection_.IsNone()) << selection_;
  selection_.anchor_ = range.EndPosition();
  selection_.focus_ = range.StartPosition();
  selection_.direction_ = Direction::kBackward;
  DCHECK_GT(selection_.anchor_, selection_.focus_);
#if DCHECK_IS_ON()
  selection_.dom_tree_version_ = range.GetDocument().DomTreeVersion();
#endif
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SetAsForwardSelection(
    const EphemeralRangeTemplate<Strategy>& range) {
  DCHECK(range.IsNotNull());
  DCHECK(selection_.IsNone()) << selection_;
  selection_.anchor_ = range.StartPosition();
  selection_.focus_ = range.EndPosition();
  selection_.direction_ = Direction::kForward;
  DCHECK_LE(selection_.anchor_, selection_.focus_);
#if DCHECK_IS_ON()
  selection_.dom_tree_version_ = range.GetDocument().DomTreeVersion();
#endif
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SetBaseAndExtent(
    const EphemeralRangeTemplate<Strategy>& range) {
  if (range.IsNull()) {
    selection_.anchor_ = PositionTemplate<Strategy>();
    selection_.focus_ = PositionTemplate<Strategy>();
#if DCHECK_IS_ON()
    selection_.dom_tree_version_ = 0;
#endif
    return *this;
  }
  return SetAsForwardSelection(range);
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SetBaseAndExtent(
    const PositionTemplate<Strategy>& base,
    const PositionTemplate<Strategy>& extent) {
  if (base.IsNull()) {
    DCHECK(extent.IsNull()) << extent;
    return SetBaseAndExtent(EphemeralRangeTemplate<Strategy>());
  }
  // TODO(crbug.com/1423127): `extent` is not expected to be `IsNull` but it
  // looks like there are such cases.
  return Collapse(base).Extend(extent);
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SetBaseAndExtentDeprecated(
    const PositionTemplate<Strategy>& base,
    const PositionTemplate<Strategy>& extent) {
  if (base.IsNotNull() && extent.IsNotNull()) {
    return SetBaseAndExtent(base, extent);
  }
  if (base.IsNotNull())
    return Collapse(base);
  if (extent.IsNotNull())
    return Collapse(extent);
  return SetBaseAndExtent(EphemeralRangeTemplate<Strategy>());
}

// ---

template <typename Strategy>
SelectionTemplate<Strategy>::InvalidSelectionResetter::InvalidSelectionResetter(
    const SelectionTemplate<Strategy>& selection)
    : document_(selection.GetDocument()),
      selection_(const_cast<SelectionTemplate&>(selection)) {
  DCHECK(selection_.AssertValid());
}

template <typename Strategy>
SelectionTemplate<
    Strategy>::InvalidSelectionResetter::~InvalidSelectionResetter() {
  if (selection_.IsNone())
    return;
  DCHECK(document_);
  if (!selection_.IsValidFor(*document_)) {
    selection_ = SelectionTemplate<Strategy>();
    return;
  }
#if DCHECK_IS_ON()
  selection_.dom_tree_version_ = document_->DomTreeVersion();
#endif
  selection_.ResetDirectionCache();
}

SelectionInDOMTree ConvertToSelectionInDOMTree(
    const SelectionInFlatTree& selection_in_flat_tree) {
  return SelectionInDOMTree::Builder()
      .SetAffinity(selection_in_flat_tree.Affinity())
      .SetBaseAndExtent(ToPositionInDOMTree(selection_in_flat_tree.Anchor()),
                        ToPositionInDOMTree(selection_in_flat_tree.Focus()))
      .Build();
}

SelectionInFlatTree ConvertToSelectionInFlatTree(
    const SelectionInDOMTree& selection) {
  SelectionInFlatTree::Builder builder;
  const PositionInFlatTree& anchor = ToPositionInFlatTree(selection.Anchor());
  const PositionInFlatTree& focus = ToPositionInFlatTree(selection.Focus());
  if (anchor.IsConnected() && focus.IsConnected()) {
    builder.SetBaseAndExtent(anchor, focus);
  } else if (anchor.IsConnected()) {
    builder.Collapse(anchor);
  } else if (focus.IsConnected()) {
    builder.Collapse(focus);
  }
  builder.SetAffinity(selection.Affinity());
  return builder.Build();
}

template <typename Strategy>
void SelectionTemplate<Strategy>::InvalidSelectionResetter::Trace(
    blink::Visitor* visitor) const {
  visitor->Trace(document_);
}

template class CORE_TEMPLATE_EXPORT SelectionTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    SelectionTemplate<EditingInFlatTreeStrategy>;

}  // namespace blink
