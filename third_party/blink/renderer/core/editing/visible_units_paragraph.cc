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
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position_units.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// VisiblePosition overloads. Position-based implementations live in
// position_units_paragraph.cc.

VisiblePosition StartOfParagraph(
    const VisiblePosition& c,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  DCHECK(c.IsValid()) << c;
  const Position& start =
      StartOfParagraph(c.DeepEquivalent(), boundary_crossing_rule);
#if DCHECK_IS_ON()
  if (start.IsNotNull() && c.IsNotNull())
    DCHECK_LE(start, c.DeepEquivalent());
#endif
  return CreateVisiblePosition(start);
}

VisiblePosition StartOfParagraphInFlatTree(
    const VisiblePosition& pos,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  const PositionInFlatTree flat_pos =
      ToPositionInFlatTree(pos.DeepEquivalent());
  const PositionInFlatTree start =
      StartOfParagraph(flat_pos, boundary_crossing_rule);
  return CreateVisiblePosition(ToPositionInDOMTree(start));
}

VisiblePositionInFlatTree StartOfParagraph(
    const VisiblePositionInFlatTree& c,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  DCHECK(c.IsValid()) << c;
  const PositionInFlatTree& start =
      StartOfParagraph(c.DeepEquivalent(), boundary_crossing_rule);
#if DCHECK_IS_ON()
  if (start.IsNotNull() && c.IsNotNull())
    DCHECK_LE(start, c.DeepEquivalent());
#endif
  return CreateVisiblePosition(start);
}

VisiblePosition EndOfParagraph(
    const VisiblePosition& c,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  DCHECK(c.IsValid()) << c;
  const Position& end =
      EndOfParagraph(c.DeepEquivalent(), boundary_crossing_rule);
#if DCHECK_IS_ON()
  if (end.IsNotNull() && c.IsNotNull())
    DCHECK_GE(end, c.DeepEquivalent());
#endif
  return CreateVisiblePosition(end);
}

VisiblePosition EndOfParagraphInFlatTree(
    const VisiblePosition& pos,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  const PositionInFlatTree flat_pos =
      ToPositionInFlatTree(pos.DeepEquivalent());
  const PositionInFlatTree end =
      EndOfParagraph(flat_pos, boundary_crossing_rule);
  return CreateVisiblePosition(ToPositionInDOMTree(end));
}

VisiblePositionInFlatTree EndOfParagraph(
    const VisiblePositionInFlatTree& c,
    EditingBoundaryCrossingRule boundary_crossing_rule) {
  DCHECK(c.IsValid()) << c;
  const PositionInFlatTree& end =
      EndOfParagraph(c.DeepEquivalent(), boundary_crossing_rule);
#if DCHECK_IS_ON()
  if (end.IsNotNull() && c.IsNotNull())
    DCHECK_GE(end, c.DeepEquivalent());
#endif
  return CreateVisiblePosition(end);
}

// TODO(editing-dev): isStartOfParagraph(startOfNextParagraph(pos)) is not
// always true
VisiblePosition StartOfNextParagraph(const VisiblePosition& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  Position paragraph_end(EndOfParagraph(visible_position.DeepEquivalent(),
                                        kCanSkipOverEditingBoundary));
  // EndOfParagraph preserves the candidate_type, so if we are already at the
  // end node we must ensure we get the next position to avoid infinite loops.
  if (paragraph_end == visible_position.DeepEquivalent()) {
    paragraph_end =
        Position::AfterNode(*visible_position.DeepEquivalent().AnchorNode());
  }
  DCHECK(!paragraph_end.IsBeforeAnchor());
  DCHECK(visible_position.DeepEquivalent() < paragraph_end ||
         visible_position.DeepEquivalent() == paragraph_end &&
             paragraph_end.IsAfterAnchor());
  VisiblePosition after_paragraph_end = CreateVisiblePosition(
      NextPositionOf(paragraph_end, kCannotCrossEditingBoundary));
  // It may happen that an element's next visually equivalent candidate is set
  // to such element when creating the VisualPosition. This may cause infinite
  // loops when we are iterating over parapgrahs.
  if (after_paragraph_end.DeepEquivalent() == paragraph_end) {
    after_paragraph_end =
        VisiblePosition::AfterNode(*paragraph_end.AnchorNode());
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
  DCHECK(pos.IsValid()) << pos;
  return pos.IsNotNull() &&
         pos.DeepEquivalent() ==
             StartOfParagraph(pos, boundary_crossing_rule).DeepEquivalent();
}

bool IsStartOfParagraph(const VisiblePositionInFlatTree& pos) {
  DCHECK(pos.IsValid()) << pos;
  return pos.IsNotNull() &&
         pos.DeepEquivalent() ==
             StartOfParagraph(pos, kCannotCrossEditingBoundary)
                 .DeepEquivalent();
}

bool IsEndOfParagraph(const VisiblePosition& pos,
                      EditingBoundaryCrossingRule boundary_crossing_rule) {
  DCHECK(pos.IsValid()) << pos;
  return pos.IsNotNull() &&
         pos.DeepEquivalent() ==
             EndOfParagraph(pos, boundary_crossing_rule).DeepEquivalent();
}

bool IsEndOfParagraph(const VisiblePositionInFlatTree& pos) {
  DCHECK(pos.IsValid()) << pos;
  return pos.IsNotNull() &&
         pos.DeepEquivalent() ==
             EndOfParagraph(pos, kCannotCrossEditingBoundary).DeepEquivalent();
}

}  // namespace blink
