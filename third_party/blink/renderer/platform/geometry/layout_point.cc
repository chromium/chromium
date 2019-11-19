// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/layout_point.h"

#include <algorithm>
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

LayoutPoint LayoutPoint::ExpandedTo(const LayoutPoint& other) const {
  return LayoutPoint(std::max(x_, other.x_), std::max(y_, other.y_));
}

LayoutPoint LayoutPoint::ShrunkTo(const LayoutPoint& other) const {
  return LayoutPoint(std::min(x_, other.x_), std::min(y_, other.y_));
}

std::ostream& operator<<(std::ostream& ostream, const LayoutPoint& point) {
  return ostream << point.ToString();
}

String LayoutPoint::ToString() const {
  return String::Format("%s,%s", X().ToString().Ascii().c_str(),
                        Y().ToString().Ascii().c_str());
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const LayoutPoint& point) {
  return ts << FloatPoint(point);
}

}  // namespace blink
