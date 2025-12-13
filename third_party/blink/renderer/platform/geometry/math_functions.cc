// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/math_functions.h"

namespace blink {

// https://drafts.csswg.org/css-values-5/#random-evaluation
double ComputeCSSRandomValue(double random_base_value,
                             double min,
                             double max,
                             std::optional<double> step) {
  if (max < min) {
    max = min;
  }

  if (std::isinf(min)) {
    return min;
  }

  if (std::isinf(max - min) || std::isnan(min) || std::isnan(max)) {
    return std::numeric_limits<double>::quiet_NaN();
  }

  if (!step.has_value()) {
    return min + random_base_value * (max - min);
  }

  if (std::isinf(*step)) {
    return min;
  }

  if (std::isnan(*step)) {
    return std::numeric_limits<double>::quiet_NaN();
  }

  if (*step <= 0) {
    return min + random_base_value * (max - min);
  }

  int n = (max - min) / (*step);

  // step_index = round(down, random_base_value * (n + 1), 1);
  int step_index =
      ClampTo<int>(EvaluateRoundDownFunction(random_base_value * (n + 1), 1.0));
  return min + step_index * (*step);
}

}  // namespace blink
