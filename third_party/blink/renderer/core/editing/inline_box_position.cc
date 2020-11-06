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

// Copyright 2017 The Chromium Authors. All rights reserved.
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
PositionTemplate<Strategy> DownstreamIgnoringEditingBoundaries(
    PositionTemplate<Strategy> position) {
  PositionTemplate<Strategy> last_position;
  while (!position.IsEquivalent(last_position)) {
    last_position = position;
    position = MostForwardCaretPosition(position, kCanCrossEditingBoundary);
  }
  return position;
}

template <typename Strategy>
PositionTemplate<Strategy> UpstreamIgnoringEditingBoundaries(
    PositionTemplate<Strategy> position) {
  PositionTemplate<Strategy> last_position;
  while (!position.IsEquivalent(last_position)) {
    last_position = position;
    position = MostBackwardCaretPosition(position, kCanCrossEditingBoundary);
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
  DCHECK(layout_object->IsBox());
  InlineBox* const inline_box = ToLayoutBox(layout_object)->InlineBoxWrapper();
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
    int recursion_depth);

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> AdjustBlockFlowPositionToInline(
    const PositionTemplate<Strategy>& position,
    int recursion_depth) {
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
      DownstreamIgnoringEditingBoundaries(position);
  if (downstream_equivalent != position) {
    return ComputeInlineAdjustedPositionAlgorithm(
        PositionWithAffinityTemplate<Strategy>(downstream_equivalent,
                                               TextAffinity::kUpstream),
        recursion_depth + 1);
  }
  const PositionTemplate<Strategy>& upstream_equivalent =
      UpstreamIgnoringEditingBoundaries(position);
  if (upstream_equivalent == position)
    return PositionWithAffinityTemplate<Strategy>();

  return ComputeInlineAdjustedPositionAlgorithm(
      PositionWithAffinityTemplate<Strategy>(upstream_equivalent,
                                             TextAffinity::kUpstream),
      recursion_depth + 1);
}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> ComputeInlineAdjustedPositionAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& position,
    int recursion_depth) {
  // TODO(yoichio): We don't assume |position| is canonicalized no longer and
  // there are few cases failing to compute. Fix it: crbug.com/812535.
  DCHECK(!position.AnchorNode()->IsShadowRoot()) << position;
  DCHECK(position.GetPosition().AnchorNode()->GetLayoutObject()) << position;
  const LayoutObject& layout_object =
      *position.GetPosition().AnchorNode()->GetLayoutObject();

  if (layout_object.IsText())
    return position;

  // We perform block flow adjustment first, so that we can move into an inline
  // block when needed instead of stopping at its boundary as if it is a
  // replaced element.
  if (layout_object.IsLayoutBlockFlow() &&
      CanHaveChildrenForEditing(position.AnchorNode()) &&
      HasRenderedNonAnonymousDescendantsWithHeight(&layout_object)) {
    return AdjustBlockFlowPositionToInline(position.GetPosition(),
                                           recursion_depth);
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
  const LayoutObject& layout_object = *position.AnchorNode()->GetLayoutObject();
  if (!layout_object.IsText())
    return false;
  const auto& layout_text = To<LayoutText>(layout_object);
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
  DCHECK(!position.AnchorNode()->IsShadowRoot()) << adjusted;
  DCHECK(position.AnchorNode()->GetLayoutObject()) << adjusted;
  const LayoutObject& layout_object = *position.AnchorNode()->GetLayoutObject();
  const int caret_offset = position.ComputeEditingOffset();
  const int round_offset =
      std::min(caret_offset, layout_object.CaretMaxOffset());

  if (layout_object.IsText()) {
    // TODO(yoichio): Consider |ToLayoutText(layout_object)->TextStartOffset()|
    // for first-letter tested with LocalCaretRectTest::FloatFirstLetter.
    return ComputeInlineBoxPositionForTextNode(
        &To<LayoutText>(layout_object), round_offset, adjusted.Affinity());
  }

  DCHECK(layout_object.IsAtomicInlineLevel());
  DCHECK(layout_object.IsInline());
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

InlineBoxPosition ComputeInlineBoxPosition(const VisiblePosition& position) {
  DCHECK(position.IsValid()) << position;
  return ComputeInlineBoxPosition(position.ToPositionWithAffinity());
}

PositionWithAffinity ComputeInlineAdjustedPosition(
    const PositionWithAffinity& position) {
  return ComputeInlineAdjustedPositionAlgorithm(position, 0);
}

PositionInFlatTreeWithAffinity ComputeInlineAdjustedPosition(
    const PositionInFlatTreeWithAffinity& position) {
  return ComputeInlineAdjustedPositionAlgorithm(position, 0);
}

PositionWithAffinity ComputeInlineAdjustedPosition(
    const VisiblePosition& position) {
  DCHECK(position.IsValid()) << position;
  return ComputeInlineAdjustedPositionAlgorithm(
      position.ToPositionWithAffinity(), 0);
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
