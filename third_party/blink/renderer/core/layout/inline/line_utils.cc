// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/line_utils.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/inline/caret_position.h"

namespace blink {

namespace {

void TrimLeadingSpace(LayoutUnit& leading_space) {
  leading_space = LayoutUnit();
}

}  // namespace

InlineCursor NGContainingLineBoxOf(const PositionWithAffinity& position) {
  const CaretPosition caret_position = ComputeCaretPosition(position);
  if (caret_position.IsNull())
    return InlineCursor();
  InlineCursor line = caret_position.cursor;
  line.MoveToContainingLine();
  return line;
}

bool InSameNGLineBox(const PositionWithAffinity& position1,
                     const PositionWithAffinity& position2) {
  const InlineCursor& line_box1 = NGContainingLineBoxOf(position1);
  if (!line_box1)
    return false;

  const InlineCursor& line_box2 = NGContainingLineBoxOf(position2);
  return line_box1 == line_box2;
}

FontHeight CalculateLeadingSpace(const LayoutUnit& line_height,
                                 const FontHeight& current_height,
                                 const ETextBoxTrim& text_box_trim,
                                 WritingMode writing_mode) {
  // TODO(kojii): floor() is to make text dump compatible with legacy test
  // results. Revisit when we paint.
  LayoutUnit ascent_leading_spacing{
      ((line_height - current_height.LineHeight()) / 2).Floor()};
  LayoutUnit descent_leading_spacing =
      line_height - current_height.LineHeight() - ascent_leading_spacing;

  // TODO(https://crbug.com/1278634): Trim it to the specified edges instead.
  if (UNLIKELY(text_box_trim == ETextBoxTrim::kBoth ||
               text_box_trim == ETextBoxTrim::kStart)) {
    TrimLeadingSpace(IsFlippedLinesWritingMode(writing_mode)
                         ? descent_leading_spacing
                         : ascent_leading_spacing);
  }
  if (UNLIKELY(text_box_trim == ETextBoxTrim::kBoth ||
               text_box_trim == ETextBoxTrim::kEnd)) {
    TrimLeadingSpace(IsFlippedLinesWritingMode(writing_mode)
                         ? ascent_leading_spacing
                         : descent_leading_spacing);
  }
  return FontHeight(ascent_leading_spacing, descent_leading_spacing);
}

}  // namespace blink
