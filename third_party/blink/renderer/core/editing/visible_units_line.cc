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

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/editing/ng_flat_tree_shorthands.h"
#include "third_party/blink/renderer/core/editing/text_offset_mapping.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/layout/inline/inline_caret_position.h"
#include "third_party/blink/renderer/core/layout/inline/line_utils.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

struct VisualOrdering;

static PositionWithAffinity AdjustForSoftLineWrap(
    const InlineCursorPosition& line_box,
    const PositionWithAffinity& position) {
  DCHECK(line_box.IsLineBox());
  if (position.IsNull())
    return PositionWithAffinity();
  if (!line_box.Style().NeedsTrailingSpace() ||
      !line_box.HasSoftWrapToNextLine())
    return position;
  // Returns a position after first space causing soft line wrap for editable.
  if (!OffsetMapping::AcceptsPosition(position.GetPosition())) {
    return position;
  }
  const OffsetMapping* mapping = OffsetMapping::GetFor(position.GetPosition());
  if (!mapping) {
    // When |line_box| width has numeric overflow, |position| doesn't have
    // mapping. See http://crbug.com/1098795
    return position;
  }
  const auto offset = mapping->GetTextContentOffset(position.GetPosition());
  if (offset == mapping->GetText().length())
    return position;
  const Position adjusted_position = mapping->GetFirstPosition(*offset + 1);
  if (adjusted_position.IsNull())
    return position;
  if (!IsA<Text>(adjusted_position.AnchorNode()))
    return position;
  if (!adjusted_position.AnchorNode()
           ->GetLayoutObject()
           ->StyleRef()
           .IsCollapsibleWhiteSpace(mapping->GetText()[*offset]))
    return position;
  // See |TryResolveCaretPositionInTextFragment()| to locate upstream position
  // of caret after soft line wrap space.
  return PositionWithAffinity(adjusted_position,
                              TextAffinity::kUpstreamIfPossible);
}

template <typename Strategy, typename Ordering>
static PositionWithAffinityTemplate<Strategy> EndPositionForLine(
    const PositionWithAffinityTemplate<Strategy>& c) {
  if (c.IsNull())
    return PositionWithAffinityTemplate<Strategy>();
  const PositionWithAffinityTemplate<Strategy> adjusted =
      ComputeInlineAdjustedPosition(c);

  if (NGInlineFormattingContextOf(adjusted.GetPosition())) {
    DCHECK((std::is_same<Ordering, VisualOrdering>::value) ||
           !RuntimeEnabledFeatures::BidiCaretAffinityEnabled())
        << "Logical line boundary for BidiCaretAffinity is not implemented yet";

    const InlineCaretPosition caret_position =
        ComputeInlineCaretPosition(adjusted);
    if (caret_position.IsNull()) {
      // TODO(crbug.com/947593): Support |ComputeInlineCaretPosition()| on
      // content hidden by 'text-overflow:ellipsis' so that we always have a
      // non-null |caret_position| here.
      return PositionWithAffinityTemplate<Strategy>();
    }
    InlineCursor line_box = caret_position.cursor;
    line_box.MoveToContainingLine();
    const PositionWithAffinity end_position = line_box.PositionForEndOfLine();
    return FromPositionInDOMTree<Strategy>(
        AdjustForSoftLineWrap(line_box.Current(), end_position));
  }

  // There are VisiblePositions at offset 0 in blocks without line boxes, like
  // empty editable blocks and bordered blocks.
  const PositionTemplate<Strategy> p = c.GetPosition();
  if (p.AnchorNode()->GetLayoutObject() &&
      p.AnchorNode()->GetLayoutObject()->IsLayoutBlock() &&
      !p.ComputeEditingOffset()) {
    return c;
  }
  return PositionWithAffinityTemplate<Strategy>();
}

template <typename Strategy, typename Ordering>
PositionWithAffinityTemplate<Strategy> StartPositionForLine(
    const PositionWithAffinityTemplate<Strategy>& c) {
  if (c.IsNull())
    return PositionWithAffinityTemplate<Strategy>();
  const PositionWithAffinityTemplate<Strategy> adjusted =
      ComputeInlineAdjustedPosition(c);

  if (NGInlineFormattingContextOf(adjusted.GetPosition())) {
    DCHECK((std::is_same<Ordering, VisualOrdering>::value) ||
           !RuntimeEnabledFeatures::BidiCaretAffinityEnabled())
        << "Logical line boundary for BidiCaretAffinity is not implemented yet";

    const InlineCaretPosition caret_position =
        ComputeInlineCaretPosition(adjusted);
    if (caret_position.IsNull()) {
      // TODO(crbug.com/947593): Support |ComputeInlineCaretPosition()| on
      // content hidden by 'text-overflow:ellipsis' so that we always have a
      // non-null |caret_position| here.
      return PositionWithAffinityTemplate<Strategy>();
    }
    InlineCursor line_box = caret_position.cursor;
    line_box.MoveToContainingLine();
    DCHECK(line_box.Current().IsLineBox()) << line_box;
    return FromPositionInDOMTree<Strategy>(line_box.PositionForStartOfLine());
  }

  // There are VisiblePositions at offset 0 in blocks without line boxes, like
  // empty editable blocks and bordered blocks.
  PositionTemplate<Strategy> p = c.GetPosition();
  if (p.AnchorNode()->GetLayoutObject() &&
      p.AnchorNode()->GetLayoutObject()->IsLayoutBlock() &&
      !p.ComputeEditingOffset()) {
    return c;
  }

  return PositionWithAffinityTemplate<Strategy>();
}

