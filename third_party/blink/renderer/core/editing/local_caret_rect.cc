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

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/editing/ng_flat_tree_shorthands.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

namespace blink {

namespace {

template <typename Strategy>
LocalCaretRect LocalCaretRectOfPositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position,
    LayoutUnit* extra_width_to_end_of_line) {
  if (position.IsNull())
    return LocalCaretRect();
  Node* const node = position.AnchorNode();
  LayoutObject* const layout_object = node->GetLayoutObject();
  if (!layout_object)
    return LocalCaretRect();

  const PositionWithAffinityTemplate<Strategy>& adjusted =
      ComputeInlineAdjustedPosition(position);

  if (adjusted.IsNotNull()) {
    if (NGInlineFormattingContextOf(adjusted.GetPosition()))
      return ComputeNGLocalCaretRect(adjusted);

    const InlineBoxPosition& box_position =
        ComputeInlineBoxPositionForInlineAdjustedPosition(adjusted);

    if (box_position.inline_box) {
      const LayoutObject* box_layout_object =
          LineLayoutAPIShim::LayoutObjectFrom(
              box_position.inline_box->GetLineLayoutItem());
      return LocalCaretRect(
          box_layout_object,
          box_layout_object->PhysicalLocalCaretRect(
              box_position.inline_box, box_position.offset_in_box,
              extra_width_to_end_of_line));
    }
  }

  // DeleteSelectionCommandTest.deleteListFromTable goes here.
  return LocalCaretRect(
      layout_object, layout_object->PhysicalLocalCaretRect(
                         nullptr, position.GetPosition().ComputeEditingOffset(),
                         extra_width_to_end_of_line));
}

// This function was added because the caret rect that is calculated by
// using the line top value instead of the selection top.
template <typename Strategy>
LocalCaretRect LocalSelectionRectOfPositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position) {
  if (position.IsNull())
    return LocalCaretRect();
  Node* const node = position.AnchorNode();
  if (!node->GetLayoutObject())
    return LocalCaretRect();

  const PositionWithAffinityTemplate<Strategy>& adjusted =
      ComputeInlineAdjustedPosition(position);
  if (adjusted.IsNull())
    return LocalCaretRect();

  if (NGInlineFormattingContextOf(adjusted.GetPosition())) {
    return ComputeNGLocalSelectionRect(adjusted);
  }

  const InlineBoxPosition& box_position =
      ComputeInlineBoxPositionForInlineAdjustedPosition(adjusted);

  if (!box_position.inline_box)
    return LocalCaretRect();

  LayoutObject* const layout_object = LineLayoutAPIShim::LayoutObjectFrom(
      box_position.inline_box->GetLineLayoutItem());

  LayoutRect rect = layout_object->LocalCaretRect(box_position.inline_box,
                                                  box_position.offset_in_box);

  if (rect.IsEmpty())
    return LocalCaretRect();

  const InlineBox* const box = box_position.inline_box;
  if (layout_object->IsHorizontalWritingMode()) {
    rect.SetY(box->Root().SelectionTop());
    rect.SetHeight(box->Root().SelectionHeight());
  } else {
    rect.SetX(box->Root().SelectionTop());
    rect.SetHeight(box->Root().SelectionHeight());
  }
  return LocalCaretRect(layout_object, layout_object->FlipForWritingMode(rect));
}

}  // namespace

LocalCaretRect LocalCaretRectOfPosition(
    const PositionWithAffinity& position,
    LayoutUnit* extra_width_to_end_of_line) {
  return LocalCaretRectOfPositionTemplate<EditingStrategy>(
      position, extra_width_to_end_of_line);
}

LocalCaretRect LocalCaretRectOfPosition(
    const PositionInFlatTreeWithAffinity& position) {
  return LocalCaretRectOfPositionTemplate<EditingInFlatTreeStrategy>(position,
                                                                     nullptr);
}

LocalCaretRect LocalSelectionRectOfPosition(
    const PositionWithAffinity& position) {
  return LocalSelectionRectOfPositionTemplate<EditingStrategy>(position);
}

// ----

template <typename Strategy>
static IntRect AbsoluteCaretBoundsOfAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& position,
    LayoutUnit* extra_width_to_end_of_line = nullptr) {
  const LocalCaretRect& caret_rect = LocalCaretRectOfPositionTemplate<Strategy>(
      position, extra_width_to_end_of_line);
  if (caret_rect.IsEmpty())
    return IntRect();
  return LocalToAbsoluteQuadOf(caret_rect).EnclosingBoundingBox();
}

IntRect AbsoluteCaretBoundsOf(const PositionWithAffinity& position,
                              LayoutUnit* extra_width_to_end_of_line) {
  return AbsoluteCaretBoundsOfAlgorithm<EditingStrategy>(
      position, extra_width_to_end_of_line);
}

template <typename Strategy>
static IntRect AbsoluteSelectionBoundsOfAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  const LocalCaretRect& caret_rect =
      LocalSelectionRectOfPosition(visible_position.ToPositionWithAffinity());
  if (caret_rect.IsEmpty())
    return IntRect();
  return LocalToAbsoluteQuadOf(caret_rect).EnclosingBoundingBox();
}

IntRect AbsoluteSelectionBoundsOf(const VisiblePosition& visible_position) {
  return AbsoluteSelectionBoundsOfAlgorithm<EditingStrategy>(visible_position);
}

IntRect AbsoluteCaretBoundsOf(const PositionInFlatTreeWithAffinity& position) {
  return AbsoluteCaretBoundsOfAlgorithm<EditingInFlatTreeStrategy>(position);
}

}  // namespace blink
