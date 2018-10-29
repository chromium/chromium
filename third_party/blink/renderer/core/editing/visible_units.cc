/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
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

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/backwards_character_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/backwards_text_buffer.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/forwards_text_buffer.h"
#include "third_party/blink/renderer/core/editing/iterators/simplified_backwards_text_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_iterator.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/inline_iterator.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/svg_element_type_helpers.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"

namespace blink {

template <typename PositionType>
static PositionType CanonicalizeCandidate(const PositionType& candidate) {
  if (candidate.IsNull())
    return PositionType();
  DCHECK(IsVisuallyEquivalentCandidate(candidate));
  PositionType upstream = MostBackwardCaretPosition(candidate);
  if (IsVisuallyEquivalentCandidate(upstream))
    return upstream;
  return candidate;
}

template <typename PositionType>
static PositionType CanonicalPosition(const PositionType& position) {
  // Sometimes updating selection positions can be extremely expensive and
  // occur frequently.  Often calling preventDefault on mousedown events can
  // avoid doing unnecessary text selection work.  http://crbug.com/472258.
  TRACE_EVENT0("input", "VisibleUnits::canonicalPosition");

  // FIXME (9535):  Canonicalizing to the leftmost candidate means that if
  // we're at a line wrap, we will ask layoutObjects to paint downstream
  // carets for other layoutObjects. To fix this, we need to either a) add
  // code to all paintCarets to pass the responsibility off to the appropriate
  // layoutObject for VisiblePosition's like these, or b) canonicalize to the
  // rightmost candidate unless the affinity is upstream.
  if (position.IsNull())
    return PositionType();

  DCHECK(position.GetDocument());
  DCHECK(!position.GetDocument()->NeedsLayoutTreeUpdate());

  const PositionType& backward_candidate = MostBackwardCaretPosition(position);
  if (IsVisuallyEquivalentCandidate(backward_candidate))
    return backward_candidate;

  const PositionType& forward_candidate = MostForwardCaretPosition(position);
  if (IsVisuallyEquivalentCandidate(forward_candidate))
    return forward_candidate;

  // When neither upstream or downstream gets us to a candidate
  // (upstream/downstream won't leave blocks or enter new ones), we search
  // forward and backward until we find one.
  const PositionType& next = CanonicalizeCandidate(NextCandidate(position));
  const PositionType& prev = CanonicalizeCandidate(PreviousCandidate(position));

  // The new position must be in the same editable element. Enforce that
  // first. Unless the descent is from a non-editable html element to an
  // editable body.
  Node* const node = position.ComputeContainerNode();
  if (node && node->GetDocument().documentElement() == node &&
      !HasEditableStyle(*node) && node->GetDocument().body() &&
      HasEditableStyle(*node->GetDocument().body()))
    return next.IsNotNull() ? next : prev;

  Element* const editing_root = RootEditableElementOf(position);
  // If the html element is editable, descending into its body will look like
  // a descent from non-editable to editable content since
  // |rootEditableElementOf()| always stops at the body.
  if ((editing_root &&
       editing_root->GetDocument().documentElement() == editing_root) ||
      position.AnchorNode()->IsDocumentNode())
    return next.IsNotNull() ? next : prev;

  Node* const next_node = next.AnchorNode();
  Node* const prev_node = prev.AnchorNode();
  const bool prev_is_in_same_editable_element =
      prev_node && RootEditableElementOf(prev) == editing_root;
  const bool next_is_in_same_editable_element =
      next_node && RootEditableElementOf(next) == editing_root;
  if (prev_is_in_same_editable_element && !next_is_in_same_editable_element)
    return prev;

  if (next_is_in_same_editable_element && !prev_is_in_same_editable_element)
    return next;

  if (!next_is_in_same_editable_element && !prev_is_in_same_editable_element)
    return PositionType();

  // The new position should be in the same block flow element. Favor that.
  Element* const original_block =
      node ? EnclosingBlockFlowElement(*node) : nullptr;
  const bool next_is_outside_original_block =
      !next_node->IsDescendantOf(original_block) && next_node != original_block;
  const bool prev_is_outside_original_block =
      !prev_node->IsDescendantOf(original_block) && prev_node != original_block;
  if (next_is_outside_original_block && !prev_is_outside_original_block)
    return prev;

  return next;
}

Position CanonicalPositionOf(const Position& position) {
  return CanonicalPosition(position);
}

PositionInFlatTree CanonicalPositionOf(const PositionInFlatTree& position) {
  return CanonicalPosition(position);
}

template <typename Strategy>
static PositionWithAffinityTemplate<Strategy>
AdjustBackwardPositionToAvoidCrossingEditingBoundariesTemplate(
    const PositionWithAffinityTemplate<Strategy>& pos,
    const PositionTemplate<Strategy>& anchor) {
  if (pos.IsNull())
    return pos;

  ContainerNode* highest_root = HighestEditableRoot(anchor);

  // Return empty position if |pos| is not somewhere inside the editable
  // region containing this position
  if (highest_root && !pos.AnchorNode()->IsDescendantOf(highest_root))
    return PositionWithAffinityTemplate<Strategy>();

  // Return |pos| itself if the two are from the very same editable region, or
  // both are non-editable
  // TODO(yosin) In the non-editable case, just because the new position is
  // non-editable doesn't mean movement to it is allowed.
  // |VisibleSelection::adjustForEditableContent()| has this problem too.
  if (HighestEditableRoot(pos.GetPosition()) == highest_root)
    return pos;

  // Return empty position if this position is non-editable, but |pos| is
  // editable.
  // TODO(yosin) Move to the previous non-editable region.
  if (!highest_root)
    return PositionWithAffinityTemplate<Strategy>();

  // Return the last position before |pos| that is in the same editable region
  // as this position
  return PositionWithAffinityTemplate<Strategy>(
      LastEditablePositionBeforePositionInRoot(pos.GetPosition(),
                                               *highest_root));
}

PositionWithAffinity AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
    const PositionWithAffinity& pos,
    const Position& anchor) {
  return AdjustBackwardPositionToAvoidCrossingEditingBoundariesTemplate(pos,
                                                                        anchor);
}

PositionInFlatTreeWithAffinity
AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
    const PositionInFlatTreeWithAffinity& pos,
    const PositionInFlatTree& anchor) {
  return AdjustBackwardPositionToAvoidCrossingEditingBoundariesTemplate(pos,
                                                                        anchor);
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
AdjustBackwardPositionToAvoidCrossingEditingBoundariesAlgorithm(
    const VisiblePositionTemplate<Strategy>& pos,
    const PositionTemplate<Strategy>& anchor) {
  DCHECK(pos.IsValid()) << pos;
  return CreateVisiblePosition(
      AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
          pos.ToPositionWithAffinity(), anchor));
}