// Provides start and end of line in logical order for implementing Home and End
// keys.
struct LogicalOrdering {
  // Make sure the end of line is at the same line as the given input
  // position. For a wrapping line, the logical end position for the
  // not-last-2-lines might incorrectly hand back the logical beginning of the
  // next line. For example,
  // <div contenteditable dir="rtl" style="line-break:before-white-space">xyz
  // a xyz xyz xyz xyz xyz xyz xyz xyz xyz xyz </div>
  // In this case, use the previous position of the computed logical end
  // position.
  template <typename Strategy>
  static PositionWithAffinityTemplate<Strategy> AdjustForSoftLineWrap(
      const PositionTemplate<Strategy>& candidate,
      const PositionWithAffinityTemplate<Strategy>& current_position) {
    const PositionWithAffinityTemplate<Strategy> candidate_position =
        PositionWithAffinityTemplate<Strategy>(
            candidate, TextAffinity::kUpstreamIfPossible);
    if (InSameLogicalLine(current_position, candidate_position))
      return candidate_position;
    return PreviousPositionOf(CreateVisiblePosition(candidate_position))
        .ToPositionWithAffinity();
  }
};

// Provides start end end of line in visual order for implementing expanding
// selection in line granularity.
struct VisualOrdering {
  // Make sure the end of line is at the same line as the given input
  // position. Else use the previous position to obtain end of line. This
  // condition happens when the input position is before the space character
  // at the end of a soft-wrapped non-editable line. In this scenario,
  // |EndPositionForLine()| would incorrectly hand back a position in the next
  // line instead. This fix is to account for the discrepancy between lines
  // with "webkit-line-break:after-white-space" style versus lines without
  // that style, which would break before a space by default.
  template <typename Strategy>
  static PositionWithAffinityTemplate<Strategy> AdjustForSoftLineWrap(
      const PositionTemplate<Strategy>& candidate,
      const PositionWithAffinityTemplate<Strategy>& current_position) {
    const PositionWithAffinityTemplate<Strategy> candidate_position =
        PositionWithAffinityTemplate<Strategy>(
            candidate, TextAffinity::kUpstreamIfPossible);
    if (InSameLine(current_position, candidate_position)) {
      return PositionWithAffinityTemplate<Strategy>(
          CreateVisiblePosition(candidate).DeepEquivalent(),
          TextAffinity::kUpstreamIfPossible);
    }
    const PositionWithAffinityTemplate<Strategy>& adjusted_position =
        PreviousPositionOf(CreateVisiblePosition(current_position))
            .ToPositionWithAffinity();
    if (adjusted_position.IsNull())
      return PositionWithAffinityTemplate<Strategy>();
    return EndPositionForLine<Strategy, VisualOrdering>(adjusted_position);
  }
};

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> StartOfLineAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& c) {
  // TODO: this is the current behavior that might need to be fixed.
  // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
  PositionWithAffinityTemplate<Strategy> vis_pos =
      StartPositionForLine<Strategy, VisualOrdering>(c);
  return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
      vis_pos, c.GetPosition());
}

bool IsInlineBlock(const LayoutBlockFlow* block_flow) {
  if (!block_flow) {
    return false;
  }
  return block_flow->StyleRef().Display() == EDisplay::kInlineBlock;
}

}  // namespace

PositionWithAffinity StartOfLine(const PositionWithAffinity& current_position) {
  return StartOfLineAlgorithm<EditingStrategy>(current_position);
}

PositionInFlatTreeWithAffinity StartOfLine(
    const PositionInFlatTreeWithAffinity& current_position) {
  return StartOfLineAlgorithm<EditingInFlatTreeStrategy>(current_position);
}

// FIXME: Rename this function to reflect the fact it ignores bidi levels.
VisiblePosition StartOfLine(const VisiblePosition& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      StartOfLine(current_position.ToPositionWithAffinity()));
}

