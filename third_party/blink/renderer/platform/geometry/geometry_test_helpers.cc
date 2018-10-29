// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/geometry_test_helpers.h"

#include <limits>
#include <math.h>

namespace blink {
namespace geometry_test {

bool ApproximatelyEqual(float a, float b, float test_epsilon) {
  float abs_a = ::fabs(a);
  float abs_b = ::fabs(b);
  float abs_err = ::fabs(a - b);
  if (a == b)
    return true;

  if (a == 0 || b == 0 || abs_err < std::numeric_limits<float>::min())
    return abs_err < (test_epsilon * std::numeric_limits<float>::min());

  return ((abs_err / (abs_a + abs_b)) < test_epsilon);
}

testing::AssertionResult AssertAlmostEqual(const char* actual_expr,
                                           const char* expected_expr,
                                           float actual,
                                           float expected,
                                           float test_epsilon) {
  if (!ApproximatelyEqual(actual, expected, test_epsilon)) {
    return testing::AssertionFailure()
           << "       Value of:" << actual_expr << std::endl
           << "         Actual:" << testing::PrintToString(actual) << std::endl
           << "Expected Approx:" << expected_expr << std::endl
           << "       Which is:" << testing::PrintToString(expected);
  }

  return testing::AssertionSuccess();
}

}  // namespace geometry_test
}  // namespace blink
