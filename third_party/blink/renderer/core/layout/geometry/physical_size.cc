// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

PhysicalSize PhysicalSize::FitToAspectRatio(const PhysicalSize& aspect_ratio,
                                            AspectRatioFit fit) const {
  // Convert to float to avoid overflow of LayoutUnit in multiplication below
  // and improve precision when calculating scale.
  float height_float = height.ToFloat();
  float width_float = width.ToFloat();
  float height_scale = height_float / aspect_ratio.height.ToFloat();
  float width_scale = width_float / aspect_ratio.width.ToFloat();
  if ((width_scale > height_scale) != (fit == kAspectRatioFitGrow)) {
    return {LayoutUnit::FromFloatRound(height_float * aspect_ratio.width /
                                       aspect_ratio.height),
            height};
  }
  return {width, LayoutUnit::FromFloatRound(width_float * aspect_ratio.height /
                                            aspect_ratio.width)};
}

String PhysicalSize::ToString() const {
  return String::Format("%sx%s", width.ToString().Ascii().c_str(),
                        height.ToString().Ascii().c_str());
}

std::ostream& operator<<(std::ostream& os, const PhysicalSize& value) {
  return os << value.ToString();
}

}  // namespace blink
