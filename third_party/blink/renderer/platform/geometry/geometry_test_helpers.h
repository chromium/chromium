// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_GEOMETRY_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_GEOMETRY_TEST_HELPERS_H_

#include <gtest/gtest.h>

namespace blink {
namespace geometry_test {

bool ApproximatelyEqual(float, float, float test_epsilon);
testing::AssertionResult AssertAlmostEqual(const char* actual_expr,
                                           const char* expected_expr,
                                           float actual,
                                           float expected,
                                           float test_epsilon = 1e-6);

}  // namespace geometry_test
}  // namespace blink

#endif
