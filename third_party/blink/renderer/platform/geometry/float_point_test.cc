// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/float_point.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/geometry_test_helpers.h"

namespace blink {

TEST(FloatPointTest, LengthTest) {
  // Sanity check the Pythagorean triples 3-4-5 and 5-12-13
  FloatPoint p1 = FloatPoint(3.f, 4.f);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual, p1.length(), 5.f);
  FloatPoint p2 = FloatPoint(5.f, 12.f);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual, p2.length(), 13.f);

  // Test very small numbers. This fails under the old implementation of
  // FloatPoint::length(): `return sqrtf(lengthSquared());'
  FloatPoint p3 = FloatPoint(.5e-20f, .5e-20f);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual, p3.length(),
                      .707106781186548e-20f);

  // Test very large numbers.
  FloatPoint p4 = FloatPoint(.5e20f, .5e20f);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual, p4.length(),
                      .707106781186548e20f);
}

}  // namespace blink
