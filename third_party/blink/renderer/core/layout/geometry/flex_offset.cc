// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/flex_offset.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

LogicalOffset FlexOffset::ToLogicalOffset(bool is_column_flex_container) const {
  if (is_column_flex_container)
    return LogicalOffset(cross_axis_offset, main_axis_offset);
  return LogicalOffset(main_axis_offset, cross_axis_offset);
}

String FlexOffset::ToString() const {
  return String::Format("%d,%d", main_axis_offset.ToInt(),
                        cross_axis_offset.ToInt());
}

std::ostream& operator<<(std::ostream& os, const FlexOffset& value) {
  return os << value.ToString();
}

}  // namespace blink
