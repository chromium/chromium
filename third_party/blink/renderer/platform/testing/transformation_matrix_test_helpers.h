// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TRANSFORMATION_MATRIX_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TRANSFORMATION_MATRIX_TEST_HELPERS_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

// Allow non-zero tolerance when comparing floating point results to
// accommodate precision errors.
constexpr double kFloatingPointErrorTolerance = 1e-6;

#define EXPECT_TRANSFORMATION_MATRIX(expected, actual)                  \
  do {                                                                  \
    EXPECT_TRANSFORM_NEAR(expected.ToTransform(), actual.ToTransform(), \
                          kFloatingPointErrorTolerance);                \
  } while (false)

#define EXPECT_FLOAT(expected, actual) \
  EXPECT_NEAR(expected, actual, kFloatingPointErrorTolerance)

inline TransformationMatrix MakeScaleMatrix(double tx,
                                            double ty,
                                            double tz = 1) {
  TransformationMatrix t;
  t.Scale3d(tx, ty, tz);
  return t;
}

inline TransformationMatrix MakeScaleMatrix(double s) {
  return MakeScaleMatrix(s, s, 1);
}

inline TransformationMatrix MakeTranslationMatrix(double tx,
                                                  double ty,
                                                  double tz = 0) {
  TransformationMatrix t;
  t.Translate3d(tx, ty, tz);
  return t;
}

inline TransformationMatrix MakeRotationMatrix(double degrees) {
  TransformationMatrix t;
  t.Rotate(degrees);
  return t;
}

inline TransformationMatrix MakeRotationMatrix(double degrees_x,
                                               double degrees_y,
                                               double degrees_z) {
  TransformationMatrix t;
  t.RotateAboutZAxis(degrees_z);
  t.RotateAboutYAxis(degrees_y);
  t.RotateAboutXAxis(degrees_x);
  return t;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TRANSFORMATION_MATRIX_TEST_HELPERS_H_
