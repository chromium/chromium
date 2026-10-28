// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/encoder_bitrate_filter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(EncoderBitrateFilterTest, TargetBitrate_SameValues) {
  static constexpr int kBandwidthKbps = 1000000;
  EncoderBitrateFilter filter(1000);
  for (int i = 0; i < 1000; i++) {
    filter.SetBandwidthEstimateKbps(kBandwidthKbps);
    EXPECT_EQ(kBandwidthKbps, filter.GetTargetBitrateKbps());
  }
}

TEST(EncoderBitrateFilterTest, WontChangeBitrateForASharpIncrement) {
  static constexpr int kBandwidthKbps = 1000000;
  EncoderBitrateFilter filter(1000);
  for (int i = 0; i < 1000; i++) {
    if (i == 500) {
      filter.SetBandwidthEstimateKbps(kBandwidthKbps * 5);
    } else {
      filter.SetBandwidthEstimateKbps(kBandwidthKbps);
    }
    EXPECT_EQ(kBandwidthKbps, filter.GetTargetBitrateKbps());
  }
}

TEST(EncoderBitrateFilterTest, WontChangeBitrateForASharpDecrement) {
  static constexpr int kBandwidthKbps = 1000000;
  EncoderBitrateFilter filter(1000);
  for (int i = 0; i < 1000; i++) {
    if (i == 500) {
      filter.SetBandwidthEstimateKbps(kBandwidthKbps / 5);
    } else {
      filter.SetBandwidthEstimateKbps(kBandwidthKbps);
    }
    EXPECT_EQ(kBandwidthKbps, filter.GetTargetBitrateKbps());
  }
}

TEST(EncoderBitrateFilterTest,
     WontChangeBitrateIfBandwidthChangeIsBelowThreshold) {
  static constexpr int kBandwidthKbps = 1000000;
  EncoderBitrateFilter filter(1000);
  for (int i = 0; i < 1000; i++) {
    if (i < 500) {
      filter.SetBandwidthEstimateKbps(kBandwidthKbps);
    } else {
      filter.SetBandwidthEstimateKbps(kBandwidthKbps * 1.2);
    }
    EXPECT_EQ(kBandwidthKbps, filter.GetTargetBitrateKbps());
  }
}

TEST(EncoderBitrateFilterTest, ChangeBitrateIfBandwidthChangeIsMet) {
  static constexpr int kBandwidthKbps = 1000000;
  EncoderBitrateFilter filter(1000);
  for (int i = 0; i < 1000; i++) {
    if (i < 500) {
      filter.SetBandwidthEstimateKbps(kBandwidthKbps);
      EXPECT_EQ(kBandwidthKbps, filter.GetTargetBitrateKbps());
    } else {
      filter.SetBandwidthEstimateKbps(kBandwidthKbps * 2);
      // If the bandwidth is doubled, the filter should be able to take effect
      // within 30 frames (1 second).
      if (i > 530) {
        EXPECT_GT(filter.GetTargetBitrateKbps(), kBandwidthKbps);
      }
    }
  }
  // After 500 frames (~16 seconds), the weight of old bandwidth estimates
  // should be small enough to have very little impact to the result.
  EXPECT_GT(filter.GetTargetBitrateKbps(), kBandwidthKbps * 1.7);
}

TEST(EncoderBitrateFilterTest, InitialBitrateIsGreaterThanZero) {
  EncoderBitrateFilter filter(1000);
  EXPECT_GT(filter.GetTargetBitrateKbps(), 0);
}

TEST(EncoderBitrateFilterTest, EnforcesMinimumBitrateForFrameSize) {
  // 2500 kbps per megapixel at 1280x720 (0.9216 MP) = 2304 kbps minimum.
  EncoderBitrateFilter filter(2500);
  filter.SetFrameSize(1280, 720);
  EXPECT_EQ(2304, filter.GetTargetBitrateKbps());

  for (int i = 0; i < 100; i++) {
    filter.SetBandwidthEstimateKbps(500);
    EXPECT_EQ(2304, filter.GetTargetBitrateKbps());
  }

  // Increasing the frame size to 1920x1080 (2.0736 MP) raises the minimum
  // bitrate floor to 5184 kbps.
  filter.SetFrameSize(1920, 1080);
  EXPECT_EQ(5184, filter.GetTargetBitrateKbps());
  for (int i = 0; i < 100; i++) {
    filter.SetBandwidthEstimateKbps(500);
    EXPECT_EQ(5184, filter.GetTargetBitrateKbps());
  }

  // Shrinking the frame size back to 1280x720 lowers the minimum bitrate floor
  // to 2304 kbps; as low bandwidth estimates are recorded, the target bitrate
  // steps down once the weighted average changes by more than 33%, while never
  // dropping below the 2304 kbps floor.
  filter.SetFrameSize(1280, 720);
  for (int i = 0; i < 100; i++) {
    filter.SetBandwidthEstimateKbps(500);
  }
  EXPECT_LT(filter.GetTargetBitrateKbps(), 5184);
  EXPECT_GE(filter.GetTargetBitrateKbps(), 2304);
}

}  // namespace remoting
