// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_value_clamping_utils.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

/*
 The value must be multiple of 360deg.
 Reference:  https://drafts.csswg.org/css-values/#numeric-types

 This constant is the biggest multiple of 360 that a double can accurately
 represent, and after converting to rads, the sin() value is close enough to 0.

 The details: https://bit.ly/349gXjq
*/

constexpr static double kApproxDoubleInfinityAngle = 2867080569122160;

double CSSValueClampingUtils::ClampDouble(double value) {
  // https://www.w3.org/TR/css-values-4/#top-level-calculation
  if (std::isnan(value)) {
    value = 0;
  }
  return ClampTo<double>(value);
}

double CSSValueClampingUtils::ClampLength(double value) {
  return ClampDouble(value);
}

double CSSValueClampingUtils::ClampTime(double value) {
  return ClampDouble(value);
}

double CSSValueClampingUtils::ClampAngle(double value) {
  if (std::isnan(value)) {
    value = kApproxDoubleInfinityAngle;
  }
  return ClampTo<double>(value, -kApproxDoubleInfinityAngle,
                         kApproxDoubleInfinityAngle);
}

float CSSValueClampingUtils::ClampLength(float value) {
  if (std::isnan(value)) {
    value = std::numeric_limits<float>::max();
  }
  return ClampTo<float>(value);
}

}  // namespace blink
