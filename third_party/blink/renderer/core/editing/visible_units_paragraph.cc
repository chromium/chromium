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

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"

namespace blink {

namespace {

bool NodeIsUserSelectAll(const Node* node) {
  return node && node->GetLayoutObject() &&
         node->GetLayoutObject()->Style()->UserSelect() == EUserSelect::kAll;
}

template <typename Strategy>
PositionTemplate<Strategy> StartOfParagraphAlgorithm(
    const PositionTemplate<Strategy>& position,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  Node* const start_node = position.AnchorNode();

  if (!start_node)
    return PositionTemplate<Strategy>();

  if (IsRenderedAsNonInlineTableImageOrHR(start_node))
    return PositionTemplate<Strategy>::BeforeNode(*start_node);

  Element* const start_block = EnclosingBlock(
      PositionTemplate<Strategy>::FirstPositionInOrBeforeNode(*start_node),
      kCannotCrossEditingBoundary);
  ContainerNode* const highest_root = HighestEditableRoot(position);
  const bool start_node_is_editable = HasEditableStyle(*start_node);

  Node* candidate_node = start_node;
  PositionAnchorType candidate_type = position.AnchorType();
  int candidate_offset = position.ComputeEditingOffset();

  Node* previous_node_iterator = start_node;
  while (previous_node_iterator) {
    if (boundary_crossing_rule == kCannotCrossEditingBoundary &&
        !NodeIsUserSelectAll(previous_node_iterator) &&
        HasEditableStyle(*previous_node_iterator) != start_node_is_editable)
      break;
    if (boundary_crossing_rule == kCanSkipOverEditingBoundary) {
      while (previous_node_iterator &&
             HasEditableStyle(*previous_node_iterator) !=
                 start_node_is_editable) {
        previous_node_iterator =
            Strategy::PreviousPostOrder(*previous_node_iterator, start_block);
      }
      if (!previous_node_iterator ||
          !previous_node_iterator->IsDescendantOf(highest_root))
        break;
    }

    const LayoutObject* layout_object =
        previous_node_iterator->GetLayoutObject();
    if (!layout_object) {
      previous_node_iterator =
          Strategy::PreviousPostOrder(*previous_node_iterator, start_block);
      continue;
    }
    const ComputedStyle& style = layout_object->StyleRef();
    if (style.Visibility() != EVisibility::kVisible) {
      previous_node_iterator =
          Strategy::PreviousPostOrder(*previous_node_iterator, start_block);
      continue;
    }

    if (layout_object->IsBR() || IsEnclosingBlock(previous_node_iterator))
      break;

    if (layout_object->IsText() &&
        ToLayoutText(previous_node_iterator->GetLayoutObject())
            ->ResolvedTextLength()) {
      if (style.PreserveNewline()) {
        LayoutText* text =
            ToLayoutText(previous_node_iterator->GetLayoutObject());
        int index = text->TextLength();
        if (previous_node_iterator == start_node && candidate_offset < index)
          index = max(0, candidate_offset);
        while (--index >= 0) {
          if ((*text)[index] == '\n') {
            return PositionTemplate<Strategy>(To<Text>(previous_node_iterator),
                                              index + 1);
          }
        }
      }
      candidate_node = previous_node_iterator;
      candidate_type = PositionAnchorType::kOffsetInAnchor;
      candidate_offset = 0;
      previous_node_iterator =
          Strategy::PreviousPostOrder(*previous_node_iterator, start_block);
    } else if (EditingIgnoresContent(*previous_node_iterator) ||
               IsDisplayInsideTable(previous_node_iterator)) {
      candidate_node = previous_node_iterator;
      candidate_type = PositionAnchorType::kBeforeAnchor;
      previous_node_iterator = previous_node_iterator->previousSibling()
                                   ? previous_node_iterator->previousSibling()
                                   : Strategy::PreviousPostOrder(
                                         *previous_node_iterator, start_block);
    } else {
      previous_node_iterator =
          Strategy::PreviousPostOrder(*previous_node_iterator, start_block);
    }
  }

  if (candidate_type == PositionAnchorType::kOffsetInAnchor)
    return PositionTemplate<Strategy>(candidate_node, candidate_offset);

  return PositionTemplate<Strategy>(candidate_node, candidate_type);
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> StartOfParagraphAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  return CreateVisiblePosition(StartOfParagraphAlgorithm(
      visible_position.DeepEquivalent(), boundary_crossing_rule));
}

template <typename Strategy>
PositionTemplate<Strategy> EndOfParagraphAlgorithm(
    const PositionTemplate<Strategy>& position,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  Node* const start_node = position.AnchorNode();

  if (!start_node)
    return PositionTemplate<Strategy>();

  if (IsRenderedAsNonInlineTableImageOrHR(start_node))
    return PositionTemplate<Strategy>::AfterNode(*start_node);

  Element* const start_block = EnclosingBlock(
      PositionTemplate<Strategy>::FirstPositionInOrBeforeNode(*start_node),
      kCannotCrossEditingBoundary);
  ContainerNode* const highest_root = HighestEditableRoot(position);
  const bool start_node_is_editable = HasEditableStyle(*start_node);

  Node* candidate_node = start_node;
  PositionAnchorType candidate_type = position.AnchorType();
  int candidate_offset = position.ComputeEditingOffset();

  Node* next_node_iterator = start_node;
  while (next_node_iterator) {
    if (boundary_crossing_rule == kCannotCrossEditingBoundary &&
        !NodeIsUserSelectAll(next_node_iterator) &&
        HasEditableStyle(*next_node_iterator) != start_node_is_editable)
      break;
    if (boundary_crossing_rule == kCanSkipOverEditingBoundary) {
      while (next_node_iterator &&
             HasEditableStyle(*next_node_iterator) != start_node_is_editable)
        next_node_iterator = Strategy::Next(*next_node_iterator, start_block);
      if (!next_node_iterator ||
          !next_node_iterator->IsDescendantOf(highest_root))
        break;
    }

    LayoutObject* const layout_object = next_node_iterator->GetLayoutObject();
    if (!layout_object) {
      next_node_iterator = Strategy::Next(*next_node_iterator, start_block);
      continue;
    }
    const ComputedStyle& style = layout_object->StyleRef();
    if (style.Visibility() != EVisibility::kVisible) {
      next_node_iterator = Strategy::Next(*next_node_iterator, start_block);
      continue;
    }

    if (layout_object->IsBR() || IsEnclosingBlock(next_node_iterator))
      break;

    // TODO(editing-dev): We avoid returning a position where the layoutObject
    // can't accept the caret.
    if (layout_object->IsText() &&
        ToLayoutText(layout_object)->ResolvedTextLength()) {
      LayoutText* const text = ToLayoutText(layout_object);
      if (style.PreserveNewline()) {
        const int length = ToLayoutText(layout_object)->TextLength();
        for (int i = (next_node_iterator == start_node ? candidate_offset : 0);
             i < length; ++i) {
          if ((*text)[i] == '\n') {
            return PositionTemplate<Strategy>(To<Text>(next_node_iterator),
                                              i + text->TextStartOffset());
          }
        }
      }

      candidate_node = next_node_iterator;
      candidate_type = PositionAnchorType::kOffsetInAnchor;
      candidate_offset =
          layout_object->CaretMaxOffset() + text->TextStartOffset();
      next_node_iterator = Strategy::Next(*next_node_iterator, start_block);
    } else if (EditingIgnoresContent(*next_node_iterator) ||
               IsDisplayInsideTable(next_node_iterator)) {
      candidate_node = next_node_iterator;
      candidate_type = PositionAnchorType::kAfterAnchor;
      next_node_iterator =
          Strategy::NextSkippingChildren(*next_node_iterator, start_block);
    } else {
      next_node_iterator = Strategy::Next(*next_node_iterator, start_block);
    }
  }

  if (candidate_type == PositionAnchorType::kOffsetInAnchor)
    return PositionTemplate<Strategy>(candidate_node, candidate_offset);

  return PositionTemplate<Strategy>(candidate_node, candidate_type);
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> EndOfParagraphAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  return CreateVisiblePosition(EndOfParagraphAlgorithm(
      visible_position.DeepEquivalent(), boundary_crossing_rule));
}

template <typename Strategy>
bool IsStartOfParagraphAlgorithm(
    const VisiblePositionTemplate<Strategy>& pos,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  DCHECK(pos.IsValid()) << pos;
  return pos.IsNotNull() &&
         pos.DeepEquivalent() ==
             StartOfParagraph(pos, boundary_crossing_rule).DeepEquivalent();
}

template <typename Strategy>
bool IsEndOfParagraphAlgorithm(
    const VisiblePositionTemplate<Strategy>& pos,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  DCHECK(pos.IsValid()) << pos;
  return pos.IsNotNull() &&
         pos.DeepEquivalent() ==
             EndOfParagraph(pos, boundary_crossing_rule).DeepEquivalent();
}

}  // namespace

VisiblePosition StartOfParagraph(
    const VisiblePosition& c,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  return StartOfParagraphAlgorithm<EditingStrategy>(c, boundary_crossing_rule);
}

VisiblePositionInFlatTree StartOfParagraph(
    const VisiblePositionInFlatTree& c,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  return StartOfParagraphAlgorithm<EditingInFlatTreeStrategy>(
      c, boundary_crossing_rule);
}

VisiblePosition EndOfParagraph(
    const VisiblePosition& c,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  return EndOfParagraphAlgorithm<EditingStrategy>(c, boundary_crossing_rule);
}

VisiblePositionInFlatTree EndOfParagraph(
    const VisiblePositionInFlatTree& c,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  return EndOfParagraphAlgorithm<EditingInFlatTreeStrategy>(
      c, boundary_crossing_rule);
}

// TODO(editing-dev): isStartOfParagraph(startOfNextParagraph(pos)) is not
// always true
VisiblePosition StartOfNextParagraph(const VisiblePosition& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  VisiblePosition paragraph_end(
      EndOfParagraph(visible_position, kCanSkipOverEditingBoundary));
  VisiblePosition after_paragraph_end(
      NextPositionOf(paragraph_end, kCannotCrossEditingBoundary));
  // It may happen that an element's next visually equivalent candidate is set
  // to such element when creating the VisualPosition. This may cause infinite
  // loops when we are iterating over parapgrahs.
  if (after_paragraph_end.DeepEquivalent() == paragraph_end.DeepEquivalent()) {
    after_paragraph_end = VisiblePosition::AfterNode(
        *paragraph_end.DeepEquivalent().AnchorNode());
  }
  // The position after the last position in the last cell of a table
  // is not the start of the next paragraph.
  if (TableElementJustBefore(after_paragraph_end))
    return NextPositionOf(after_paragraph_end, kCannotCrossEditingBoundary);
  return after_paragraph_end;
}

// TODO(editing-dev): isStartOfParagraph(startOfNextParagraph(pos)) is not
// always true
bool InSameParagraph(const VisiblePosition& a,
                     const VisiblePosition& b,
                     EditingBoundaryCrossingRule boundary_crossing_rule) {
  DCHECK(a.IsValid()) << a;
  DCHECK(b.IsValid()) << b;
  return a.IsNotNull() &&
         StartOfParagraph(a, boundary_crossing_rule).DeepEquivalent() ==
             StartOfParagraph(b, boundary_crossing_rule).DeepEquivalent();
}

bool IsStartOfParagraph(const VisiblePosition& pos,
                        EditingBoundaryCrossingRule boundary_crossing_rule) {
  return IsStartOfParagraphAlgorithm<EditingStrategy>(pos,
                                                      boundary_crossing_rule);
}

bool IsStartOfParagraph(const VisiblePositionInFlatTree& pos) {
  return IsStartOfParagraphAlgorithm<EditingInFlatTreeStrategy>(
      pos, kCannotCrossEditingBoundary);
}

bool IsEndOfParagraph(const VisiblePosition& pos,
                      EditingBoundaryCrossingRule boundary_crossing_rule) {
  return IsEndOfParagraphAlgorithm<EditingStrategy>(pos,
                                                    boundary_crossing_rule);
}

bool IsEndOfParagraph(const VisiblePositionInFlatTree& pos) {
  return IsEndOfParagraphAlgorithm<EditingInFlatTreeStrategy>(
      pos, kCannotCrossEditingBoundary);
}

EphemeralRange ExpandToParagraphBoundary(const EphemeralRange& range) {
  const VisiblePosition& start = CreateVisiblePosition(range.StartPosition());
  DCHECK(start.IsNotNull()) << range.StartPosition();
  const Position& paragraph_start = StartOfParagraph(start).DeepEquivalent();
  DCHECK(paragraph_start.IsNotNull()) << range.StartPosition();

  const VisiblePosition& end = CreateVisiblePosition(range.EndPosition());
  DCHECK(end.IsNotNull()) << range.EndPosition();
  const Position& paragraph_end = EndOfParagraph(end).DeepEquivalent();
  DCHECK(paragraph_end.IsNotNull()) << range.EndPosition();

  // TODO(editing-dev): There are some cases (crbug.com/640112) where we get
  // |paragraphStart > paragraphEnd|, which is the reason we cannot directly
  // return |EphemeralRange(paragraphStart, paragraphEnd)|. This is not
  // desired, though. We should do more investigation to ensure that why
  // |paragraphStart <= paragraphEnd| is violated.
  const Position& result_start =
      paragraph_start.IsNotNull() && paragraph_start <= range.StartPosition()
          ? paragraph_start
          : range.StartPosition();
  const Position& result_end =
      paragraph_end.IsNotNull() && paragraph_end >= range.EndPosition()
          ? paragraph_end
          : range.EndPosition();
  return EphemeralRange(result_start, result_end);
}

}  // namespace blink
