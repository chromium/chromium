// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/logical_fragment_link.h"

#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {

void LogicalFragmentLink::ReverseChildOffset(
    const WritingDirectionMode& writing_direction,
    bool is_block_direction,
    LayoutUnit container_size,
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

  stored_offset = CalculateReverseChildOffset(
      stored_offset, fragment_size, container_size,
      border_scrollbar_padding_start, margin_start, margin_end);
}

}  // namespace blink
