// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_value_clamping_utils.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

double CSSValueClampingUtils::ClampLength(double value) {
  if (std::isnan(value))
    value = std::numeric_limits<double>::max();
  return clampTo<double>(value);
}

float CSSValueClampingUtils::ClampLength(float value) {
  if (std::isnan(value))
    value = std::numeric_limits<float>::max();
  return clampTo<float>(value);
}

}  // namespace blink
