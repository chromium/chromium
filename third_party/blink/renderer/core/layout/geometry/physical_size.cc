// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

PhysicalSize PhysicalSize::FitToAspectRatio(const PhysicalSize& aspect_ratio,
                                            AspectRatioFit fit) const {
  DCHECK_GT(aspect_ratio.width, 0);
  DCHECK_GT(aspect_ratio.height, 0);
  const LayoutUnit constrained_height =
      width.MulDiv(aspect_ratio.height, aspect_ratio.width);
  const bool grow = fit == kAspectRatioFitGrow;
  if ((grow && constrained_height < height) ||
      (!grow && constrained_height > height)) {
    const LayoutUnit constrained_width =
        height.MulDiv(aspect_ratio.width, aspect_ratio.height);
    return {constrained_width, height};
  }
  return {width, constrained_height};
}

String PhysicalSize::ToString() const {
  return String::Format("%sx%s", width.ToString().Ascii().c_str(),
                        height.ToString().Ascii().c_str());
}

std::ostream& operator<<(std::ostream& os, const PhysicalSize& value) {
  return os << value.ToString();
}

}  // namespace blink