VisiblePosition AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
    const VisiblePosition& visiblePosition,
    const Position& anchor) {
  return AdjustBackwardPositionToAvoidCrossingEditingBoundariesAlgorithm(
      visiblePosition, anchor);
}

VisiblePositionInFlatTree
AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
    const VisiblePositionInFlatTree& visiblePosition,
    const PositionInFlatTree& anchor) {
  return AdjustBackwardPositionToAvoidCrossingEditingBoundariesAlgorithm(
      visiblePosition, anchor);
}

template <typename Strategy>
static PositionWithAffinityTemplate<Strategy>
AdjustForwardPositionToAvoidCrossingEditingBoundariesTemplate(
    const PositionWithAffinityTemplate<Strategy>& pos,
    const PositionTemplate<Strategy>& anchor) {
  if (pos.IsNull())
    return pos;

  ContainerNode* highest_root = HighestEditableRoot(anchor);

  // Return empty position if |pos| is not somewhere inside the editable
  // region containing this position
  if (highest_root && !pos.AnchorNode()->IsDescendantOf(highest_root))
    return PositionWithAffinityTemplate<Strategy>();

  // Return |pos| itself if the two are from the very same editable region, or
  // both are non-editable
  // TODO(yosin) In the non-editable case, just because the new position is
  // non-editable doesn't mean movement to it is allowed.
  // |VisibleSelection::adjustForEditableContent()| has this problem too.
  if (HighestEditableRoot(pos.GetPosition()) == highest_root)
    return pos;

  // Returns the last position in the highest non-editable ancestor of |anchor|.
  if (!highest_root) {
    const Node* last_non_editable = anchor.ComputeContainerNode();
    for (const Node& ancestor : Strategy::AncestorsOf(*last_non_editable)) {
      if (HasEditableStyle(ancestor)) {
        return PositionWithAffinityTemplate<Strategy>(
            PositionTemplate<Strategy>::LastPositionInNode(*last_non_editable));
      }
      last_non_editable = &ancestor;
    }
    return PositionWithAffinityTemplate<Strategy>();
  }

  // Return the next position after |pos| that is in the same editable region
  // as this position
  return PositionWithAffinityTemplate<Strategy>(
      FirstEditablePositionAfterPositionInRoot(pos.GetPosition(),
                                               *highest_root));
}

PositionWithAffinity AdjustForwardPositionToAvoidCrossingEditingBoundaries(
    const PositionWithAffinity& pos,
    const Position& anchor) {
  return AdjustForwardPositionToAvoidCrossingEditingBoundariesTemplate(pos,
                                                                       anchor);
}

PositionInFlatTreeWithAffinity
AdjustForwardPositionToAvoidCrossingEditingBoundaries(
    const PositionInFlatTreeWithAffinity& pos,
    const PositionInFlatTree& anchor) {
  return AdjustForwardPositionToAvoidCrossingEditingBoundariesTemplate(
      PositionInFlatTreeWithAffinity(pos), anchor);
}

VisiblePosition AdjustForwardPositionToAvoidCrossingEditingBoundaries(
    const VisiblePosition& pos,
    const Position& anchor) {
  DCHECK(pos.IsValid()) << pos;
  return CreateVisiblePosition(
      AdjustForwardPositionToAvoidCrossingEditingBoundaries(
          pos.ToPositionWithAffinity(), anchor));
}

VisiblePositionInFlatTree AdjustForwardPositionToAvoidCrossingEditingBoundaries(
    const VisiblePositionInFlatTree& pos,
    const PositionInFlatTree& anchor) {
  DCHECK(pos.IsValid()) << pos;
  return CreateVisiblePosition(
      AdjustForwardPositionToAvoidCrossingEditingBoundaries(
          pos.ToPositionWithAffinity(), anchor));
}

template <typename Strategy>
static ContainerNode* NonShadowBoundaryParentNode(Node* node) {
  ContainerNode* parent = Strategy::Parent(*node);
  return parent && !parent->IsShadowRoot() ? parent : nullptr;
}

template <typename Strategy>
static Node* ParentEditingBoundary(const PositionTemplate<Strategy>& position) {
  Node* const anchor_node = position.AnchorNode();
  if (!anchor_node)
    return nullptr;

  Node* document_element = anchor_node->GetDocument().documentElement();
  if (!document_element)
    return nullptr;

  Node* boundary = position.ComputeContainerNode();
  while (boundary != document_element &&
         NonShadowBoundaryParentNode<Strategy>(boundary) &&
         HasEditableStyle(*anchor_node) ==
             HasEditableStyle(*Strategy::Parent(*boundary)))
    boundary = NonShadowBoundaryParentNode<Strategy>(boundary);

  return boundary;
}

