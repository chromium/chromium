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

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/inline_box_position.h"

#include "third_party/blink/renderer/core/editing/bidi_adjustment.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"

namespace blink {

namespace {

const int kBlockFlowAdjustmentMaxRecursionDepth = 256;

bool IsNonTextLeafChild(LayoutObject* object) {
  if (object->SlowFirstChild())
    return false;
  if (object->IsText())
    return false;
  return true;
}

InlineTextBox* SearchAheadForBetterMatch(const LayoutText* layout_object) {
  LayoutBlock* container = layout_object->ContainingBlock();
  for (LayoutObject* next = layout_object->NextInPreOrder(container); next;
       next = next->NextInPreOrder(container)) {
    if (next->IsLayoutBlock())
      return nullptr;
    if (next->IsBR())
      return nullptr;
    if (IsNonTextLeafChild(next))
      return nullptr;
    if (next->IsText()) {
      InlineTextBox* match = nullptr;
      int min_offset = INT_MAX;
      for (InlineTextBox* box : To<LayoutText>(next)->TextBoxes()) {
        int caret_min_offset = box->CaretMinOffset();
        if (caret_min_offset < min_offset) {
          match = box;
          min_offset = caret_min_offset;
        }
      }
      if (match)
        return match;
    }
  }
  return nullptr;
}

template <typename Strategy>
PositionTemplate<Strategy> DownstreamVisuallyEquivalent(
    PositionTemplate<Strategy> position,
    EditingBoundaryCrossingRule rule = kCanCrossEditingBoundary) {
  PositionTemplate<Strategy> last_position;
  while (!position.IsEquivalent(last_position)) {
    last_position = position;
    position =
        MostForwardCaretPosition(position, rule, SnapToClient::kLocalCaretRect);
  }
  return position;
}

template <typename Strategy>
PositionTemplate<Strategy> UpstreamVisuallyEquivalent(
    PositionTemplate<Strategy> position,
    EditingBoundaryCrossingRule rule = kCanCrossEditingBoundary) {
  PositionTemplate<Strategy> last_position;
  while (!position.IsEquivalent(last_position)) {
    last_position = position;
    position = MostBackwardCaretPosition(position, rule,
                                         SnapToClient::kLocalCaretRect);
  }
  return position;
}

InlineBoxPosition AdjustInlineBoxPositionForTextDirection(InlineBox* inline_box,
                                                          int caret_offset) {
  DCHECK(caret_offset == inline_box->CaretLeftmostOffset() ||
         caret_offset == inline_box->CaretRightmostOffset());
  return BidiAdjustment::AdjustForCaretPositionResolution(
      InlineBoxPosition(inline_box, caret_offset));
}

// Returns true if |caret_offset| is at edge of |box| based on |affinity|.
// |caret_offset| must be either |box.CaretMinOffset()| or
// |box.CaretMaxOffset()|.
bool IsCaretAtEdgeOfInlineTextBox(int caret_offset,
                                  const InlineTextBox& box,
                                  TextAffinity affinity) {
  if (caret_offset == box.CaretMinOffset())
    return affinity == TextAffinity::kDownstream;
  DCHECK_EQ(caret_offset, box.CaretMaxOffset());
  if (affinity == TextAffinity::kUpstream)
    return true;
  return box.NextLeafChild() && box.NextLeafChild()->IsLineBreak();
}

template <typename Strategy>
LayoutObject& GetLayoutObjectSkippingShadowRoot(
    const PositionTemplate<Strategy>& position) {
  // TODO(editing-dev): This function doesn't handle all types of positions. We
  // may want to investigate callers and decide if we need to generalize it.
  DCHECK(position.IsNotNull());
  const Node* anchor_node = position.AnchorNode();
  auto* shadow_root = DynamicTo<ShadowRoot>(anchor_node);
  LayoutObject* result = shadow_root ? shadow_root->host().GetLayoutObject()
                                     : anchor_node->GetLayoutObject();
  DCHECK(result) << position;
  return *result;
}

InlineBoxPosition ComputeInlineBoxPositionForTextNode(
    const LayoutText* text_layout_object,
    int caret_offset,
    TextAffinity affinity) {
  // TODO(editing-dev): Add the following DCHECK when ready.
  // DCHECK(CanUseInlineBox(*text_layout_object));

  InlineBox* inline_box = nullptr;
  InlineTextBox* candidate = nullptr;

  for (InlineTextBox* box : text_layout_object->TextBoxes()) {
    int caret_min_offset = box->CaretMinOffset();
    int caret_max_offset = box->CaretMaxOffset();

    if (caret_offset < caret_min_offset || caret_offset > caret_max_offset ||
        (caret_offset == caret_max_offset && box->IsLineBreak())) {
      continue;
    }

    if (caret_offset > caret_min_offset && caret_offset < caret_max_offset)
      return InlineBoxPosition(box, caret_offset);

    if (IsCaretAtEdgeOfInlineTextBox(caret_offset, *box, affinity)) {
      inline_box = box;
      break;
    }

    candidate = box;
  }

  // TODO(editing-dev): The fixup below seems hacky. It may also be incorrect in
  // non-ltr text. Make it saner.
  if (candidate && candidate == text_layout_object->LastTextBox() &&
      affinity == TextAffinity::kDownstream &&
      caret_offset == candidate->CaretMaxOffset()) {
    inline_box = SearchAheadForBetterMatch(text_layout_object);
    if (inline_box)
      caret_offset = inline_box->CaretMinOffset();
  }
  if (!inline_box)
    inline_box = candidate;

  if (!inline_box)
    return InlineBoxPosition();
  return AdjustInlineBoxPositionForTextDirection(inline_box, caret_offset);
}

InlineBoxPosition ComputeInlineBoxPositionForAtomicInline(
    const LayoutObject* layout_object,
    int caret_offset) {
  // TODO(editing-dev): Add the following DCHECK when ready.
  // DCHECK(CanUseInlineBox(*layout_object);
  InlineBox* const inline_box =
      To<LayoutBox>(layout_object)->InlineBoxWrapper();
  if (!inline_box)
    return InlineBoxPosition();
  if ((caret_offset > inline_box->CaretMinOffset() &&
       caret_offset < inline_box->CaretMaxOffset()))
    return InlineBoxPosition(inline_box, caret_offset);
  return AdjustInlineBoxPositionForTextDirection(inline_box, caret_offset);
}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> ComputeInlineAdjustedPositionAlgorithm(
    const PositionWithAffinityTemplate<Strategy>&,
    int recursion_depth,
    EditingBoundaryCrossingRule rule);

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> AdjustBlockFlowPositionToInline(
    const PositionTemplate<Strategy>& position,
    int recursion_depth,
    EditingBoundaryCrossingRule rule) {
  DCHECK(position.IsNotNull());
  if (recursion_depth >= kBlockFlowAdjustmentMaxRecursionDepth) {
    // TODO(editing-dev): This function enters infinite recursion in some cases.
    // Find the root cause and fix it. See https://crbug.com/857266
    return PositionWithAffinityTemplate<Strategy>();
  }

  // Try a visually equivalent position with possibly opposite editability. This
  // helps in case |position| is in an editable block but surrounded by
  // non-editable positions. It acts to negate the logic at the beginning of
  // |LayoutObject::CreatePositionWithAffinity()|.
  const PositionTemplate<Strategy>& downstream_equivalent =
      DownstreamVisuallyEquivalent(position, rule);
  DCHECK(downstream_equivalent.IsNotNull());
  if (downstream_equivalent != position &&
      downstream_equivalent.AnchorNode()->GetLayoutObject()) {
    return ComputeInlineAdjustedPositionAlgorithm(
        PositionWithAffinityTemplate<Strategy>(downstream_equivalent,
                                               TextAffinity::kUpstream),
        recursion_depth + 1, rule);
  }
  const PositionTemplate<Strategy>& upstream_equivalent =
      UpstreamVisuallyEquivalent(position, rule);
  DCHECK(upstream_equivalent.IsNotNull());
  if (upstream_equivalent == position ||
      !upstream_equivalent.AnchorNode()->GetLayoutObject())
    return PositionWithAffinityTemplate<Strategy>();

  return ComputeInlineAdjustedPositionAlgorithm(
      PositionWithAffinityTemplate<Strategy>(upstream_equivalent,
                                             TextAffinity::kUpstream),
      recursion_depth + 1, rule);
}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> ComputeInlineAdjustedPositionAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& position,
    int recursion_depth,
    EditingBoundaryCrossingRule rule) {
  const LayoutObject& layout_object =
      GetLayoutObjectSkippingShadowRoot(position.GetPosition());

  if (layout_object.IsText())
    return position;

  if (position.GetPosition().IsBeforeAnchor() ||
      position.GetPosition().IsAfterAnchor()) {
    if (layout_object.IsInLayoutNGInlineFormattingContext()) {
      if (!layout_object.IsInline()) {
        // BeforeNode(<object>) reaches here[1].
        // [1]  editing/return-with-object-element.html
        return PositionWithAffinityTemplate<Strategy>();
      }
      return position;
    }
    // Note: |InlineBoxPosition| supports only LayoutText and atomic inline.
    if (layout_object.IsInline() && layout_object.IsAtomicInlineLevel())
      return position;
  }

  // We perform block flow adjustment first, so that we can move into an inline
  // block when needed instead of stopping at its boundary as if it is a
  // replaced element.
  if (layout_object.IsLayoutBlockFlow() &&
      CanHaveChildrenForEditing(position.AnchorNode()) &&
      HasRenderedNonAnonymousDescendantsWithHeight(&layout_object)) {
    return AdjustBlockFlowPositionToInline(position.GetPosition(),
                                           recursion_depth, rule);
  }

  // TODO(crbug.com/567964): Change the second operand to DCHECK once fixed.
  if (!layout_object.IsAtomicInlineLevel() || !layout_object.IsInline())
    return PositionWithAffinityTemplate<Strategy>();
  return position;
}

// Returns true if |layout_object| and |offset| points after line end.
template <typename Strategy>
bool NeedsLineEndAdjustment(
    const PositionWithAffinityTemplate<Strategy>& adjusted) {
  const PositionTemplate<Strategy>& position = adjusted.GetPosition();
  const LayoutObject* layout_object = position.AnchorNode()->GetLayoutObject();
  if (!layout_object || !layout_object->IsText())
    return false;
  const auto& layout_text = To<LayoutText>(*layout_object);
  if (layout_text.IsBR())
    return position.IsAfterAnchor();
  // For normal text nodes.
  if (!layout_text.Style()->PreserveNewline())
    return false;
  if (!layout_text.TextLength() ||
      layout_text.CharacterAt(layout_text.TextLength() - 1) != '\n')
    return false;
  if (position.IsAfterAnchor())
    return true;
  return position.IsOffsetInAnchor() &&
         position.OffsetInContainerNode() ==
             static_cast<int>(layout_text.TextLength());
}

// Returns the first InlineBoxPosition at next line of last InlineBoxPosition
// in |layout_object| if it exists to avoid making InlineBoxPosition at end of
// line.
InlineBoxPosition NextLinePositionOf(const LayoutText& layout_text) {
  InlineTextBox* const last = layout_text.LastTextBox();
  if (!last)
    return InlineBoxPosition();
  const RootInlineBox& root = last->Root();
  for (const RootInlineBox* runner = root.NextRootBox(); runner;
       runner = runner->NextRootBox()) {
    InlineBox* const inline_box = runner->FirstLeafChild();
    if (!inline_box)
      continue;

    return AdjustInlineBoxPositionForTextDirection(
        inline_box, inline_box->CaretMinOffset());
  }
  return InlineBoxPosition();
}

template <typename Strategy>
InlineBoxPosition ComputeInlineBoxPositionForLineEnd(
    const PositionWithAffinityTemplate<Strategy>& adjusted) {
  const auto& layout_text =
      To<LayoutText>(*adjusted.GetPosition().AnchorNode()->GetLayoutObject());
  const InlineBoxPosition next_line_position = NextLinePositionOf(layout_text);
  if (next_line_position.inline_box)
    return next_line_position;
  // |adjusted| is after line end and no layout object after the position.
  // Fall back to previous position.
  DCHECK_GE(layout_text.TextLength(), 1u);
  return ComputeInlineBoxPositionForTextNode(
      &layout_text, layout_text.TextLength() - 1u, adjusted.Affinity());
}

template <typename Strategy>
InlineBoxPosition ComputeInlineBoxPositionForInlineAdjustedPositionAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& adjusted) {
  if (NeedsLineEndAdjustment(adjusted))
    return ComputeInlineBoxPositionForLineEnd(adjusted);

  const PositionTemplate<Strategy>& position = adjusted.GetPosition();
  LayoutObject& layout_object = GetLayoutObjectSkippingShadowRoot(position);
  const int caret_offset = position.ComputeEditingOffset();

  if (layout_object.IsText()) {
    // TODO(yoichio): Consider |ToLayoutText(layout_object)->TextStartOffset()|
    // for first-letter tested with LocalCaretRectTest::FloatFirstLetter.
    const LayoutText& layout_text = To<LayoutText>(layout_object);
    const int round_offset =
        std::min(caret_offset, layout_text.CaretMaxOffset());
    return ComputeInlineBoxPositionForTextNode(&layout_text, round_offset,
                                               adjusted.Affinity());
  }

  if (!layout_object.IsAtomicInlineLevel() || !layout_object.IsInline()) {
    // AfterNode(<table>) reaches here[1].
    // [1] editing/selection/modify_move/move_into_inline_block_nested.html
    return InlineBoxPosition();
  }

  const int round_offset =
      std::min(caret_offset, LineLayoutItem(&layout_object).CaretMaxOffset());
  return ComputeInlineBoxPositionForAtomicInline(&layout_object, round_offset);
}

