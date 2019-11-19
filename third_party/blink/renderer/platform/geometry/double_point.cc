// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/double_point.h"

#include <algorithm>
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

DoublePoint::operator FloatPoint() const {
  return FloatPoint(clampTo<float>(x_), clampTo<float>(y_));
}

DoublePoint DoublePoint::ExpandedTo(const DoublePoint& other) const {
  return DoublePoint(std::max(x_, other.x_), std::max(y_, other.y_));
}

DoublePoint DoublePoint::ShrunkTo(const DoublePoint& other) const {
  return DoublePoint(std::min(x_, other.x_), std::min(y_, other.y_));
}

std::ostream& operator<<(std::ostream& ostream, const DoublePoint& point) {
  return ostream << point.ToString();
}

String DoublePoint::ToString() const {
  return String::Format("%lg,%lg", X(), Y());
}

}  // namespace blink
