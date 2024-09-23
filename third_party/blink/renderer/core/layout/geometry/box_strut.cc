// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String BoxStrut::ToString() const {
  return String::Format("Inline: (%d %d) Block: (%d %d)", inline_start.ToInt(),
                        inline_end.ToInt(), block_start.ToInt(),
                        block_end.ToInt());
}

std::ostream& operator<<(std::ostream& stream, const BoxStrut& value) {
  return stream << value.ToString();
}

BoxStrut::BoxStrut(const LineBoxStrut& line_relative, bool is_flipped_lines) {
  if (!is_flipped_lines) {
    *this = {line_relative.inline_start, line_relative.inline_end,
             line_relative.line_over, line_relative.line_under};
  } else {
    *this = {line_relative.inline_start, line_relative.inline_end,
             line_relative.line_under, line_relative.line_over};
  }
}

BoxStrut::BoxStrut(const LogicalSize& outer_size, const LogicalRect& inner_rect)
    : inline_start(inner_rect.offset.inline_offset),
      inline_end(outer_size.inline_size - inner_rect.InlineEndOffset()),
      block_start(inner_rect.offset.block_offset),
      block_end(outer_size.block_size - inner_rect.BlockEndOffset()) {}

BoxStrut& BoxStrut::Intersect(const BoxStrut& other) {
  inline_start = std::min(inline_start, other.inline_start);
  inline_end = std::min(inline_end, other.inline_end);
  block_start = std::min(block_start, other.block_start);
  block_end = std::min(block_end, other.block_end);
  return *this;
}

LineBoxStrut::LineBoxStrut(const BoxStrut& flow_relative,
                           bool is_flipped_lines) {
  if (!is_flipped_lines) {
    *this = {flow_relative.inline_start, flow_relative.inline_end,
             flow_relative.block_start, flow_relative.block_end};
  } else {
    *this = {flow_relative.inline_start, flow_relative.inline_end,
             flow_relative.block_end, flow_relative.block_start};
  }
}

std::ostream& operator<<(std::ostream& stream, const LineBoxStrut& value) {
  return stream << "Inline: (" << value.inline_start << " " << value.inline_end
                << ") Line: (" << value.line_over << " " << value.line_under
                << ") ";
}

PhysicalBoxStrut::PhysicalBoxStrut(const PhysicalSize& outer_size,
                                   const PhysicalRect& inner_rect)
    : top(inner_rect.offset.top),
      right(outer_size.width - inner_rect.Right()),
      bottom(outer_size.height - inner_rect.Bottom()),
      left(inner_rect.offset.left) {}

PhysicalBoxStrut& PhysicalBoxStrut::Unite(const PhysicalBoxStrut& other) {
  top = std::max(top, other.top);
  right = std::max(right, other.right);
  bottom = std::max(bottom, other.bottom);
  left = std::max(left, other.left);
  return *this;
}

}  // namespace blink
