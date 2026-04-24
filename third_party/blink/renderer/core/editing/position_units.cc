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
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"

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

}  // namespace blink
