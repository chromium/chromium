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
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate()
    : affinity_(TextAffinity::kDownstream), base_is_first_(true) {}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(
    const SelectionTemplate<Strategy>& selection)
    : base_(selection.Base()),
      extent_(selection.Extent()),
      affinity_(selection.Affinity()),
      base_is_first_(selection.IsBaseFirst()) {}

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

 private:
  static SelectionTemplate<Strategy> ComputeVisibleSelection(
      const SelectionTemplate<Strategy>& passed_selection,
      TextGranularity granularity) {
    DCHECK(!NeedsLayoutTreeUpdate(passed_selection.Base()));
    DCHECK(!NeedsLayoutTreeUpdate(passed_selection.Extent()));

    const SelectionTemplate<Strategy>& canonicalized_selection =
        CanonicalizeSelection(passed_selection);

    if (canonicalized_selection.IsNone())
      return SelectionTemplate<Strategy>();

    const SelectionTemplate<Strategy>& granularity_adjusted_selection =
        SelectionAdjuster::AdjustSelectionRespectingGranularity(
            canonicalized_selection, granularity);
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

VisibleSelection CreateVisibleSelectionWithGranularity(
    const SelectionInDOMTree& selection,
    TextGranularity granularity) {
  return VisibleSelection::Creator::CreateWithGranularity(selection,
                                                          granularity);
}

VisibleSelectionInFlatTree CreateVisibleSelectionWithGranularity(
    const SelectionInFlatTree& selection,
    TextGranularity granularity) {
  return VisibleSelectionInFlatTree::Creator::CreateWithGranularity(
      selection, granularity);
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(
    const VisibleSelectionTemplate<Strategy>& other)
    : base_(other.base_),
      extent_(other.extent_),
      affinity_(other.affinity_),
      base_is_first_(other.base_is_first_) {}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>& VisibleSelectionTemplate<Strategy>::
operator=(const VisibleSelectionTemplate<Strategy>& other) {
  base_ = other.base_;
  extent_ = other.extent_;
  affinity_ = other.affinity_;
  base_is_first_ = other.base_is_first_;
  return *this;
}

template <typename Strategy>
SelectionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::AsSelection()
    const {
  if (base_.IsNull()) {
    return typename SelectionTemplate<Strategy>::Builder()
        .Build();
  }
  return typename SelectionTemplate<Strategy>::Builder()
      .SetBaseAndExtent(base_, extent_)
      .SetAffinity(affinity_)
      .Build();
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::IsCaret() const {
  return base_.IsNotNull() && base_ == extent_;
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::IsNone() const {
  return base_.IsNull();
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::IsRange() const {
  return base_ != extent_;
}

template <typename Strategy>
PositionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::Start() const {
  return base_is_first_ ? base_ : extent_;
}

template <typename Strategy>
PositionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::End() const {
  return base_is_first_ ? extent_ : base_;
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
  if (IsNone())
    return EphemeralRangeTemplate<Strategy>();

  // Make sure we have an updated layout since this function is called
  // in the course of running edit commands which modify the DOM.
  // Failing to ensure this can result in equivalentXXXPosition calls returning
  // incorrect results.
  DCHECK(!NeedsLayoutTreeUpdate(Start())) << *this;

  if (IsCaret()) {
    // If the selection is a caret, move the range start upstream. This
    // helps us match the conventions of text editors tested, which make
    // style determinations based on the character before the caret, if any.
    const PositionTemplate<Strategy> start =
        MostBackwardCaretPosition(Start()).ParentAnchoredEquivalent();
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
  DCHECK(IsRange());
  return NormalizeRange(EphemeralRangeTemplate<Strategy>(Start(), End()));
}

template <typename Strategy>
static SelectionTemplate<Strategy> CanonicalizeSelection(
    const SelectionTemplate<Strategy>& selection) {
  if (selection.IsNone())
    return SelectionTemplate<Strategy>();
  const PositionTemplate<Strategy>& base =
      CreateVisiblePosition(selection.Base(), selection.Affinity())
          .DeepEquivalent();
  if (selection.IsCaret()) {
    if (base.IsNull())
      return SelectionTemplate<Strategy>();
    return
        typename SelectionTemplate<Strategy>::Builder().Collapse(base).Build();
  }
  const PositionTemplate<Strategy>& extent =
      CreateVisiblePosition(selection.Extent(), selection.Affinity())
          .DeepEquivalent();
  if (base.IsNotNull() && extent.IsNotNull()) {
    return typename SelectionTemplate<Strategy>::Builder()
        .SetBaseAndExtent(base, extent)
        .Build();
  }
  if (base.IsNotNull()) {
    return
        typename SelectionTemplate<Strategy>::Builder().Collapse(base).Build();
  }
  if (extent.IsNotNull()) {
    return typename SelectionTemplate<Strategy>::Builder()
        .Collapse(extent)
        .Build();
  }
  return SelectionTemplate<Strategy>();
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::IsValidFor(
    const Document& document) const {
  if (IsNone())
    return true;
  return base_.IsValidFor(document) && extent_.IsValidFor(document);
}

// TODO(yosin) This function breaks the invariant of this class.
// But because we use VisibleSelection to store values in editing commands for
// use when undoing the command, we need to be able to create a selection that
// while currently invalid, will be valid once the changes are undone. This is a
// design problem. To fix it we either need to change the invariants of
// |VisibleSelection| or create a new class for editing to use that can
// manipulate selections that are not currently valid.
template <typename Strategy>
VisibleSelectionTemplate<Strategy>
VisibleSelectionTemplate<Strategy>::CreateWithoutValidationDeprecated(
    const PositionTemplate<Strategy>& base,
    const PositionTemplate<Strategy>& extent,
    TextAffinity affinity) {
  DCHECK(base.IsNotNull());
  DCHECK(extent.IsNotNull());

  VisibleSelectionTemplate<Strategy> visible_selection;
  visible_selection.base_ = base;
  visible_selection.extent_ = extent;
  visible_selection.base_is_first_ = base.CompareTo(extent) <= 0;
  if (base == extent) {
    visible_selection.affinity_ = affinity;
    return visible_selection;
  }
  // Since |affinity_| for non-|CaretSelection| is always |kDownstream|,
  // we should keep this invariant. Note: This function can be called with
  // |affinity_| is |kUpstream|.
  visible_selection.affinity_ = TextAffinity::kDownstream;
  return visible_selection;
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

  return selection_wrapper1.Base() == selection_wrapper2.Base() &&
         selection_wrapper1.Extent() == selection_wrapper2.Extent();
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
VisibleSelectionTemplate<Strategy>::VisibleBase() const {
  return CreateVisiblePosition(
      base_, IsRange() ? (IsBaseFirst() ? TextAffinity::kUpstream
                                        : TextAffinity::kDownstream)
                       : Affinity());
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisibleSelectionTemplate<Strategy>::VisibleExtent() const {
  return CreateVisiblePosition(
      extent_, IsRange() ? (IsBaseFirst() ? TextAffinity::kDownstream
                                          : TextAffinity::kUpstream)
                         : Affinity());
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::Trace(Visitor* visitor) {
  visitor->Trace(base_);
  visitor->Trace(extent_);
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
  *ostream << "VisibleSelection(base: " << selection.Base()
           << " extent:" << selection.Extent()
           << " start: " << selection.Start() << " end: " << selection.End()
           << ' ' << selection.Affinity() << ' '
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

void showTree(const blink::VisibleSelection& sel) {
  sel.ShowTreeForThis();
}

void showTree(const blink::VisibleSelection* sel) {
  if (sel)
    sel->ShowTreeForThis();
}

void showTree(const blink::VisibleSelectionInFlatTree& sel) {
  sel.ShowTreeForThis();
}

void showTree(const blink::VisibleSelectionInFlatTree* sel) {
  if (sel)
    sel->ShowTreeForThis();
}
#endif
