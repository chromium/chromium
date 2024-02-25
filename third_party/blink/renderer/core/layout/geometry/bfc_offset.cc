// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/bfc_offset.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String BfcOffset::ToString() const {
  return String::Format("%dx%d", line_offset.ToInt(), block_offset.ToInt());
}

std::ostream& operator<<(std::ostream& os, const BfcOffset& value) {
  return os << value.ToString();
}

}  // namespace blink
