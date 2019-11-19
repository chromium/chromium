// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

LogicalOffset PhysicalOffset::ConvertToLogical(WritingMode mode,
                                               TextDirection direction,
                                               PhysicalSize outer_size,
                                               PhysicalSize inner_size) const {
  switch (mode) {
    case WritingMode::kHorizontalTb:
      if (direction == TextDirection::kLtr)
        return LogicalOffset(left, top);
      return LogicalOffset(outer_size.width - left - inner_size.width, top);
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      if (direction == TextDirection::kLtr)
        return LogicalOffset(top, outer_size.width - left - inner_size.width);
      return LogicalOffset(outer_size.height - top - inner_size.height,
                           outer_size.width - left - inner_size.width);
    case WritingMode::kVerticalLr:
      if (direction == TextDirection::kLtr)
        return LogicalOffset(top, left);
      return LogicalOffset(outer_size.height - top - inner_size.height, left);
    case WritingMode::kSidewaysLr:
      if (direction == TextDirection::kLtr)
        return LogicalOffset(outer_size.height - top - inner_size.height, left);
      return LogicalOffset(top, left);
    default:
      NOTREACHED();
      return LogicalOffset();
  }
}

String PhysicalOffset::ToString() const {
  return String::Format("%s,%s", left.ToString().Ascii().c_str(),
                        top.ToString().Ascii().c_str());
}

std::ostream& operator<<(std::ostream& os, const PhysicalOffset& value) {
  return os << value.ToString();
}

}  // namespace blink
