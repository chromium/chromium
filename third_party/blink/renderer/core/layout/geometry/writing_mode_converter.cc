// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"

namespace blink {

LogicalOffset WritingModeConverter::SlowToLogical(
    const PhysicalOffset& offset,
    const PhysicalSize& inner_size) const {
  switch (GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      DCHECK(!IsLtr());  // LTR is in the fast code path.
      return LogicalOffset(outer_size_.width - offset.left - inner_size.width,
                           offset.top);
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      if (IsLtr()) {
        return LogicalOffset(
            offset.top, outer_size_.width - offset.left - inner_size.width);
      }
      return LogicalOffset(outer_size_.height - offset.top - inner_size.height,
                           outer_size_.width - offset.left - inner_size.width);
    case WritingMode::kVerticalLr:
      if (IsLtr())
        return LogicalOffset(offset.top, offset.left);
      return LogicalOffset(outer_size_.height - offset.top - inner_size.height,
                           offset.left);
    case WritingMode::kSidewaysLr:
      if (IsLtr()) {
        return LogicalOffset(
            outer_size_.height - offset.top - inner_size.height, offset.left);
      }
      return LogicalOffset(offset.top, offset.left);
  }
  NOTREACHED();
  return LogicalOffset();
}

PhysicalOffset WritingModeConverter::SlowToPhysical(
    const LogicalOffset& offset,
    const PhysicalSize& inner_size) const {
  switch (GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      DCHECK(!IsLtr());  // LTR is in the fast code path.
      return PhysicalOffset(
          outer_size_.width - offset.inline_offset - inner_size.width,
          offset.block_offset);
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      if (IsLtr()) {
        return PhysicalOffset(
            outer_size_.width - offset.block_offset - inner_size.width,
            offset.inline_offset);
      }
      return PhysicalOffset(
          outer_size_.width - offset.block_offset - inner_size.width,
          outer_size_.height - offset.inline_offset - inner_size.height);
    case WritingMode::kVerticalLr:
      if (IsLtr())
        return PhysicalOffset(offset.block_offset, offset.inline_offset);
      return PhysicalOffset(
          offset.block_offset,
          outer_size_.height - offset.inline_offset - inner_size.height);
    case WritingMode::kSidewaysLr:
      if (IsLtr()) {
        return PhysicalOffset(
            offset.block_offset,
            outer_size_.height - offset.inline_offset - inner_size.height);
      }
      return PhysicalOffset(offset.block_offset, offset.inline_offset);
  }
  NOTREACHED();
  return PhysicalOffset();
}

LogicalRect WritingModeConverter::SlowToLogical(
    const PhysicalRect& rect) const {
  return LogicalRect(SlowToLogical(rect.offset, rect.size),
                     ToLogical(rect.size));
}

PhysicalRect WritingModeConverter::SlowToPhysical(
    const LogicalRect& rect) const {
  const PhysicalSize size = ToPhysical(rect.size);
  return PhysicalRect(SlowToPhysical(rect.offset, size), size);
}

}  // namespace blink