template <typename Strategy>
static PositionTemplate<Strategy> PreviousBoundaryAlgorithm(
    const VisiblePositionTemplate<Strategy>& c,
    BoundarySearchFunction search_function) {
  DCHECK(c.IsValid()) << c;
  const PositionTemplate<Strategy> pos = c.DeepEquivalent();
  Node* boundary = ParentEditingBoundary(pos);
  if (!boundary)
    return PositionTemplate<Strategy>();

  const PositionTemplate<Strategy> start =
      PositionTemplate<Strategy>::EditingPositionOf(boundary, 0)
          .ParentAnchoredEquivalent();
  const PositionTemplate<Strategy> end = pos.ParentAnchoredEquivalent();

  ForwardsTextBuffer suffix_string;
  if (RequiresContextForWordBoundary(CharacterBefore(c))) {
    TextIteratorAlgorithm<Strategy> forwards_iterator(
        end, PositionTemplate<Strategy>::AfterNode(*boundary));
    while (!forwards_iterator.AtEnd()) {
      forwards_iterator.CopyTextTo(&suffix_string);
      int context_end_index = EndOfFirstWordBoundaryContext(
          suffix_string.Data() + suffix_string.Size() -
              forwards_iterator.length(),
          forwards_iterator.length());
      if (context_end_index < forwards_iterator.length()) {
        suffix_string.Shrink(forwards_iterator.length() - context_end_index);
        break;
      }
      forwards_iterator.Advance();
    }
  }

  unsigned suffix_length = suffix_string.Size();
  BackwardsTextBuffer string;
  string.PushRange(suffix_string.Data(), suffix_string.Size());

  // Treat bullets used in the text security mode as regular characters when
  // looking for boundaries.
  SimplifiedBackwardsTextIteratorAlgorithm<Strategy> it(
      EphemeralRangeTemplate<Strategy>(start, end),
      TextIteratorBehavior::Builder()
          .SetEmitsSmallXForTextSecurity(true)
          .Build());
  unsigned next = 0;
  bool need_more_context = false;
  while (!it.AtEnd()) {
    // iterate to get chunks until the searchFunction returns a non-zero
    // value.
    int run_offset = 0;
    do {
      run_offset += it.CopyTextTo(&string, run_offset, string.Capacity());
      next = search_function(string.Data(), string.Size(),
                             string.Size() - suffix_length, kMayHaveMoreContext,
                             need_more_context);
    } while (!next && run_offset < it.length());
    if (next)
      break;
    it.Advance();
  }
  if (need_more_context) {
    // The last search returned the beginning of the buffer and asked for
    // more context, but there is no earlier text. Force a search with
    // what's available.
    next = search_function(string.Data(), string.Size(),
                           string.Size() - suffix_length, kDontHaveMoreContext,
                           need_more_context);
    DCHECK(!need_more_context);
  }

  if (!next)
    return it.AtEnd() ? it.StartPosition() : pos;

  // Use the character iterator to translate the next value into a DOM
  // position.
  BackwardsCharacterIteratorAlgorithm<Strategy> char_it(
      EphemeralRangeTemplate<Strategy>(start, end));
  char_it.Advance(string.Size() - suffix_length - next);
  // TODO(yosin) charIt can get out of shadow host.
  return char_it.EndPosition();
}

Position PreviousBoundary(const VisiblePosition& visible_position,
                          BoundarySearchFunction search_function) {
  return PreviousBoundaryAlgorithm(visible_position, search_function);
}

PositionInFlatTree PreviousBoundary(
    const VisiblePositionInFlatTree& visible_position,
    BoundarySearchFunction search_function) {
  return PreviousBoundaryAlgorithm(visible_position, search_function);
}

// ---------

template <typename Strategy>
static VisiblePositionTemplate<Strategy> StartOfDocumentAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  Node* node = visible_position.DeepEquivalent().AnchorNode();
  if (!node || !node->GetDocument().documentElement())
    return VisiblePositionTemplate<Strategy>();

  return CreateVisiblePosition(PositionTemplate<Strategy>::FirstPositionInNode(
      *node->GetDocument().documentElement()));
}

VisiblePosition StartOfDocument(const VisiblePosition& c) {
  return StartOfDocumentAlgorithm<EditingStrategy>(c);
}

VisiblePositionInFlatTree StartOfDocument(const VisiblePositionInFlatTree& c) {
  return StartOfDocumentAlgorithm<EditingInFlatTreeStrategy>(c);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> EndOfDocumentAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  Node* node = visible_position.DeepEquivalent().AnchorNode();
  if (!node || !node->GetDocument().documentElement())
    return VisiblePositionTemplate<Strategy>();

  Element* doc = node->GetDocument().documentElement();
  return CreateVisiblePosition(
      PositionTemplate<Strategy>::LastPositionInNode(*doc));
}

VisiblePosition EndOfDocument(const VisiblePosition& c) {
  return EndOfDocumentAlgorithm<EditingStrategy>(c);
}

VisiblePositionInFlatTree EndOfDocument(const VisiblePositionInFlatTree& c) {
  return EndOfDocumentAlgorithm<EditingInFlatTreeStrategy>(c);
}

bool IsStartOfDocument(const VisiblePosition& p) {
  DCHECK(p.IsValid()) << p;
  return p.IsNotNull() &&
         PreviousPositionOf(p, kCanCrossEditingBoundary).IsNull();
}

bool IsEndOfDocument(const VisiblePosition& p) {
  DCHECK(p.IsValid()) << p;
  return p.IsNotNull() && NextPositionOf(p, kCanCrossEditingBoundary).IsNull();
}

// ---------

VisiblePosition StartOfEditableContent(
    const VisiblePosition& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  ContainerNode* highest_root =
      HighestEditableRoot(visible_position.DeepEquivalent());
  if (!highest_root)
    return VisiblePosition();

  return VisiblePosition::FirstPositionInNode(*highest_root);
}

VisiblePosition EndOfEditableContent(const VisiblePosition& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  ContainerNode* highest_root =
      HighestEditableRoot(visible_position.DeepEquivalent());
  if (!highest_root)
    return VisiblePosition();

  return VisiblePosition::LastPositionInNode(*highest_root);
}

bool IsEndOfEditableOrNonEditableContent(const VisiblePosition& position) {
  DCHECK(position.IsValid()) << position;
  return position.IsNotNull() && NextPositionOf(position).IsNull();
}

// TODO(yosin) We should rename |isEndOfEditableOrNonEditableContent()| what
// this function does, e.g. |isLastVisiblePositionOrEndOfInnerEditor()|.
bool IsEndOfEditableOrNonEditableContent(
    const VisiblePositionInFlatTree& position) {
  DCHECK(position.IsValid()) << position;
  if (position.IsNull())
    return false;
  const VisiblePositionInFlatTree next_position = NextPositionOf(position);
  if (next_position.IsNull())
    return true;
  // In DOM version, following condition, the last position of inner editor
  // of INPUT/TEXTAREA element, by |nextPosition().isNull()|, because of
  // an inner editor is an only leaf node.
  if (!next_position.DeepEquivalent().IsAfterAnchor())
    return false;
  return IsTextControl(next_position.DeepEquivalent().AnchorNode());
}

static LayoutUnit BoundingBoxLogicalHeight(LayoutObject* o,
                                           const LayoutRect& rect) {
  return o->Style()->IsHorizontalWritingMode() ? rect.Height() : rect.Width();
}