template <typename Strategy>
InlineBoxPosition ComputeInlineBoxPositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position) {
  const PositionWithAffinityTemplate<Strategy> adjusted =
      ComputeInlineAdjustedPosition(position);
  if (adjusted.IsNull())
    return InlineBoxPosition();
  return ComputeInlineBoxPositionForInlineAdjustedPosition(adjusted);
}

}  // namespace

// TODO(xiaochengh): Migrate current callers of ComputeInlineBoxPosition to
// ComputeInlineAdjustedPosition() instead.

InlineBoxPosition ComputeInlineBoxPosition(
    const PositionWithAffinity& position) {
  return ComputeInlineBoxPositionTemplate<EditingStrategy>(position);
}

InlineBoxPosition ComputeInlineBoxPosition(
    const PositionInFlatTreeWithAffinity& position) {
  return ComputeInlineBoxPositionTemplate<EditingInFlatTreeStrategy>(position);
}

PositionWithAffinity ComputeInlineAdjustedPosition(
    const PositionWithAffinity& position,
    EditingBoundaryCrossingRule rule) {
  return ComputeInlineAdjustedPositionAlgorithm(position, 0, rule);
}

PositionInFlatTreeWithAffinity ComputeInlineAdjustedPosition(
    const PositionInFlatTreeWithAffinity& position,
    EditingBoundaryCrossingRule rule) {
  return ComputeInlineAdjustedPositionAlgorithm(position, 0, rule);
}

InlineBoxPosition ComputeInlineBoxPositionForInlineAdjustedPosition(
    const PositionWithAffinity& position) {
  return ComputeInlineBoxPositionForInlineAdjustedPositionAlgorithm(position);
}

InlineBoxPosition ComputeInlineBoxPositionForInlineAdjustedPosition(
    const PositionInFlatTreeWithAffinity& position) {
  return ComputeInlineBoxPositionForInlineAdjustedPositionAlgorithm(position);
}

}  // namespace blink
