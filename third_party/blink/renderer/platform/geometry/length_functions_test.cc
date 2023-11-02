// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/length_functions.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(LengthFunctionsTest, OutOfRangePercentage) {
  Length max = Length::Percent(std::numeric_limits<float>::max());
  float value = FloatValueForLength(max, 800);
  EXPECT_TRUE(isfinite(value));
}

}  // namespace blink