// TODO(editing-dev): The semantics seems wrong when we're in a one-letter block
// with first-letter style, e.g., <div>F</div>, where the letter is laid-out in
// an anonymous first-letter LayoutTextFragment instead of the LayoutObject of
// the text node. It seems weird to return false in this case.
bool HasRenderedNonAnonymousDescendantsWithHeight(
    const LayoutObject* layout_object) {
  const LayoutObject* stop = layout_object->NextInPreOrderAfterChildren();
  // TODO(editing-dev): Avoid single-character parameter names.
  for (LayoutObject* o = layout_object->SlowFirstChild(); o && o != stop;
       o = o->NextInPreOrder()) {
    if (o->NonPseudoNode()) {
      if ((o->IsText() && ToLayoutText(o)->HasNonCollapsedText()) ||
          (o->IsBox() && ToLayoutBox(o)->PixelSnappedLogicalHeight()) ||
          (o->IsLayoutInline() && IsEmptyInline(LineLayoutItem(o)) &&
           // TODO(crbug.com/771398): Find alternative ways to check whether an
           // empty LayoutInline is rendered, without checking InlineBox.
           BoundingBoxLogicalHeight(o, ToLayoutInline(o)->LinesBoundingBox())))
        return true;
    }
  }
  return false;
}

VisiblePosition VisiblePositionForContentsPoint(const IntPoint& contents_point,
                                                LocalFrame* frame) {
  HitTestRequest request = HitTestRequest::kMove | HitTestRequest::kReadOnly |
                           HitTestRequest::kActive |
                           HitTestRequest::kIgnoreClipping;
  HitTestLocation location(contents_point);
  HitTestResult result(request, location);
  frame->GetDocument()->GetLayoutView()->HitTest(location, result);

  if (Node* node = result.InnerNode()) {
    return CreateVisiblePosition(PositionRespectingEditingBoundary(
        frame->Selection().ComputeVisibleSelectionInDOMTreeDeprecated().Start(),
        result.LocalPoint(), node));
  }
  return VisiblePosition();
}

// TODO(yosin): We should use |AssociatedLayoutObjectOf()| in "visible_units.cc"
// where it takes |LayoutObject| from |Position|.

int CaretMinOffset(const Node* node) {
  const LayoutObject* layout_object = AssociatedLayoutObjectOf(*node, 0);
  return layout_object ? layout_object->CaretMinOffset() : 0;
}

int CaretMaxOffset(const Node* n) {
  return EditingStrategy::CaretMaxOffset(*n);
}

template <typename Strategy>
static bool InRenderedText(const PositionTemplate<Strategy>& position) {
  Node* const anchor_node = position.AnchorNode();
  if (!anchor_node || !anchor_node->IsTextNode())
    return false;

  const int offset_in_node = position.ComputeEditingOffset();
  const LayoutObject* layout_object =
      AssociatedLayoutObjectOf(*anchor_node, offset_in_node);
  if (!layout_object)
    return false;

  const LayoutText* text_layout_object = ToLayoutText(layout_object);
  const int text_offset =
      offset_in_node - text_layout_object->TextStartOffset();
  if (!text_layout_object->ContainsCaretOffset(text_offset))
    return false;
  // Return false for offsets inside composed characters.
  // TODO(editing-dev): Previous/NextGraphemeBoundaryOf() work on DOM offsets,
  // So they should use |offset_in_node| instead of |text_offset|.
  return text_offset == text_layout_object->CaretMinOffset() ||
         text_offset == NextGraphemeBoundaryOf(*anchor_node,
                                               PreviousGraphemeBoundaryOf(
                                                   *anchor_node, text_offset));
}

bool RendersInDifferentPosition(const Position& position1,
                                const Position& position2) {
  if (position1.IsNull() || position2.IsNull())
    return false;
  const LocalCaretRect& caret_rect1 =
      LocalCaretRectOfPosition(PositionWithAffinity(position1));
  const LocalCaretRect& caret_rect2 =
      LocalCaretRectOfPosition(PositionWithAffinity(position2));
  if (!caret_rect1.layout_object || !caret_rect2.layout_object)
    return caret_rect1.layout_object != caret_rect2.layout_object;
  return LocalToAbsoluteQuadOf(caret_rect1) !=
         LocalToAbsoluteQuadOf(caret_rect2);
}

// TODO(editing-dev): Share code with IsVisuallyEquivalentCandidate if possible.
bool EndsOfNodeAreVisuallyDistinctPositions(const Node* node) {
  if (!node)
    return false;

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return false;

  if (!layout_object->IsInline())
    return true;

  // Don't include inline tables.
  if (IsHTMLTableElement(*node))
    return false;

  // A Marquee elements are moving so we should assume their ends are always
  // visibily distinct.
  if (IsHTMLMarqueeElement(*node))
    return true;

  // There is a VisiblePosition inside an empty inline-block container.
  return layout_object->IsAtomicInlineLevel() &&
         CanHaveChildrenForEditing(node) &&
         !ToLayoutBox(layout_object)->Size().IsEmpty() &&
         !HasRenderedNonAnonymousDescendantsWithHeight(layout_object);
}

template <typename Strategy>
static Node* EnclosingVisualBoundary(Node* node) {
  while (node && !EndsOfNodeAreVisuallyDistinctPositions(node))
    node = Strategy::Parent(*node);

  return node;
}

// upstream() and downstream() want to return positions that are either in a
// text node or at just before a non-text node.  This method checks for that.
template <typename Strategy>
static bool IsStreamer(const PositionIteratorAlgorithm<Strategy>& pos) {
  if (!pos.GetNode())
    return true;

  if (IsAtomicNode(pos.GetNode()))
    return true;

  return pos.AtStartOfNode();
}

template <typename Strategy>
static PositionTemplate<Strategy> AdjustPositionForBackwardIteration(
    const PositionTemplate<Strategy>& position) {
  DCHECK(!position.IsNull());
  if (!position.IsAfterAnchor())
    return position;
  if (IsUserSelectContain(*position.AnchorNode()))
    return position.ToOffsetInAnchor();
  return PositionTemplate<Strategy>::EditingPositionOf(
      position.AnchorNode(), Strategy::CaretMaxOffset(*position.AnchorNode()));
}

