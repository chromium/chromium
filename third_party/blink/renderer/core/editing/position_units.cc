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

#include "third_party/blink/renderer/core/editing/position_units.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"

namespace blink {

// ----- document -----

namespace {

template <typename Strategy>
PositionTemplate<Strategy> StartOfDocumentAlgorithm(
    const PositionTemplate<Strategy>& position) {
  const Node* const node = position.AnchorNode();
  if (!node || !node->GetDocument().documentElement()) {
    return PositionTemplate<Strategy>();
  }
  return PositionTemplate<Strategy>::FirstPositionInNode(
      *node->GetDocument().documentElement());
}

template <typename Strategy>
PositionTemplate<Strategy> EndOfDocumentAlgorithm(
    const PositionTemplate<Strategy>& position) {
  const Node* node = position.AnchorNode();
  if (!node || !node->GetDocument().documentElement()) {
    return PositionTemplate<Strategy>();
  }
  return PositionTemplate<Strategy>::LastPositionInNode(
      *node->GetDocument().documentElement());
}

template <typename Strategy>
PositionTemplate<Strategy> NextPositionOfAlgorithm(
    const PositionTemplate<Strategy>& position,
    EditingBoundaryCrossingRule rule) {
  const PositionTemplate<Strategy> next =
      NextVisuallyDistinctCandidate(position, rule);
  if (next.IsNull()) {
    return PositionTemplate<Strategy>();
  }

  switch (rule) {
    case kCanCrossEditingBoundary:
      return next;
    case kCannotCrossEditingBoundary:
      return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
                 PositionWithAffinityTemplate<Strategy>(next), position)
          .GetPosition();
    case kCanSkipOverEditingBoundary:
      return SkipToEndOfEditingBoundary(next, position);
  }
  NOTREACHED();
}

template <typename Strategy>
PositionTemplate<Strategy> PreviousPositionOfAlgorithm(
    const PositionTemplate<Strategy>& position,
    EditingBoundaryCrossingRule rule) {
  const PositionTemplate<Strategy> prev =
      PreviousVisuallyDistinctCandidate(position, rule);
  // Unlike NextVisuallyDistinctCandidate (which returns null at tree end),
  // the backward variant can return a non-null position before <html>.
  if (prev.IsNull() || prev.AtStartOfTree()) {
    return PositionTemplate<Strategy>();
  }
  // The backward variant can also stall, returning the same position when
  // already at a boundary. Treat as no previous position to avoid looping.
  if (prev == position) {
    return PositionTemplate<Strategy>();
  }

  switch (rule) {
    case kCanCrossEditingBoundary:
      return prev;
    case kCannotCrossEditingBoundary:
      return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
                 PositionWithAffinityTemplate<Strategy>(prev), position)
          .GetPosition();
    case kCanSkipOverEditingBoundary:
      return SkipToStartOfEditingBoundary(prev, position);
  }
  NOTREACHED();
}

template <typename Strategy>
UChar32 CharacterAfterAlgorithm(const PositionTemplate<Strategy>& position) {
  // Canonicalize forward — the character is in the text node after this
  // position once we resolve collapsible whitespace.
  const PositionTemplate<Strategy> canonical_position =
      MostForwardCaretPosition(position);
  if (!canonical_position.IsOffsetInAnchor()) {
    return 0;
  }

  auto* text_node = DynamicTo<Text>(canonical_position.ComputeContainerNode());
  if (!text_node) {
    return 0;
  }

  const unsigned offset =
      static_cast<unsigned>(canonical_position.OffsetInContainerNode());
  const unsigned length = text_node->length();
  if (offset >= length) {
    return 0;
  }

  return text_node->data().CodePointAtOrZero(offset);
}

template <typename Strategy>
PositionTemplate<Strategy> SkipToEndOfEditingBoundaryAlgorithm(
    const PositionTemplate<Strategy>& pos,
    const PositionTemplate<Strategy>& anchor) {
  if (pos.IsNull()) {
    return pos;
  }

  ContainerNode* highest_root = HighestEditableRoot(anchor);
  ContainerNode* highest_root_of_pos = HighestEditableRoot(pos);

  if (highest_root_of_pos == highest_root) {
    return pos;
  }

  if (!highest_root && highest_root_of_pos) {
    return PositionTemplate<Strategy>(highest_root_of_pos,
                                      PositionAnchorType::kAfterAnchor)
        .ParentAnchoredEquivalent();
  }

  DCHECK(highest_root);
  return FirstEditablePositionAfterPositionInRoot(pos, *highest_root);
}

template <typename Strategy>
PositionTemplate<Strategy> SkipToStartOfEditingBoundaryAlgorithm(
    const PositionTemplate<Strategy>& pos,
    const PositionTemplate<Strategy>& anchor) {
  if (pos.IsNull()) {
    return pos;
  }

  ContainerNode* highest_root = HighestEditableRoot(anchor);
  ContainerNode* highest_root_of_pos = HighestEditableRoot(pos);

  if (highest_root_of_pos == highest_root) {
    return pos;
  }

  if (!highest_root && highest_root_of_pos) {
    return PreviousVisuallyDistinctCandidate(
        PositionTemplate<Strategy>(highest_root_of_pos,
                                   PositionAnchorType::kBeforeAnchor)
            .ParentAnchoredEquivalent());
  }

  DCHECK(highest_root);
  return LastEditablePositionBeforePositionInRoot(pos, *highest_root);
}

}  // namespace

