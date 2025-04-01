// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/physical_offset.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

template <typename ValueType>
String PhysicalFixedOffset<ValueType>::ToString() const {
  return String::Format("%s,%s", left.ToString().Ascii().c_str(),
                        top.ToString().Ascii().c_str());
}

template <typename ValueType>
std::ostream& operator<<(std::ostream& os,
                         const PhysicalFixedOffset<ValueType>& value) {
  return os << value.ToString();
}

// Explicit instantiations.
#define INSTANTIATE(ValueType)                       \
  template struct PhysicalFixedOffset<ValueType>;    \
  template PLATFORM_EXPORT std::ostream& operator<<( \
      std::ostream&, const PhysicalFixedOffset<ValueType>&)

INSTANTIATE(LayoutUnit);

}  // namespace blink