template <typename Strategy>
static PositionTemplate<Strategy> MostBackwardCaretPosition(
    const PositionTemplate<Strategy>& position,
    EditingBoundaryCrossingRule rule) {
  DCHECK(!NeedsLayoutTreeUpdate(position)) << position;
  TRACE_EVENT0("input", "VisibleUnits::mostBackwardCaretPosition");

  Node* const start_node = position.AnchorNode();
  if (!start_node)
    return PositionTemplate<Strategy>();

  // iterate backward from there, looking for a qualified position
  Node* const boundary = EnclosingVisualBoundary<Strategy>(start_node);
  // FIXME: PositionIterator should respect Before and After positions.
  PositionIteratorAlgorithm<Strategy> last_visible(
      AdjustPositionForBackwardIteration<Strategy>(position));
  const bool start_editable = HasEditableStyle(*start_node);
  Node* last_node = start_node;
  bool boundary_crossed = false;
  for (PositionIteratorAlgorithm<Strategy> current_pos = last_visible;
       !current_pos.AtStart(); current_pos.Decrement()) {
    Node* current_node = current_pos.GetNode();
    // Don't check for an editability change if we haven't moved to a different
    // node, to avoid the expense of computing hasEditableStyle().
    if (current_node != last_node) {
      // Don't change editability.
      const bool current_editable = HasEditableStyle(*current_node);
      if (start_editable != current_editable) {
        if (rule == kCannotCrossEditingBoundary)
          break;
        boundary_crossed = true;
      }
      last_node = current_node;
    }

    // There is no caret position in non-text svg elements.
    if (current_node->IsSVGElement() && !IsSVGTextElement(current_node))
      continue;

    // If we've moved to a position that is visually distinct, return the last
    // saved position. There is code below that terminates early if we're
    // *about* to move to a visually distinct position.
    if (EndsOfNodeAreVisuallyDistinctPositions(current_node) &&
        current_node != boundary)
      return last_visible.DeprecatedComputePosition();

    // skip position in non-laid out or invisible node
    const LayoutObject* const layout_object =
        AssociatedLayoutObjectOf(*current_node, current_pos.OffsetInLeafNode(),
                                 LayoutObjectSide::kFirstLetterIfOnBoundary);
    if (!layout_object ||
        layout_object->Style()->Visibility() != EVisibility::kVisible)
      continue;

    if (rule == kCanCrossEditingBoundary && boundary_crossed) {
      last_visible = current_pos;
      break;
    }

    // track last visible streamer position
    if (IsStreamer<Strategy>(current_pos))
      last_visible = current_pos;

    // Don't move past a position that is visually distinct.  We could rely on
    // code above to terminate and return lastVisible on the next iteration, but
    // we terminate early to avoid doing a nodeIndex() call.
    if (EndsOfNodeAreVisuallyDistinctPositions(current_node) &&
        current_pos.AtStartOfNode())
      return last_visible.DeprecatedComputePosition();

    // Return position after tables and nodes which have content that can be
    // ignored.
    if (EditingIgnoresContent(*current_node) ||
        IsDisplayInsideTable(current_node)) {
      if (current_pos.AtEndOfNode())
        return PositionTemplate<Strategy>::AfterNode(*current_node);
      continue;
    }

    // return current position if it is in laid out text
    if (!layout_object->IsText())
      continue;
    const LayoutText* const text_layout_object = ToLayoutText(layout_object);
    if (!text_layout_object->HasNonCollapsedText())
      continue;
    const unsigned text_start_offset = text_layout_object->TextStartOffset();
    if (current_node != start_node) {
      // This assertion fires in layout tests in the case-transform.html test
      // because of a mix-up between offsets in the text in the DOM tree with
      // text in the layout tree which can have a different length due to case
      // transformation.
      // Until we resolve that, disable this so we can run the layout tests!
      // DCHECK_GE(currentOffset, layoutObject->caretMaxOffset());
      return PositionTemplate<Strategy>(
          current_node, layout_object->CaretMaxOffset() + text_start_offset);
    }

    DCHECK_GE(current_pos.OffsetInLeafNode(),
              static_cast<int>(text_layout_object->TextStartOffset()));
    if (text_layout_object->IsAfterNonCollapsedCharacter(
            current_pos.OffsetInLeafNode() -
            text_layout_object->TextStartOffset()))
      return current_pos.ComputePosition();
  }
  return last_visible.DeprecatedComputePosition();
}

Position MostBackwardCaretPosition(const Position& position,
                                   EditingBoundaryCrossingRule rule) {
  return MostBackwardCaretPosition<EditingStrategy>(position, rule);
}

PositionInFlatTree MostBackwardCaretPosition(const PositionInFlatTree& position,
                                             EditingBoundaryCrossingRule rule) {
  return MostBackwardCaretPosition<EditingInFlatTreeStrategy>(position, rule);
}

namespace {
bool HasInvisibleFirstLetter(const Node* node) {
  if (!node || !node->IsTextNode())
    return false;
  const LayoutTextFragment* remaining_text =
      ToLayoutTextFragmentOrNull(node->GetLayoutObject());
  if (!remaining_text || !remaining_text->IsRemainingTextLayoutObject())
    return false;
  const LayoutTextFragment* first_letter =
      ToLayoutTextFragmentOrNull(AssociatedLayoutObjectOf(*node, 0));
  if (!first_letter || first_letter == remaining_text)
    return false;
  return first_letter->StyleRef().Visibility() != EVisibility::kVisible;
}
}  // namespace

