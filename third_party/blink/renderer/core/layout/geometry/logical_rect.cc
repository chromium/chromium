// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
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

  UniteEvenIfEmpty(other);
}

void LogicalRect::UniteEvenIfEmpty(const LogicalRect& other) {
  LogicalOffset new_end_offset(Max(EndOffset(), other.EndOffset()));
  LogicalOffset new_start_offset(Min(offset, other.offset));
  size = new_end_offset - new_start_offset;
  offset = {new_end_offset.inline_offset - size.inline_size,
            new_end_offset.block_offset - size.block_size};
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
