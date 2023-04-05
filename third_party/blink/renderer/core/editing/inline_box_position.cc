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
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"

namespace blink {

namespace {

const int kBlockFlowAdjustmentMaxRecursionDepth = 256;

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

}  // namespace

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

}  // namespace blink