template <typename Strategy>
PositionTemplate<Strategy> MostForwardCaretPosition(
    const PositionTemplate<Strategy>& position,
    EditingBoundaryCrossingRule rule) {
  DCHECK(!NeedsLayoutTreeUpdate(position)) << position;
  TRACE_EVENT0("input", "VisibleUnits::mostForwardCaretPosition");

  Node* const start_node = position.AnchorNode();
  if (!start_node)
    return PositionTemplate<Strategy>();

  // iterate forward from there, looking for a qualified position
  Node* const boundary = EnclosingVisualBoundary<Strategy>(start_node);
  // FIXME: PositionIterator should respect Before and After positions.
  PositionIteratorAlgorithm<Strategy> last_visible(
      position.IsAfterAnchor()
          ? PositionTemplate<Strategy>::EditingPositionOf(
                position.AnchorNode(),
                Strategy::CaretMaxOffset(*position.AnchorNode()))
          : position);
  const bool start_editable = HasEditableStyle(*start_node);
  Node* last_node = start_node;
  bool boundary_crossed = false;
  for (PositionIteratorAlgorithm<Strategy> current_pos = last_visible;
       !current_pos.AtEnd(); current_pos.Increment()) {
    Node* current_node = current_pos.GetNode();
    // Don't check for an editability change if we haven't moved to a different
    // node, to avoid the expense of computing hasEditableStyle().
    if (current_node != last_node) {
      // Don't change editability.
      const bool current_editable = HasEditableStyle(*current_node);
      if (start_editable != current_editable) {
        if (rule == kCannotCrossEditingBoundary)
          break;
        boundary_crossed = true;
      }

      last_node = current_node;
    }

    // stop before going above the body, up into the head
    // return the last visible streamer position
    if (IsHTMLBodyElement(*current_node) && current_pos.AtEndOfNode())
      break;

    // There is no caret position in non-text svg elements.
    if (current_node->IsSVGElement() && !IsSVGTextElement(current_node))
      continue;

    // Do not move to a visually distinct position.
    if (EndsOfNodeAreVisuallyDistinctPositions(current_node) &&
        current_node != boundary)
      return last_visible.DeprecatedComputePosition();
    // Do not move past a visually disinct position.
    // Note: The first position after the last in a node whose ends are visually
    // distinct positions will be [boundary->parentNode(),
    // originalBlock->nodeIndex() + 1].
    if (boundary && Strategy::Parent(*boundary) == current_node)
      return last_visible.DeprecatedComputePosition();

    // skip position in non-laid out or invisible node
    const LayoutObject* const layout_object =
        AssociatedLayoutObjectOf(*current_node, current_pos.OffsetInLeafNode());
    if (!layout_object ||
        layout_object->Style()->Visibility() != EVisibility::kVisible)
      continue;

    if (rule == kCanCrossEditingBoundary && boundary_crossed)
      return current_pos.DeprecatedComputePosition();

    // track last visible streamer position
    if (IsStreamer<Strategy>(current_pos))
      last_visible = current_pos;

    // Return position before tables and nodes which have content that can be
    // ignored.
    if (EditingIgnoresContent(*current_node) ||
        IsDisplayInsideTable(current_node)) {
      if (current_pos.OffsetInLeafNode() <= layout_object->CaretMinOffset())
        return PositionTemplate<Strategy>::EditingPositionOf(
            current_node, layout_object->CaretMinOffset());
      continue;
    }

    // return current position if it is in laid out text
    if (!layout_object->IsText())
      continue;
    const LayoutText* const text_layout_object = ToLayoutText(layout_object);
    if (!text_layout_object->HasNonCollapsedText())
      continue;
    const unsigned text_start_offset = text_layout_object->TextStartOffset();
    if (current_node != start_node) {
      DCHECK(current_pos.AtStartOfNode() ||
             HasInvisibleFirstLetter(current_node));
      return PositionTemplate<Strategy>(
          current_node, layout_object->CaretMinOffset() + text_start_offset);
    }

    DCHECK_GE(current_pos.OffsetInLeafNode(),
              static_cast<int>(text_layout_object->TextStartOffset()));
    if (text_layout_object->IsBeforeNonCollapsedCharacter(
            current_pos.OffsetInLeafNode() -
            text_layout_object->TextStartOffset()))
      return current_pos.ComputePosition();
  }
  return last_visible.DeprecatedComputePosition();
}

Position MostForwardCaretPosition(const Position& position,
                                  EditingBoundaryCrossingRule rule) {
  return MostForwardCaretPosition<EditingStrategy>(position, rule);
}

PositionInFlatTree MostForwardCaretPosition(const PositionInFlatTree& position,
                                            EditingBoundaryCrossingRule rule) {
  return MostForwardCaretPosition<EditingInFlatTreeStrategy>(position, rule);
}

// Returns true if the visually equivalent positions around have different
// editability. A position is considered at editing boundary if one of the
// following is true:
// 1. It is the first position in the node and the next visually equivalent
//    position is non editable.
// 2. It is the last position in the node and the previous visually equivalent
//    position is non editable.
// 3. It is an editable position and both the next and previous visually
//    equivalent positions are both non editable.
template <typename Strategy>
static bool AtEditingBoundary(const PositionTemplate<Strategy> positions) {
  PositionTemplate<Strategy> next_position =
      MostForwardCaretPosition(positions, kCanCrossEditingBoundary);
  if (positions.AtFirstEditingPositionForNode() && next_position.IsNotNull() &&
      !HasEditableStyle(*next_position.AnchorNode()))
    return true;

  PositionTemplate<Strategy> prev_position =
      MostBackwardCaretPosition(positions, kCanCrossEditingBoundary);
  if (positions.AtLastEditingPositionForNode() && prev_position.IsNotNull() &&
      !HasEditableStyle(*prev_position.AnchorNode()))
    return true;

  return next_position.IsNotNull() &&
         !HasEditableStyle(*next_position.AnchorNode()) &&
         prev_position.IsNotNull() &&
         !HasEditableStyle(*prev_position.AnchorNode());
}

template <typename Strategy>
static bool IsVisuallyEquivalentCandidateAlgorithm(
    const PositionTemplate<Strategy>& position) {
  Node* const anchor_node = position.AnchorNode();
  if (!anchor_node)
    return false;

  LayoutObject* layout_object = anchor_node->GetLayoutObject();
  if (!layout_object)
    return false;

  if (layout_object->Style()->Visibility() != EVisibility::kVisible)
    return false;

  if (layout_object->IsBR()) {
    // TODO(leviw) The condition should be
    // anchor_type_ == PositionAnchorType::kBeforeAnchor, but for now we
    // still need to support legacy positions.
    if (position.IsAfterAnchor())
      return false;
    if (position.ComputeEditingOffset())
      return false;
    const Node* parent = Strategy::Parent(*anchor_node);
    return parent->GetLayoutObject() &&
           parent->GetLayoutObject()->IsSelectable();
  }

  if (layout_object->IsText())
    return layout_object->IsSelectable() && InRenderedText(position);

  if (layout_object->IsSVG()) {
    // We don't consider SVG elements are contenteditable except for
    // associated |layoutObject| returns |isText()| true,
    // e.g. |LayoutSVGInlineText|.
    return false;
  }

  if (IsDisplayInsideTable(anchor_node) ||
      EditingIgnoresContent(*anchor_node)) {
    if (!position.AtFirstEditingPositionForNode() &&
        !position.AtLastEditingPositionForNode())
      return false;
    const Node* parent = Strategy::Parent(*anchor_node);
    return parent->GetLayoutObject() &&
           parent->GetLayoutObject()->IsSelectable();
  }

  if (anchor_node->GetDocument().documentElement() == anchor_node ||
      anchor_node->IsDocumentNode())
    return false;

  if (!layout_object->IsSelectable())
    return false;

  if (layout_object->IsLayoutBlockFlow() || layout_object->IsFlexibleBox() ||
      layout_object->IsLayoutGrid()) {
    if (ToLayoutBlock(layout_object)->LogicalHeight() ||
        anchor_node->GetDocument().body() == anchor_node) {
      if (!HasRenderedNonAnonymousDescendantsWithHeight(layout_object))
        return position.AtFirstEditingPositionForNode();
      return HasEditableStyle(*anchor_node) && AtEditingBoundary(position);
    }
  } else {
    return HasEditableStyle(*anchor_node) && AtEditingBoundary(position);
  }

  return false;
}

