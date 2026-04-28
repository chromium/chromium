// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/logical_fragment_link.h"

#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {

void LogicalFragmentLink::ReverseChildOffset(
    const WritingDirectionMode& writing_direction,
    bool is_block_direction,
    LayoutUnit container_size_in_reversal_axis,
    LayoutUnit border_scrollbar_padding_start) {
  // Reverse the child across the container in the reversal axis.
  const auto& box_fragment = To<PhysicalBoxFragment>(*fragment);
  BoxStrut margins = box_fragment.Margins().ConvertToLogical(writing_direction);
  LogicalFragment logical_fragment(writing_direction, *fragment);

  LayoutUnit margin_start =
      is_block_direction ? margins.block_start : margins.inline_start;
  LayoutUnit margin_end =
      is_block_direction ? margins.block_end : margins.inline_end;
  LayoutUnit fragment_size = is_block_direction ? logical_fragment.BlockSize()
                                                : logical_fragment.InlineSize();
  LayoutUnit& stored_offset =
      is_block_direction ? offset.block_offset : offset.inline_offset;

  // Flip `stored_offset` so that its origin starts from
  // `container_size_in_reversal_axis` rather than the original origin of the
  // container.
  stored_offset =
      container_size_in_reversal_axis - stored_offset - fragment_size;

  // `stored_offset` was originally measured from the container's border-box
  // start, so `border_scrollbar_padding_start` was already baked into it.
  // Add `border_scrollbar_padding_start` back twice: the first time undoes
  // its loss in the subtraction above. The second time is to account for the
  // fact that `container_size_in_reversal_axis` is the container size not
  // including border, scrollbar, and padding, so we must add an extra offset
  // so that, in the final layout, items end up positioned past the border,
  // scrollbar, and padding at the start of the container.
  stored_offset += 2 * border_scrollbar_padding_start;

  // Margins keep their physical sides under reversal (e.g., margin-bottom
  // stays on the bottom of the box). Add `margin_start` to leave room for the
  // leading-side margin, and subtract `margin_end` to leave room for the
  // trailing-side margin.
  stored_offset += margin_start - margin_end;
}

}  // namespace blink
