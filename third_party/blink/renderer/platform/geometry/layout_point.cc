// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/layout_point.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

std::ostream& operator<<(std::ostream& ostream, const LayoutPoint& point) {
  return ostream << point.ToString();
}

String LayoutPoint::ToString() const {
  return String::Format("%s,%s", X().ToString().Ascii().c_str(),
                        Y().ToString().Ascii().c_str());
}

}  // namespace blink
