// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

PhysicalOffset LogicalOffset::ConvertToPhysical(WritingMode mode,
                                                TextDirection direction,
                                                PhysicalSize outer_size,
                                                PhysicalSize inner_size) const {
  switch (mode) {
    case WritingMode::kHorizontalTb:
      if (direction == TextDirection::kLtr)
        return PhysicalOffset(inline_offset, block_offset);
      return PhysicalOffset(outer_size.width - inline_offset - inner_size.width,
                            block_offset);
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      if (direction == TextDirection::kLtr) {
        return PhysicalOffset(
            outer_size.width - block_offset - inner_size.width, inline_offset);
      }
      return PhysicalOffset(
          outer_size.width - block_offset - inner_size.width,
          outer_size.height - inline_offset - inner_size.height);
    case WritingMode::kVerticalLr:
      if (direction == TextDirection::kLtr)
        return PhysicalOffset(block_offset, inline_offset);
      return PhysicalOffset(
          block_offset, outer_size.height - inline_offset - inner_size.height);
    case WritingMode::kSidewaysLr:
      if (direction == TextDirection::kLtr) {
        return PhysicalOffset(block_offset, outer_size.height - inline_offset -
                                                inner_size.height);
      }
      return PhysicalOffset(block_offset, inline_offset);
    default:
      NOTREACHED();
      return PhysicalOffset();
  }
}

String LogicalOffset::ToString() const {
  return String::Format("%d,%d", inline_offset.ToInt(), block_offset.ToInt());
}

std::ostream& operator<<(std::ostream& os, const LogicalOffset& value) {
  return os << value.ToString();
}

}  // namespace blink
