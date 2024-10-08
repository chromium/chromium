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

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/editing/ng_flat_tree_shorthands.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/layout/inline/caret_rect.h"
#include "third_party/blink/renderer/core/layout/inline/inline_caret_position.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

// Returns a position suitable for |ComputeNGCaretPosition()| to calculate
// local caret rect by |ComputeLocalCaretRect()|:
//  - A position in |Text| node
//  - A position before/after atomic inline element. Note: This function
//    doesn't check whether anchor node is atomic inline level or not.
template <typename Strategy>
PositionWithAffinityTemplate<Strategy> AdjustForInlineCaretPosition(
    const PositionWithAffinityTemplate<Strategy>& position_with_affinity) {
  switch (position_with_affinity.GetPosition().AnchorType()) {
    case PositionAnchorType::kAfterAnchor:
    case PositionAnchorType::kBeforeAnchor:
      return position_with_affinity;
    case PositionAnchorType::kAfterChildren:
      // For caret rect computation, |kAfterChildren| and |kAfterNode| are
      // equivalent. See http://crbug.com/1174101
      return PositionWithAffinityTemplate<Strategy>(
          PositionTemplate<Strategy>::AfterNode(
              *position_with_affinity.GetPosition().AnchorNode()),
          position_with_affinity.Affinity());
    case PositionAnchorType::kOffsetInAnchor: {
      const Node& node = *position_with_affinity.GetPosition().AnchorNode();
      if (IsA<Text>(node) ||
          position_with_affinity.GetPosition().OffsetInContainerNode())
        return position_with_affinity;
      const LayoutObject* const layout_object = node.GetLayoutObject();
      if (!layout_object || IsA<LayoutBlockFlow>(layout_object)) {
        // In case of <div>@0
        return position_with_affinity;
      }
      // For caret rect computation, we paint caret before |layout_object|
      // instead of inside of it.
      return PositionWithAffinityTemplate<Strategy>(
          PositionTemplate<Strategy>::BeforeNode(node),
          position_with_affinity.Affinity());
    }
  }
  NOTREACHED_IN_MIGRATION();
  return position_with_affinity;
}

template <typename Strategy>
LocalCaretRect LocalCaretRectOfPositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position,
    EditingBoundaryCrossingRule rule) {
  if (position.IsNull())
    return LocalCaretRect();
  Node* const node = position.AnchorNode();
  LayoutObject* const layout_object = node->GetLayoutObject();
  if (!layout_object)
    return LocalCaretRect();

  // If the `position` is for `LayoutText` or before/after inline boxes, let
  // `ComputeLocalCaretRect` compute.
  const PositionWithAffinityTemplate<Strategy>& adjusted =
      ComputeInlineAdjustedPosition(position, rule);
  if (adjusted.IsNotNull()) {
    if (auto caret_position = ComputeInlineCaretPosition(
            AdjustForInlineCaretPosition(adjusted))) {
      return ComputeLocalCaretRect(caret_position);
    }
  }

  // If the caret is in an empty `LayoutBlockFlow`, and if it is block-
  // fragmented, set the first fragment to prevent rendering multiple carets in
  // following fragments.
  const PhysicalBoxFragment* root_box_fragment = nullptr;
  if (position.GetPosition().IsOffsetInAnchor() &&
      !position.GetPosition().OffsetInContainerNode()) {
    if (const auto* block_flow = DynamicTo<LayoutBlockFlow>(layout_object)) {
      if (!block_flow->FirstChild() &&
          block_flow->PhysicalFragmentCount() >= 2) {
        root_box_fragment = block_flow->GetPhysicalFragment(0);
      }
    }
  }

  return LocalCaretRect(layout_object,
                        layout_object->LocalCaretRect(
                            position.GetPosition().ComputeEditingOffset()),
                        root_box_fragment);
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

  if (auto caret_position =
          ComputeInlineCaretPosition(AdjustForInlineCaretPosition(adjusted))) {
    return ComputeLocalSelectionRect(caret_position);
  }

  return LocalCaretRect();
}

}  // namespace

LocalCaretRect LocalCaretRectOfPosition(const PositionWithAffinity& position,
                                        EditingBoundaryCrossingRule rule) {
  return LocalCaretRectOfPositionTemplate<EditingStrategy>(position, rule);
}

LocalCaretRect LocalCaretRectOfPosition(
    const PositionInFlatTreeWithAffinity& position,
    EditingBoundaryCrossingRule rule) {
  return LocalCaretRectOfPositionTemplate<EditingInFlatTreeStrategy>(position,
                                                                     rule);
}

LocalCaretRect LocalSelectionRectOfPosition(
    const PositionWithAffinity& position) {
  return LocalSelectionRectOfPositionTemplate<EditingStrategy>(position);
}

// ----

template <typename Strategy>
static gfx::Rect AbsoluteCaretBoundsOfAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& position,
    EditingBoundaryCrossingRule rule) {
  const LocalCaretRect& caret_rect =
      LocalCaretRectOfPositionTemplate<Strategy>(position, rule);
  if (caret_rect.IsEmpty())
    return gfx::Rect();
  return gfx::ToEnclosingRect(LocalToAbsoluteQuadOf(caret_rect).BoundingBox());
}

gfx::Rect AbsoluteCaretBoundsOf(const PositionWithAffinity& position,
                                EditingBoundaryCrossingRule rule) {
  return AbsoluteCaretBoundsOfAlgorithm<EditingStrategy>(position, rule);
}

template <typename Strategy>
static gfx::Rect AbsoluteSelectionBoundsOfAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  const LocalCaretRect& caret_rect =
      LocalSelectionRectOfPosition(visible_position.ToPositionWithAffinity());
  if (caret_rect.IsEmpty())
    return gfx::Rect();
  return gfx::ToEnclosingRect(LocalToAbsoluteQuadOf(caret_rect).BoundingBox());
}

gfx::Rect AbsoluteSelectionBoundsOf(const VisiblePosition& visible_position) {
  return AbsoluteSelectionBoundsOfAlgorithm<EditingStrategy>(visible_position);
}

gfx::Rect AbsoluteCaretBoundsOf(
    const PositionInFlatTreeWithAffinity& position) {
  return AbsoluteCaretBoundsOfAlgorithm<EditingInFlatTreeStrategy>(
      position, kCanCrossEditingBoundary);
}

}  // namespace blink
