// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/float_box.h"

#include <algorithm>
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

void FloatBox::ExpandTo(const FloatPoint3D& low, const FloatPoint3D& high) {
  DCHECK_LE(low.x(), high.x());
  DCHECK_LE(low.y(), high.y());
  DCHECK_LE(low.z(), high.z());

  float min_x = std::min(x_, low.x());
  float min_y = std::min(y_, low.y());
  float min_z = std::min(z_, low.z());

  float max_x = std::max(right(), high.x());
  float max_y = std::max(bottom(), high.y());
  float max_z = std::max(front(), high.z());

  x_ = min_x;
  y_ = min_y;
  z_ = min_z;

  width_ = max_x - min_x;
  height_ = max_y - min_y;
  depth_ = max_z - min_z;
}

std::ostream& operator<<(std::ostream& ostream, const FloatBox& box) {
  return ostream << box.ToString();
}

String FloatBox::ToString() const {
  return String::Format("%lg,%lg,%lg %lgx%lgx%lg", x(), y(), z(), width(),
                        height(), depth());
}

}  // namespace blink
