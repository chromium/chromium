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

}  // namespace blink::audio_utilities
