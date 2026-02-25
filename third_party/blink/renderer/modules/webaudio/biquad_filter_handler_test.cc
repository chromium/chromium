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

TEST(HasConstantValuesTest, PartialBuffer) {
  test::TaskEnvironment task_environment;
  float values[128] = {};

  // Fill the first 10 frames with 1.0
  for (float& v : base::span(values).first(10u)) {
    v = 1.0f;
  }
  // Fill the rest with 2.0 (stale data)
  for (float& v : base::span(values).subspan(10u)) {
    v = 2.0f;
  }

  // If we only care about the first 10 frames, it should be constant.
  base::span<float> partial_span = base::span(values).first(10u);
  EXPECT_TRUE(
      blink::BiquadFilterHandler::HasConstantValuesForTesting(partial_span));

  // If we pass the whole buffer, it's NOT constant.
  EXPECT_FALSE(blink::BiquadFilterHandler::HasConstantValuesForTesting(values));
}

}  // namespace blink