VisiblePositionInFlatTree StartOfLine(
    const VisiblePositionInFlatTree& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      StartOfLine(current_position.ToPositionWithAffinity()));
}

template <typename Strategy>
static PositionWithAffinityTemplate<Strategy> LogicalStartOfLineAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& c) {
  // TODO: this is the current behavior that might need to be fixed.
  // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
  PositionWithAffinityTemplate<Strategy> vis_pos =
      StartPositionForLine<Strategy, LogicalOrdering>(c);

  if (ContainerNode* editable_root = HighestEditableRoot(c.GetPosition())) {
    if (!editable_root->contains(
            vis_pos.GetPosition().ComputeContainerNode())) {
      return PositionWithAffinityTemplate<Strategy>(
          PositionTemplate<Strategy>::FirstPositionInNode(*editable_root));
    }
  }

  return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
      vis_pos, c.GetPosition());
}

static PositionWithAffinity LogicalStartOfLine(
    const PositionWithAffinity& position) {
  return LogicalStartOfLineAlgorithm<EditingStrategy>(position);
}

static PositionInFlatTreeWithAffinity LogicalStartOfLine(
    const PositionInFlatTreeWithAffinity& position) {
  return LogicalStartOfLineAlgorithm<EditingInFlatTreeStrategy>(position);
}

VisiblePosition LogicalStartOfLine(const VisiblePosition& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      LogicalStartOfLine(current_position.ToPositionWithAffinity()));
}

VisiblePositionInFlatTree LogicalStartOfLine(
    const VisiblePositionInFlatTree& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      LogicalStartOfLine(current_position.ToPositionWithAffinity()));
}

// TODO(yosin) Rename this function to reflect the fact it ignores bidi levels.
template <typename Strategy>
static PositionWithAffinityTemplate<Strategy> EndOfLineAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& current_position) {
  // TODO(yosin) this is the current behavior that might need to be fixed.
  // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
  const PositionWithAffinityTemplate<Strategy>& candidate_position =
      EndPositionForLine<Strategy, VisualOrdering>(current_position);

  return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
      candidate_position, current_position.GetPosition());
}

PositionWithAffinity EndOfLine(const PositionWithAffinity& position) {
  return EndOfLineAlgorithm<EditingStrategy>(position);
}

PositionInFlatTreeWithAffinity EndOfLine(
    const PositionInFlatTreeWithAffinity& position) {
  return EndOfLineAlgorithm<EditingInFlatTreeStrategy>(position);
}

template <typename Strategy>
static bool InSameLogicalLine(
    const PositionWithAffinityTemplate<Strategy>& position1,
    const PositionWithAffinityTemplate<Strategy>& position2) {
  return position1.IsNotNull() &&
         LogicalStartOfLine(position1).GetPosition() ==
             LogicalStartOfLine(position2).GetPosition();
}

template <typename Strategy>
static PositionWithAffinityTemplate<Strategy> LogicalEndOfLineAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& current_position) {
  // TODO(yosin) this is the current behavior that might need to be fixed.
  // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
  const PositionWithAffinityTemplate<Strategy> candidate_position =
      EndPositionForLine<Strategy, LogicalOrdering>(current_position);

  if (ContainerNode* editable_root =
          HighestEditableRoot(current_position.GetPosition())) {
    if (!editable_root->contains(
            candidate_position.GetPosition().ComputeContainerNode())) {
      return PositionWithAffinityTemplate<Strategy>(
          PositionTemplate<Strategy>::LastPositionInNode(*editable_root));
    }
  }

  return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
      candidate_position, current_position.GetPosition());
}

static PositionWithAffinity LogicalEndOfLine(
    const PositionWithAffinity& position) {
  return LogicalEndOfLineAlgorithm<EditingStrategy>(position);
}

static PositionInFlatTreeWithAffinity LogicalEndOfLine(
    const PositionInFlatTreeWithAffinity& position) {
  return LogicalEndOfLineAlgorithm<EditingInFlatTreeStrategy>(position);
}

VisiblePosition LogicalEndOfLine(const VisiblePosition& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      LogicalEndOfLine(current_position.ToPositionWithAffinity()));
}

VisiblePositionInFlatTree LogicalEndOfLine(
    const VisiblePositionInFlatTree& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      LogicalEndOfLine(current_position.ToPositionWithAffinity()));
}