bool IsVisuallyEquivalentCandidate(const Position& position) {
  return IsVisuallyEquivalentCandidateAlgorithm<EditingStrategy>(position);
}

bool IsVisuallyEquivalentCandidate(const PositionInFlatTree& position) {
  return IsVisuallyEquivalentCandidateAlgorithm<EditingInFlatTreeStrategy>(
      position);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> SkipToEndOfEditingBoundary(
    const VisiblePositionTemplate<Strategy>& pos,
    const PositionTemplate<Strategy>& anchor) {
  DCHECK(pos.IsValid()) << pos;
  if (pos.IsNull())
    return pos;

  ContainerNode* highest_root = HighestEditableRoot(anchor);
  ContainerNode* highest_root_of_pos =
      HighestEditableRoot(pos.DeepEquivalent());

  // Return |pos| itself if the two are from the very same editable region,
  // or both are non-editable.
  if (highest_root_of_pos == highest_root)
    return pos;

  // If this is not editable but |pos| has an editable root, skip to the end
  if (!highest_root && highest_root_of_pos)
    return CreateVisiblePosition(
        PositionTemplate<Strategy>(highest_root_of_pos,
                                   PositionAnchorType::kAfterAnchor)
            .ParentAnchoredEquivalent());

  // That must mean that |pos| is not editable. Return the next position after
  // |pos| that is in the same editable region as this position
  DCHECK(highest_root);
  return FirstEditableVisiblePositionAfterPositionInRoot(pos.DeepEquivalent(),
                                                         *highest_root);
}

template <typename Strategy>
static UChar32 CharacterAfterAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  // We canonicalize to the first of two equivalent candidates, but the second
  // of the two candidates is the one that will be inside the text node
  // containing the character after this visible position.
  const PositionTemplate<Strategy> pos =
      MostForwardCaretPosition(visible_position.DeepEquivalent());
  if (!pos.IsOffsetInAnchor())
    return 0;
  Node* container_node = pos.ComputeContainerNode();
  if (!container_node || !container_node->IsTextNode())
    return 0;
  unsigned offset = static_cast<unsigned>(pos.OffsetInContainerNode());
  Text* text_node = ToText(container_node);
  unsigned length = text_node->length();
  if (offset >= length)
    return 0;

  return text_node->data().CharacterStartingAt(offset);
}

UChar32 CharacterAfter(const VisiblePosition& visible_position) {
  return CharacterAfterAlgorithm<EditingStrategy>(visible_position);
}

UChar32 CharacterAfter(const VisiblePositionInFlatTree& visible_position) {
  return CharacterAfterAlgorithm<EditingInFlatTreeStrategy>(visible_position);
}

template <typename Strategy>
static UChar32 CharacterBeforeAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  return CharacterAfter(PreviousPositionOf(visible_position));
}

UChar32 CharacterBefore(const VisiblePosition& visible_position) {
  return CharacterBeforeAlgorithm<EditingStrategy>(visible_position);
}

UChar32 CharacterBefore(const VisiblePositionInFlatTree& visible_position) {
  return CharacterBeforeAlgorithm<EditingInFlatTreeStrategy>(visible_position);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> NextPositionOfAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& position,
    EditingBoundaryCrossingRule rule) {
  const VisiblePositionTemplate<Strategy> next = CreateVisiblePosition(
      NextVisuallyDistinctCandidate(position.GetPosition()),
      position.Affinity());

  switch (rule) {
    case kCanCrossEditingBoundary:
      return next;
    case kCannotCrossEditingBoundary:
      return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
          next, position.GetPosition());
    case kCanSkipOverEditingBoundary:
      return SkipToEndOfEditingBoundary(next, position.GetPosition());
  }
  NOTREACHED();
  return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
      next, position.GetPosition());
}

VisiblePosition NextPositionOf(const VisiblePosition& visible_position,
                               EditingBoundaryCrossingRule rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  return NextPositionOfAlgorithm<EditingStrategy>(
      visible_position.ToPositionWithAffinity(), rule);
}

