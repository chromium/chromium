// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

LogicalOffset PhysicalOffset::ConvertToLogical(
    WritingDirectionMode writing_direction,
    PhysicalSize outer_size,
    PhysicalSize inner_size) const {
  return WritingModeConverter(writing_direction, outer_size)
      .ToLogical(*this, inner_size);
}

String PhysicalOffset::ToString() const {
  return String::Format("%s,%s", left.ToString().Ascii().c_str(),
                        top.ToString().Ascii().c_str());
}

std::ostream& operator<<(std::ostream& os, const PhysicalOffset& value) {
  return os << value.ToString();
}

}  // namespace blink
