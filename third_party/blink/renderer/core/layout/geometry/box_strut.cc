// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"

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

PhysicalBoxStrut& PhysicalBoxStrut::Unite(const PhysicalBoxStrut& other) {
  top = std::max(top, other.top);
  right = std::max(right, other.right);
  bottom = std::max(bottom, other.bottom);
  left = std::max(left, other.left);
  return *this;
}

}  // namespace blink
