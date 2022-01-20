// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/double_point.h"

#include <algorithm>
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

DoublePoint::operator gfx::PointF() const {
  return gfx::PointF(ClampTo<float>(x_), ClampTo<float>(y_));
}

std::ostream& operator<<(std::ostream& ostream, const DoublePoint& point) {
  return ostream << point.ToString();
}

String DoublePoint::ToString() const {
  return String::Format("%lg,%lg", X(), Y());
}

}  // namespace blink
