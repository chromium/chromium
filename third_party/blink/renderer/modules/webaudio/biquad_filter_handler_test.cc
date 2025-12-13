// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/biquad_filter_handler.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(HasConstantValuesTest, RegressionTest) {
  test::TaskEnvironment task_environment;
  float values[7] = {};

  // Test with all same elements
  EXPECT_TRUE(blink::BiquadFilterHandler::HasConstantValuesForTesting(values));

  // Test with a single different element at each position
  for (float& value : values) {
    value = 1.0;
    EXPECT_FALSE(
        blink::BiquadFilterHandler::HasConstantValuesForTesting(values));
    value = 0;
  }
}

}  // namespace blink
