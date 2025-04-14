// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/physical_size.h"

#include "base/numerics/safe_conversions.h"
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

// static
PhysicalSize LayoutRatioFromSizeF(gfx::SizeF ratio) {
  // Check if we can convert without any error.
  LayoutUnit width(ratio.width()), height(ratio.height());
  if ((width.ToFloat() == ratio.width() &&
       height.ToFloat() == ratio.height()) ||
      ratio.IsEmpty()) {
    return {width, height};
  }
  if (ratio.width() == ratio.height()) {
    return {LayoutUnit(1), LayoutUnit(1)};
  }

  // If we can't get a precise ratio we use the continued fraction algorithm to
  // get an approximation. See: https://en.wikipedia.org/wiki/Continued_fraction
  float initial = ratio.AspectRatio();
  float x = initial;

  // Use ints for the direct conversion using |LayoutUnit::FromRawValue| below.
  using ClampedInt = base::ClampedNumeric<int>;
  ClampedInt h0 = 0, h1 = 1, k0 = 1, k1 = 0;

  // The worst case for this algorithm is the golden ratio, which requires 16
  // iterations to reach our desired error.
  for (wtf_size_t i = 0; i < 16; ++i) {
    // Break if we've gone Inf, or NaN.
    if (!std::isfinite(x)) {
      break;
    }
    // Break if we've hit a good approximation.
    float estimate = static_cast<float>(h1) / k1;
    if (fabs(initial - estimate) < 0.000001f) {
      break;
    }

    const int a = base::ClampFloor<int>(x);
    ClampedInt h2 = (h1 * a) + h0;
    ClampedInt k2 = (k1 * a) + k0;

    // Break if we've saturated (the ratio becomes meaningless).
    if (h2 == std::numeric_limits<int>::max() ||
        k2 == std::numeric_limits<int>::max()) {
      break;
    }

    // Update our convergents.
    h0 = h1, k0 = k1, h1 = h2, k1 = k2;
    x = 1 / (x - a);
  }

  // Don't return an invalid ratio - instead just return the truncated ratio.
  if (h1 == 0 || k1 == 0) {
    return {width, height};
  }

  return {LayoutUnit::FromRawValue(h1.RawValue()),
          LayoutUnit::FromRawValue(k1.RawValue())};
}

}  // namespace blink
