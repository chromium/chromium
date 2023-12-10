// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/biquad_dsp_kernel.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(HasConstantValuesTest, RegressionTest) {
  test::TaskEnvironment task_environment;
  const int frames_to_process = 7;
  float values[frames_to_process] = {0};

  // Test with all same elements
  EXPECT_TRUE(blink::BiquadDSPKernel::HasConstantValuesForTesting(
      values, frames_to_process));

  // Test with a single different element at each position
  for (float& value : values) {
    value = 1.0;
    EXPECT_FALSE(blink::BiquadDSPKernel::HasConstantValuesForTesting(
        values, frames_to_process));
    value = 0;
  }
}

}  // namespace blink