Position StartOfDocument(const Position& position) {
  return StartOfDocumentAlgorithm<EditingStrategy>(position);
}

PositionInFlatTree StartOfDocument(const PositionInFlatTree& position) {
  return StartOfDocumentAlgorithm<EditingInFlatTreeStrategy>(position);
}

Position EndOfDocument(const Position& position) {
  return EndOfDocumentAlgorithm<EditingStrategy>(position);
}

PositionInFlatTree EndOfDocument(const PositionInFlatTree& position) {
  return EndOfDocumentAlgorithm<EditingInFlatTreeStrategy>(position);
}

bool IsStartOfDocument(const Position& position) {
  return position.IsNotNull() && position == StartOfDocument(position);
}

bool IsEndOfDocument(const Position& position) {
  return position.IsNotNull() && position == EndOfDocument(position);
}

// ----- editable content -----

PositionInFlatTree StartOfEditableContent(const PositionInFlatTree& position) {
  ContainerNode* highest_root = HighestEditableRoot(position);
  if (!highest_root) {
    return PositionInFlatTree();
  }

  return PositionInFlatTree::FirstPositionInNode(*highest_root);
}

PositionInFlatTree EndOfEditableContent(const PositionInFlatTree& position) {
  ContainerNode* highest_root = HighestEditableRoot(position);
  if (!highest_root) {
    return PositionInFlatTree();
  }

  return PositionInFlatTree::LastPositionInNode(*highest_root);
}

Position StartOfEditableContent(const Position& position) {
  return ToPositionInDOMTree(
      StartOfEditableContent(ToPositionInFlatTree(position)));
}

Position EndOfEditableContent(const Position& position) {
  return ToPositionInDOMTree(
      EndOfEditableContent(ToPositionInFlatTree(position)));
}

bool IsEndOfEditableOrNonEditableContent(const Position& position) {
  if (position.IsNull()) {
    return false;
  }
  ContainerNode* highest_root = HighestEditableRoot(position);
  if (highest_root) {
    return position == Position::LastPositionInNode(*highest_root);
  }
  return IsEndOfDocument(position);
}

bool IsEndOfEditableOrNonEditableContent(const PositionInFlatTree& position) {
  if (position.IsNull()) {
    return false;
  }
  ContainerNode* highest_root = HighestEditableRoot(position);
  if (highest_root) {
    return position == PositionInFlatTree::LastPositionInNode(*highest_root);
  }
  return position == EndOfDocument(position);
}

Position NextPositionOf(const Position& position,
                        EditingBoundaryCrossingRule rule) {
  DCHECK(position.IsValidFor(*position.GetDocument())) << position;
  return NextPositionOfAlgorithm<EditingStrategy>(position, rule);
}

PositionInFlatTree NextPositionOf(const PositionInFlatTree& position,
                                  EditingBoundaryCrossingRule rule) {
  DCHECK(position.IsValidFor(*position.GetDocument())) << position;
  return NextPositionOfAlgorithm<EditingInFlatTreeStrategy>(position, rule);
}

Position PreviousPositionOf(const Position& position,
                            EditingBoundaryCrossingRule rule) {
  DCHECK(position.IsValidFor(*position.GetDocument())) << position;
  return PreviousPositionOfAlgorithm<EditingStrategy>(position, rule);
}

PositionInFlatTree PreviousPositionOf(const PositionInFlatTree& position,
                                      EditingBoundaryCrossingRule rule) {
  DCHECK(position.IsValidFor(*position.GetDocument())) << position;
  return PreviousPositionOfAlgorithm<EditingInFlatTreeStrategy>(position, rule);
}

UChar32 CharacterAfter(const Position& position) {
  return CharacterAfterAlgorithm(position);
}

UChar32 CharacterAfter(const PositionInFlatTree& position) {
  return CharacterAfterAlgorithm(position);
}

Position SkipToEndOfEditingBoundary(const Position& pos,
                                    const Position& anchor) {
  return SkipToEndOfEditingBoundaryAlgorithm<EditingStrategy>(pos, anchor);
}

PositionInFlatTree SkipToEndOfEditingBoundary(
    const PositionInFlatTree& pos,
    const PositionInFlatTree& anchor) {
  return SkipToEndOfEditingBoundaryAlgorithm<EditingInFlatTreeStrategy>(pos,
                                                                        anchor);
}

Position SkipToStartOfEditingBoundary(const Position& pos,
                                      const Position& anchor) {
  return SkipToStartOfEditingBoundaryAlgorithm<EditingStrategy>(pos, anchor);
}

PositionInFlatTree SkipToStartOfEditingBoundary(
    const PositionInFlatTree& pos,
    const PositionInFlatTree& anchor) {
  return SkipToStartOfEditingBoundaryAlgorithm<EditingInFlatTreeStrategy>(
      pos, anchor);
}

}  // namespace blink
