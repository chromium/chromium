// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

inline LogicalOffset Min(LogicalOffset a, LogicalOffset b) {
  return {std::min(a.inline_offset, b.inline_offset),
          std::min(a.block_offset, b.block_offset)};
}

inline LogicalOffset Max(LogicalOffset a, LogicalOffset b) {
  return {std::max(a.inline_offset, b.inline_offset),
          std::max(a.block_offset, b.block_offset)};
}

}  // namespace

void LogicalRect::Unite(const LogicalRect& other) {
  if (other.IsEmpty())
    return;
  if (IsEmpty()) {
    *this = other;
    return;
  }

  LogicalOffset new_end_offset(Max(EndOffset(), other.EndOffset()));
  offset = Min(offset, other.offset);
  size = new_end_offset - offset;
}

PhysicalRect LogicalRect::ConvertToPhysical(
    WritingMode writing_mode,
    const PhysicalSize& outer_size) const {
  if (IsHorizontalWritingMode(writing_mode)) {
    return {offset.inline_offset, offset.block_offset, size.inline_size,
            size.block_size};
  }

  // Vertical, clock-wise rotation.
  if (writing_mode != WritingMode::kSidewaysLr) {
    return {outer_size.width - BlockEndOffset(), offset.inline_offset,
            size.block_size, size.inline_size};
  }

  // Vertical, counter-clock-wise rotation.
  return {offset.block_offset, outer_size.height - InlineEndOffset(),
          size.block_size, size.inline_size};
}

String LogicalRect::ToString() const {
  return String::Format("%s,%s %sx%s",
                        offset.inline_offset.ToString().Ascii().c_str(),
                        offset.block_offset.ToString().Ascii().c_str(),
                        size.inline_size.ToString().Ascii().c_str(),
                        size.block_size.ToString().Ascii().c_str());
}

std::ostream& operator<<(std::ostream& os, const LogicalRect& value) {
  return os << value.ToString();
}

}  // namespace blink
