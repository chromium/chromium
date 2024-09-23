/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/editing/visible_selection.h"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/selection_adjuster.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate()
    : affinity_(TextAffinity::kDownstream), anchor_is_first_(true) {}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(
    const SelectionTemplate<Strategy>& selection)
    : anchor_(selection.Anchor()),
      focus_(selection.Focus()),
      affinity_(selection.Affinity()),
      anchor_is_first_(selection.IsAnchorFirst()) {}

template <typename Strategy>
class VisibleSelectionTemplate<Strategy>::Creator {
  STATIC_ONLY(Creator);

 public:
  static VisibleSelectionTemplate<Strategy> CreateWithGranularity(
      const SelectionTemplate<Strategy>& selection,
      TextGranularity granularity) {
    return VisibleSelectionTemplate<Strategy>(
        ComputeVisibleSelection(selection, granularity));
  }

  static SelectionTemplate<Strategy> ComputeVisibleSelection(
      const SelectionTemplate<Strategy>& passed_selection,
      TextGranularity granularity,
      const WordInclusion& inclusion = WordInclusion::kDefault) {
    DCHECK(!NeedsLayoutTreeUpdate(passed_selection.Anchor()));
    DCHECK(!NeedsLayoutTreeUpdate(passed_selection.Focus()));

    const SelectionTemplate<Strategy>& canonicalized_selection =
        CanonicalizeSelection(passed_selection);

    if (canonicalized_selection.IsNone())
      return SelectionTemplate<Strategy>();

    const SelectionTemplate<Strategy>& granularity_adjusted_selection =
        SelectionAdjuster::AdjustSelectionRespectingGranularity(
            canonicalized_selection, granularity, inclusion);
    const SelectionTemplate<Strategy>& shadow_adjusted_selection =
        SelectionAdjuster::AdjustSelectionToAvoidCrossingShadowBoundaries(
            granularity_adjusted_selection);
    const SelectionTemplate<Strategy>& editing_adjusted_selection =
        SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
            shadow_adjusted_selection);
    const SelectionTemplate<Strategy>& type_adjusted_selection =
        SelectionAdjuster::AdjustSelectionType(
            typename SelectionTemplate<Strategy>::Builder(
                editing_adjusted_selection)
                .SetAffinity(passed_selection.Affinity())
                .Build());
    return type_adjusted_selection;
  }
};

VisibleSelection CreateVisibleSelection(const SelectionInDOMTree& selection) {
  return VisibleSelection::Creator::CreateWithGranularity(
      selection, TextGranularity::kCharacter);
}

VisibleSelectionInFlatTree CreateVisibleSelection(
    const SelectionInFlatTree& selection) {
  return VisibleSelectionInFlatTree::Creator::CreateWithGranularity(
      selection, TextGranularity::kCharacter);
}

SelectionInDOMTree ExpandWithGranularity(const SelectionInDOMTree& selection,
                                         TextGranularity granularity,
                                         const WordInclusion& inclusion) {
  return VisibleSelection::Creator::ComputeVisibleSelection(
      selection, granularity, inclusion);
}