template <typename Strategy>
static bool InSameLineAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& position1,
    const PositionWithAffinityTemplate<Strategy>& position2) {
  if (position1.IsNull() || position2.IsNull())
    return false;
  DCHECK_EQ(position1.GetDocument(), position2.GetDocument());
  DCHECK(!position1.GetDocument()->NeedsLayoutTreeUpdate());

  const LayoutBlockFlow* block1 =
      NGInlineFormattingContextOf(position1.GetPosition());
  const LayoutBlockFlow* block2 =
      NGInlineFormattingContextOf(position2.GetPosition());
  if (block1 || block2) {
    if (RuntimeEnabledFeatures::InlineBlockInSameLineEnabled() &&
        (IsInlineBlock(block1) || IsInlineBlock(block2))) {
      const TextOffsetMapping::InlineContents inline_contents1 =
          TextOffsetMapping::FindForwardInlineContents(
              ToPositionInFlatTree(position1.GetPosition()));
      const TextOffsetMapping::InlineContents inline_contents2 =
          TextOffsetMapping::FindForwardInlineContents(
              ToPositionInFlatTree(position2.GetPosition()));
      if (inline_contents1 != inline_contents2) {
        return false;
      }
    } else {
      if (block1 != block2) {
        return false;
      }
      if (!InSameNGLineBox(position1, position2)) {
        return false;
      }
    }
    // See (ParameterizedVisibleUnitsLineTest.InSameLineWithMixedEditability
    return RootEditableElementOf(position1.GetPosition()) ==
           RootEditableElementOf(position2.GetPosition());
  }

  // Neither positions are in LayoutNG. Fall through to legacy handling.

  PositionWithAffinityTemplate<Strategy> start_of_line1 =
      StartOfLine(position1);
  PositionWithAffinityTemplate<Strategy> start_of_line2 =
      StartOfLine(position2);
  if (start_of_line1 == start_of_line2)
    return true;
  PositionTemplate<Strategy> canonicalized1 =
      CanonicalPositionOf(start_of_line1.GetPosition());
  if (canonicalized1 == start_of_line2.GetPosition())
    return true;
  return canonicalized1 == CanonicalPositionOf(start_of_line2.GetPosition());
}

bool InSameLine(const PositionWithAffinity& a, const PositionWithAffinity& b) {
  return InSameLineAlgorithm<EditingStrategy>(a, b);
}

bool InSameLine(const PositionInFlatTreeWithAffinity& position1,
                const PositionInFlatTreeWithAffinity& position2) {
  return InSameLineAlgorithm<EditingInFlatTreeStrategy>(position1, position2);
}

bool InSameLine(const VisiblePosition& position1,
                const VisiblePosition& position2) {
  DCHECK(position1.IsValid()) << position1;
  DCHECK(position2.IsValid()) << position2;
  return InSameLine(position1.ToPositionWithAffinity(),
                    position2.ToPositionWithAffinity());
}

bool InSameLine(const VisiblePositionInFlatTree& position1,
                const VisiblePositionInFlatTree& position2) {
  DCHECK(position1.IsValid()) << position1;
  DCHECK(position2.IsValid()) << position2;
  return InSameLine(position1.ToPositionWithAffinity(),
                    position2.ToPositionWithAffinity());
}

template <typename Strategy>
static bool IsStartOfLineAlgorithm(const VisiblePositionTemplate<Strategy>& p) {
  DCHECK(p.IsValid()) << p;
  return p.IsNotNull() && p.DeepEquivalent() == StartOfLine(p).DeepEquivalent();
}

bool IsStartOfLine(const VisiblePosition& p) {
  return IsStartOfLineAlgorithm<EditingStrategy>(p);
}

bool IsStartOfLine(const VisiblePositionInFlatTree& p) {
  return IsStartOfLineAlgorithm<EditingInFlatTreeStrategy>(p);
}

template <typename Strategy>
static bool IsEndOfLineAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  if (visible_position.IsNull())
    return false;
  const auto& end_of_line =
      EndOfLine(visible_position.ToPositionWithAffinity());
  return visible_position.DeepEquivalent() == end_of_line.GetPosition();
}

bool IsEndOfLine(const VisiblePosition& p) {
  return IsEndOfLineAlgorithm<EditingStrategy>(p);
}

bool IsEndOfLine(const VisiblePositionInFlatTree& p) {
  return IsEndOfLineAlgorithm<EditingInFlatTreeStrategy>(p);
}

template <typename Strategy>
static bool IsLogicalEndOfLineAlgorithm(
    const VisiblePositionTemplate<Strategy>& p) {
  DCHECK(p.IsValid()) << p;
  return p.IsNotNull() &&
         p.DeepEquivalent() == LogicalEndOfLine(p).DeepEquivalent();
}

bool IsLogicalEndOfLine(const VisiblePosition& p) {
  return IsLogicalEndOfLineAlgorithm<EditingStrategy>(p);
}

bool IsLogicalEndOfLine(const VisiblePositionInFlatTree& p) {
  return IsLogicalEndOfLineAlgorithm<EditingInFlatTreeStrategy>(p);
}

}  // namespace blink
