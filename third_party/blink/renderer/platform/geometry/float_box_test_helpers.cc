/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/geometry/float_box_test_helpers.h"

#include "third_party/blink/renderer/platform/geometry/float_box.h"
#include "third_party/blink/renderer/platform/geometry/geometry_test_helpers.h"

const static float kTestEpsilon = 1e-6;

namespace blink {
namespace float_box_test {

bool ApproximatelyEqual(const float& a, const float& b) {
  return geometry_test::ApproximatelyEqual(a, b, kTestEpsilon);
}

bool ApproximatelyEqual(const FloatBox& a, const FloatBox& b) {
  if (!ApproximatelyEqual(a.X(), b.X()) || !ApproximatelyEqual(a.Y(), b.Y()) ||
      !ApproximatelyEqual(a.Z(), b.Z()) ||
      !ApproximatelyEqual(a.Width(), b.Width()) ||
      !ApproximatelyEqual(a.Height(), b.Height()) ||
      !ApproximatelyEqual(a.Depth(), b.Depth())) {
    return false;
  }
  return true;
}

testing::AssertionResult AssertAlmostEqual(const char* expr,
                                           const char* n_expr,
                                           const FloatBox& m,
                                           const FloatBox& n) {
  if (!ApproximatelyEqual(m, n)) {
    return testing::AssertionFailure()
           << "       Value of:" << n_expr << std::endl
           << "         Actual:" << testing::PrintToString(n) << std::endl
           << "Expected Approx:" << expr << std::endl
           << "       Which is:" << testing::PrintToString(m);
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult AssertContains(const char* expr,
                                        const char* n_expr,
                                        const FloatBox& m,
                                        const FloatBox& n) {
  FloatBox new_m = m;
  new_m.ExpandTo(n);
  if (!ApproximatelyEqual(m, new_m)) {
    return testing::AssertionFailure()
           << "        Value of:" << n_expr << std::endl
           << "          Actual:" << testing::PrintToString(n) << std::endl
           << "Not Contained in:" << expr << std::endl
           << "        Which is:" << testing::PrintToString(m);
  }
  return testing::AssertionSuccess();
}

}  // namespace float_box_test
}  // namespace blink
