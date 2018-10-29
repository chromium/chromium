// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/float_size.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/geometry_test_helpers.h"

namespace blink {

TEST(FloatSizeTest, DiagonalLengthTest) {
  // Sanity check the Pythagorean triples 3-4-5 and 5-12-13
  FloatSize s1 = FloatSize(3.f, 4.f);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual, s1.DiagonalLength(),
                      5.f);
  FloatSize s2 = FloatSize(5.f, 12.f);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual, s2.DiagonalLength(),
                      13.f);

  // Test very small numbers.
  FloatSize s3 = FloatSize(.5e-20f, .5e-20f);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual, s3.DiagonalLength(),
                      .707106781186548e-20f);

  // Test very large numbers.
  FloatSize s4 = FloatSize(.5e20f, .5e20f);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual, s4.DiagonalLength(),
                      .707106781186548e20f);
}

}  // namespace blink
