// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORMATION_MATRIX_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORMATION_MATRIX_TEST_HELPERS_H_

#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace blink {

// Allow non-zero tolerance when comparing floating point results to
// accommodate precision errors.
constexpr double kFloatingPointErrorTolerance = 1e-6;

#define EXPECT_TRANSFORMATION_MATRIX(expected, actual) \
  do {                                                 \
    SCOPED_TRACE("");                                  \
    cc::ExpectTransformationMatrixNear(                \
        TransformationMatrix::ToTransform(expected),   \
        TransformationMatrix::ToTransform(actual),     \
        kFloatingPointErrorTolerance);                 \
  } while (false)

#define EXPECT_FLOAT(expected, actual) \
  EXPECT_NEAR(expected, actual, kFloatingPointErrorTolerance)

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORMATION_MATRIX_TEST_HELPERS_H_
