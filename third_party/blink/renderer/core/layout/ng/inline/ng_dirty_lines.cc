// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_dirty_lines.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"

namespace blink {

void NGDirtyLines::MarkLastFragment() {
  if (last_fragment_) {
    // Changes in this LayoutObject may affect the line that contains its
    // previous object. Mark the line box that contains the last fragment
    // of the previous object.
    last_fragment_->LastForSameLayoutObject()->MarkContainingLineBoxDirty();
  } else {
    // If there were no fragments so far in this pre-order traversal, mark
    // the first line box dirty.
    DCHECK(block_fragment_);
    if (NGPaintFragment* first_line = block_fragment_->FirstLineBox())
      first_line->MarkLineBoxDirty();
  }
}

void NGDirtyLines::MarkAtTextOffset(unsigned offset) {
  for (NGPaintFragment* child : block_fragment_->Children()) {
    // Only the first dirty line is relevant.
    if (child->IsDirty())
      break;

    const auto* line =
        DynamicTo<NGPhysicalLineBoxFragment>(child->PhysicalFragment());
    if (!line)
      continue;

    const auto* break_token = To<NGInlineBreakToken>(line->BreakToken());
    DCHECK(break_token);
    if (break_token->IsFinished())
      break;

    if (offset < break_token->TextOffset()) {
      child->MarkLineBoxDirty();
      break;
    }
  }
}

}  // namespace blink