VisiblePositionInFlatTree NextPositionOf(
    const VisiblePositionInFlatTree& visible_position,
    EditingBoundaryCrossingRule rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  return NextPositionOfAlgorithm<EditingInFlatTreeStrategy>(
      visible_position.ToPositionWithAffinity(), rule);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> SkipToStartOfEditingBoundary(
    const VisiblePositionTemplate<Strategy>& pos,
    const PositionTemplate<Strategy>& anchor) {
  DCHECK(pos.IsValid()) << pos;
  if (pos.IsNull())
    return pos;

  ContainerNode* highest_root = HighestEditableRoot(anchor);
  ContainerNode* highest_root_of_pos =
      HighestEditableRoot(pos.DeepEquivalent());

  // Return |pos| itself if the two are from the very same editable region, or
  // both are non-editable.
  if (highest_root_of_pos == highest_root)
    return pos;

  // If this is not editable but |pos| has an editable root, skip to the start
  if (!highest_root && highest_root_of_pos)
    return CreateVisiblePosition(PreviousVisuallyDistinctCandidate(
        PositionTemplate<Strategy>(highest_root_of_pos,
                                   PositionAnchorType::kBeforeAnchor)
            .ParentAnchoredEquivalent()));

  // That must mean that |pos| is not editable. Return the last position
  // before |pos| that is in the same editable region as this position
  DCHECK(highest_root);
  return LastEditableVisiblePositionBeforePositionInRoot(pos.DeepEquivalent(),
                                                         *highest_root);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> PreviousPositionOfAlgorithm(
    const PositionTemplate<Strategy>& position,
    EditingBoundaryCrossingRule rule) {
  const PositionTemplate<Strategy> prev_position =
      PreviousVisuallyDistinctCandidate(position);

  // return null visible position if there is no previous visible position
  if (prev_position.AtStartOfTree())
    return VisiblePositionTemplate<Strategy>();

  // we should always be able to make the affinity |TextAffinity::Downstream|,
  // because going previous from an |TextAffinity::Upstream| position can
  // never yield another |TextAffinity::Upstream position| (unless line wrap
  // length is 0!).
  const VisiblePositionTemplate<Strategy> prev =
      CreateVisiblePosition(prev_position);
  if (prev.DeepEquivalent() == position)
    return VisiblePositionTemplate<Strategy>();

  switch (rule) {
    case kCanCrossEditingBoundary:
      return prev;
    case kCannotCrossEditingBoundary:
      return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(prev,
                                                                    position);
    case kCanSkipOverEditingBoundary:
      return SkipToStartOfEditingBoundary(prev, position);
  }

  NOTREACHED();
  return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(prev, position);
}

VisiblePosition PreviousPositionOf(const VisiblePosition& visible_position,
                                   EditingBoundaryCrossingRule rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  return PreviousPositionOfAlgorithm<EditingStrategy>(
      visible_position.DeepEquivalent(), rule);
}

VisiblePositionInFlatTree PreviousPositionOf(
    const VisiblePositionInFlatTree& visible_position,
    EditingBoundaryCrossingRule rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  return PreviousPositionOfAlgorithm<EditingInFlatTreeStrategy>(
      visible_position.DeepEquivalent(), rule);
}

template <typename Strategy>
static EphemeralRangeTemplate<Strategy> MakeSearchRange(
    const PositionTemplate<Strategy>& pos) {
  Node* node = pos.ComputeContainerNode();
  if (!node)
    return EphemeralRangeTemplate<Strategy>();
  Document& document = node->GetDocument();
  if (!document.documentElement())
    return EphemeralRangeTemplate<Strategy>();
  Element* boundary = EnclosingBlockFlowElement(*node);
  if (!boundary)
    return EphemeralRangeTemplate<Strategy>();

  return EphemeralRangeTemplate<Strategy>(
      pos, PositionTemplate<Strategy>::LastPositionInNode(*boundary));
}

template <typename Strategy>
static PositionTemplate<Strategy> SkipWhitespaceAlgorithm(
    const PositionTemplate<Strategy>& position) {
  const EphemeralRangeTemplate<Strategy>& search_range =
      MakeSearchRange(position);
  if (search_range.IsNull())
    return position;

  CharacterIteratorAlgorithm<Strategy> char_it(
      search_range.StartPosition(), search_range.EndPosition(),
      TextIteratorBehavior::Builder()
          .SetEmitsCharactersBetweenAllVisiblePositions(true)
          .Build());
  PositionTemplate<Strategy> runner = position;
  // TODO(editing-dev): We should consider U+20E3, COMBINING ENCLOSING KEYCAP.
  // When whitespace character followed by U+20E3, we should not consider
  // it as trailing white space.
  for (; char_it.length(); char_it.Advance(1)) {
    UChar c = char_it.CharacterAt(0);
    if ((!IsSpaceOrNewline(c) && c != kNoBreakSpaceCharacter) || c == '\n')
      return runner;
    runner = char_it.EndPosition();
  }
  return runner;
}

Position SkipWhitespace(const Position& position) {
  return SkipWhitespaceAlgorithm(position);
}

PositionInFlatTree SkipWhitespace(const PositionInFlatTree& position) {
  return SkipWhitespaceAlgorithm(position);
}

template <typename Strategy>
static Vector<FloatQuad> ComputeTextBounds(
    const EphemeralRangeTemplate<Strategy>& range) {
  const PositionTemplate<Strategy>& start_position = range.StartPosition();
  const PositionTemplate<Strategy>& end_position = range.EndPosition();
  Node* const start_container = start_position.ComputeContainerNode();
  DCHECK(start_container);
  Node* const end_container = end_position.ComputeContainerNode();
  DCHECK(end_container);
  DCHECK(!start_container->GetDocument().NeedsLayoutTreeUpdate());

  Vector<FloatQuad> result;
  for (const Node& node : range.Nodes()) {
    LayoutObject* const layout_object = node.GetLayoutObject();
    if (!layout_object || !layout_object->IsText())
      continue;
    const LayoutText* layout_text = ToLayoutText(layout_object);
    unsigned start_offset =
        node == start_container ? start_position.OffsetInContainerNode() : 0;
    unsigned end_offset = node == end_container
                              ? end_position.OffsetInContainerNode()
                              : std::numeric_limits<unsigned>::max();
    layout_text->AbsoluteQuadsForRange(result, start_offset, end_offset);
  }
  return result;
}

template <typename Strategy>
static FloatRect ComputeTextRectTemplate(
    const EphemeralRangeTemplate<Strategy>& range) {
  FloatRect result;
  for (auto rect : ComputeTextBounds<Strategy>(range))
    result.Unite(rect.BoundingBox());
  return result;
}

IntRect ComputeTextRect(const EphemeralRange& range) {
  return EnclosingIntRect(ComputeTextRectTemplate(range));
}

IntRect ComputeTextRect(const EphemeralRangeInFlatTree& range) {
  return EnclosingIntRect(ComputeTextRectTemplate(range));
}

FloatRect ComputeTextFloatRect(const EphemeralRange& range) {
  return ComputeTextRectTemplate(range);
}

IntRect FirstRectForRange(const EphemeralRange& range) {
  DCHECK(!range.GetDocument().NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      range.GetDocument().Lifecycle());

  LayoutUnit extra_width_to_end_of_line;
  DCHECK(range.IsNotNull());

  const PositionWithAffinity start_position(
      CreateVisiblePosition(range.StartPosition()).DeepEquivalent(),
      TextAffinity::kDownstream);
  const IntRect start_caret_rect =
      AbsoluteCaretRectOfPosition(start_position, &extra_width_to_end_of_line);
  if (start_caret_rect.IsEmpty())
    return IntRect();

  const PositionWithAffinity end_position(
      CreateVisiblePosition(range.EndPosition()).DeepEquivalent(),
      TextAffinity::kUpstream);
  const IntRect end_caret_rect = AbsoluteCaretRectOfPosition(end_position);
  if (end_caret_rect.IsEmpty())
    return IntRect();

  if (start_caret_rect.Y() == end_caret_rect.Y()) {
    // start and end are on the same line
    return IntRect(
        std::min(start_caret_rect.X(), end_caret_rect.X()),
        start_caret_rect.Y(), abs(end_caret_rect.X() - start_caret_rect.X()),
        std::max(start_caret_rect.Height(), end_caret_rect.Height()));
  }

  // start and end aren't on the same line, so go from start to the end of its
  // line
  return IntRect(
      start_caret_rect.X(), start_caret_rect.Y(),
      (start_caret_rect.Width() + extra_width_to_end_of_line).ToInt(),
      start_caret_rect.Height());
}

}  // namespace blink
