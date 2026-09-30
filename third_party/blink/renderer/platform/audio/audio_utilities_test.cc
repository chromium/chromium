// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/audio_utilities.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink::audio_utilities {

TEST(AudioUtilitiesTest, RoundUpToMultiple) {
  EXPECT_EQ(0u, RoundUpToMultiple(0, 128));
  EXPECT_EQ(128u, RoundUpToMultiple(100, 128));
  EXPECT_EQ(128u, RoundUpToMultiple(128, 128));
  EXPECT_EQ(256u, RoundUpToMultiple(129, 128));
  EXPECT_EQ(132000u, RoundUpToMultiple(131072, 3000));
  EXPECT_DEATH_IF_SUPPORTED(RoundUpToMultiple(100, 0), "");
  EXPECT_EQ(100u, RoundUpToMultiple(100, 1));
}

TEST(AudioUtilitiesTest, HasConstantValues) {
  // Empty and single-element spans are always constant.
  EXPECT_TRUE(HasConstantValues({}));
  const float single_value[] = {42.0f};
  EXPECT_TRUE(HasConstantValues(single_value));

  // Non-multiple-of-4 size exercising both SIMD and scalar tail paths.
  float values[7] = {};
  EXPECT_TRUE(HasConstantValues(values));
  for (float& value : values) {
    value = 1.0f;
    EXPECT_FALSE(HasConstantValues(values));
    value = 0.0f;
  }

  // Partial / unaligned subspan check.
  float full_buffer[128] = {};
  for (float& v : base::span(full_buffer).first(10u)) {
    v = 3.0f;
  }
  for (float& v : base::span(full_buffer).subspan(10u)) {
    v = 5.0f;
  }
  EXPECT_TRUE(HasConstantValues(base::span(full_buffer).first(10u)));
  EXPECT_TRUE(HasConstantValues(base::span(full_buffer).subspan(1u, 9u)));
  EXPECT_FALSE(HasConstantValues(base::span(full_buffer).subspan(1u, 10u)));
  EXPECT_FALSE(HasConstantValues(full_buffer));
}

}  // namespace blink::audio_utilities
