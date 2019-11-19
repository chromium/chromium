// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_utils.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_position.h"

namespace blink {

NGInlineCursor NGContainingLineBoxOf(const PositionWithAffinity& position) {
  const NGCaretPosition caret_position = ComputeNGCaretPosition(position);
  if (caret_position.IsNull())
    return NGInlineCursor();
  NGInlineCursor line = caret_position.cursor;
  line.MoveToContainingLine();
  return line;
}

bool InSameNGLineBox(const PositionWithAffinity& position1,
                     const PositionWithAffinity& position2) {
  const NGInlineCursor& line_box1 = NGContainingLineBoxOf(position1);
  if (!line_box1)
    return false;

  const NGInlineCursor& line_box2 = NGContainingLineBoxOf(position2);
  return line_box1 == line_box2;
}

}  // namespace blink
