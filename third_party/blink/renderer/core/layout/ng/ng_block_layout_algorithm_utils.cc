// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm_utils.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

namespace blink {

LayoutUnit CalculateOutOfFlowStaticInlineLevelOffset(
    const ComputedStyle& container_style,
    const NGBfcOffset& origin_bfc_offset,
    const NGExclusionSpace& exclusion_space,
    LayoutUnit child_available_inline_size) {
  const TextDirection direction = container_style.Direction();

  // Find a layout opportunity, where we would have placed a zero-sized line.
  NGLayoutOpportunity opportunity = exclusion_space.FindLayoutOpportunity(
      origin_bfc_offset, child_available_inline_size);

  LayoutUnit child_line_offset = IsLtr(direction)
                                     ? opportunity.rect.LineStartOffset()
                                     : opportunity.rect.LineEndOffset();

  LayoutUnit relative_line_offset =
      child_line_offset - origin_bfc_offset.line_offset;

  // Convert back to the logical coordinate system. As the conversion is on an
  // OOF-positioned node, we pretent it has zero inline-size.
  LayoutUnit inline_offset =
      IsLtr(direction) ? relative_line_offset
                       : child_available_inline_size - relative_line_offset;

  // Adjust for text alignment, within the layout opportunity.
  LayoutUnit line_offset = LineOffsetForTextAlign(
      container_style.GetTextAlign(), direction, opportunity.rect.InlineSize());

  if (IsLtr(direction))
    inline_offset += line_offset;
  else
    inline_offset += opportunity.rect.InlineSize() - line_offset;

  // Adjust for the text-indent.
  inline_offset += MinimumValueForLength(container_style.TextIndent(),
                                         child_available_inline_size);

  return inline_offset;
}

}  // namespace blink