SelectionInFlatTree ExpandWithGranularity(const SelectionInFlatTree& selection,
                                          TextGranularity granularity,
                                          const WordInclusion& inclusion) {
  return VisibleSelectionInFlatTree::Creator::ComputeVisibleSelection(
      selection, granularity, inclusion);
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(
    const VisibleSelectionTemplate<Strategy>& other)
    : anchor_(other.anchor_),
      focus_(other.focus_),
      affinity_(other.affinity_),
      anchor_is_first_(other.anchor_is_first_) {}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>& VisibleSelectionTemplate<Strategy>::
operator=(const VisibleSelectionTemplate<Strategy>& other) {
  anchor_ = other.anchor_;
  focus_ = other.focus_;
  affinity_ = other.affinity_;
  anchor_is_first_ = other.anchor_is_first_;
  return *this;
}

template <typename Strategy>
SelectionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::AsSelection()
    const {
  if (anchor_.IsNull()) {
    return typename SelectionTemplate<Strategy>::Builder()
        .Build();
  }
  return typename SelectionTemplate<Strategy>::Builder()
      .SetBaseAndExtent(anchor_, focus_)
      .SetAffinity(affinity_)
      .Build();
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::IsCaret() const {
  return anchor_.IsNotNull() && anchor_ == focus_;
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::IsNone() const {
  return anchor_.IsNull();
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::IsRange() const {
  return anchor_ != focus_;
}

template <typename Strategy>
PositionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::Start() const {
  return anchor_is_first_ ? anchor_ : focus_;
}

template <typename Strategy>
PositionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::End() const {
  return anchor_is_first_ ? focus_ : anchor_;
}

EphemeralRange FirstEphemeralRangeOf(const VisibleSelection& selection) {
  if (selection.IsNone())
    return EphemeralRange();
  Position start = selection.Start().ParentAnchoredEquivalent();
  Position end = selection.End().ParentAnchoredEquivalent();
  return EphemeralRange(start, end);
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>
VisibleSelectionTemplate<Strategy>::ToNormalizedEphemeralRange() const {
  return NormalizeRange(AsSelection());
}

template <typename Strategy>
static EphemeralRangeTemplate<Strategy> NormalizeRangeAlgorithm(
    const SelectionTemplate<Strategy>& selection) {
  if (selection.IsNone())
    return EphemeralRangeTemplate<Strategy>();

  // Make sure we have an updated layout since this function is called
  // in the course of running edit commands which modify the DOM.
  // Failing to ensure this can result in equivalentXXXPosition calls returning
  // incorrect results.
  DCHECK(!NeedsLayoutTreeUpdate(selection.Anchor())) << selection;

  if (selection.IsCaret()) {
    // If the selection is a caret, move the range start upstream. This
    // helps us match the conventions of text editors tested, which make
    // style determinations based on the character before the caret, if any.
    const PositionTemplate<Strategy> start =
        MostBackwardCaretPosition(selection.ComputeStartPosition())
            .ParentAnchoredEquivalent();
    return EphemeralRangeTemplate<Strategy>(start, start);
  }
  // If the selection is a range, select the minimum range that encompasses
  // the selection. Again, this is to match the conventions of text editors
  // tested, which make style determinations based on the first character of
  // the selection. For instance, this operation helps to make sure that the
  // "X" selected below is the only thing selected. The range should not be
  // allowed to "leak" out to the end of the previous text node, or to the
  // beginning of the next text node, each of which has a different style.
  //
  // On a treasure map, <b>X</b> marks the spot.
  //                       ^ selected
  //
  DCHECK(selection.IsRange());
  return NormalizeRange(selection.ComputeRange());
}

EphemeralRange NormalizeRange(const SelectionInDOMTree& selection) {
  return NormalizeRangeAlgorithm(selection);
}

EphemeralRangeInFlatTree NormalizeRange(const SelectionInFlatTree& selection) {
  return NormalizeRangeAlgorithm(selection);
}

template <typename Strategy>
static SelectionTemplate<Strategy> CanonicalizeSelection(
    const SelectionTemplate<Strategy>& selection) {
  if (selection.IsNone())
    return SelectionTemplate<Strategy>();
  const PositionTemplate<Strategy>& anchor =
      CreateVisiblePosition(selection.Anchor(), selection.Affinity())
          .DeepEquivalent();
  if (selection.IsCaret()) {
    if (anchor.IsNull()) {
      return SelectionTemplate<Strategy>();
    }
    return typename SelectionTemplate<Strategy>::Builder()
        .Collapse(anchor)
        .Build();
  }
  const PositionTemplate<Strategy>& focus =
      CreateVisiblePosition(selection.Focus(), selection.Affinity())
          .DeepEquivalent();
  if (anchor.IsNotNull() && focus.IsNotNull()) {
    return typename SelectionTemplate<Strategy>::Builder()
        .SetBaseAndExtent(anchor, focus)
        .Build();
  }
  if (anchor.IsNotNull()) {
    return typename SelectionTemplate<Strategy>::Builder()
        .Collapse(anchor)
        .Build();
  }
  if (focus.IsNotNull()) {
    return
        typename SelectionTemplate<Strategy>::Builder().Collapse(focus).Build();
  }
  return SelectionTemplate<Strategy>();
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::IsValidFor(
    const Document& document) const {
  if (IsNone())
    return true;
  return anchor_.IsValidFor(document) && focus_.IsValidFor(document);
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::IsContentEditable() const {
  return IsEditablePosition(Start());
}

template <typename Strategy>
Element* VisibleSelectionTemplate<Strategy>::RootEditableElement() const {
  return RootEditableElementOf(Start());
}

template <typename Strategy>
static bool EqualSelectionsAlgorithm(
    const VisibleSelectionTemplate<Strategy>& selection1,
    const VisibleSelectionTemplate<Strategy>& selection2) {
  if (selection1.Affinity() != selection2.Affinity())
    return false;

  if (selection1.IsNone())
    return selection2.IsNone();

  const VisibleSelectionTemplate<Strategy> selection_wrapper1(selection1);
  const VisibleSelectionTemplate<Strategy> selection_wrapper2(selection2);

  return selection_wrapper1.Anchor() == selection_wrapper2.Anchor() &&
         selection_wrapper1.Focus() == selection_wrapper2.Focus();
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::operator==(
    const VisibleSelectionTemplate<Strategy>& other) const {
  return EqualSelectionsAlgorithm<Strategy>(*this, other);
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisibleSelectionTemplate<Strategy>::VisibleStart() const {
  return CreateVisiblePosition(
      Start(), IsRange() ? TextAffinity::kDownstream : Affinity());
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisibleSelectionTemplate<Strategy>::VisibleEnd() const {
  return CreateVisiblePosition(
      End(), IsRange() ? TextAffinity::kUpstream : Affinity());
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisibleSelectionTemplate<Strategy>::VisibleAnchor() const {
  return CreateVisiblePosition(
      anchor_, IsRange() ? (IsAnchorFirst() ? TextAffinity::kUpstream
                                            : TextAffinity::kDownstream)
                         : Affinity());
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisibleSelectionTemplate<Strategy>::VisibleFocus() const {
  return CreateVisiblePosition(
      focus_, IsRange() ? (IsAnchorFirst() ? TextAffinity::kDownstream
                                           : TextAffinity::kUpstream)
                        : Affinity());
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_);
  visitor->Trace(focus_);
}

#if DCHECK_IS_ON()

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::ShowTreeForThis() const {
  if (!Start().AnchorNode()) {
    LOG(INFO) << "\nselection is null";
    return;
  }
  LOG(INFO) << "\n"
            << Start()
                   .AnchorNode()
                   ->ToMarkedTreeString(Start().AnchorNode(), "S",
                                        End().AnchorNode(), "E")
                   .Utf8()
            << "start: " << Start().ToAnchorTypeAndOffsetString().Utf8() << "\n"
            << "end: " << End().ToAnchorTypeAndOffsetString().Utf8();
}

#endif

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::PrintTo(
    const VisibleSelectionTemplate<Strategy>& selection,
    std::ostream* ostream) {
  if (selection.IsNone()) {
    *ostream << "VisibleSelection()";
    return;
  }
  *ostream << "VisibleSelection(anchor: " << selection.Anchor()
           << " focus:" << selection.Focus() << " start: " << selection.Start()
           << " end: " << selection.End() << ' ' << selection.Affinity() << ' '
           << ')';
}

template class CORE_TEMPLATE_EXPORT VisibleSelectionTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    VisibleSelectionTemplate<EditingInFlatTreeStrategy>;

std::ostream& operator<<(std::ostream& ostream,
                         const VisibleSelection& selection) {
  VisibleSelection::PrintTo(selection, &ostream);
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const VisibleSelectionInFlatTree& selection) {
  VisibleSelectionInFlatTree::PrintTo(selection, &ostream);
  return ostream;
}

}  // namespace blink

#if DCHECK_IS_ON()

void ShowTree(const blink::VisibleSelection& sel) {
  sel.ShowTreeForThis();
}

void ShowTree(const blink::VisibleSelection* sel) {
  if (sel)
    sel->ShowTreeForThis();
}

void ShowTree(const blink::VisibleSelectionInFlatTree& sel) {
  sel.ShowTreeForThis();
}

void ShowTree(const blink::VisibleSelectionInFlatTree* sel) {
  if (sel)
    sel->ShowTreeForThis();
}
#endif
