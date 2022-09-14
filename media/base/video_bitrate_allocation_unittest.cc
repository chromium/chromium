// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "media/base/video_bitrate_allocation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(VideoBitrateAllocationTest, Constructor_DefaultsModeConstant) {
  VideoBitrateAllocation allocation;

  ASSERT_EQ(allocation.GetMode(), Bitrate::Mode::kConstant);
}

TEST(VideoBitrateAllocationTest, Constructor_ConstantBitrate_CorrectMode) {
  VideoBitrateAllocation allocation(Bitrate::Mode::kConstant);

  ASSERT_EQ(allocation.GetMode(), Bitrate::Mode::kConstant);
}

TEST(VideoBitrateAllocationTest, Constructor_VariableBitrate_CorrectMode) {
  VideoBitrateAllocation allocation(Bitrate::Mode::kVariable);

  ASSERT_EQ(allocation.GetMode(), Bitrate::Mode::kVariable);
}

TEST(VideoBitrateAllocationTest,
     Constructor_ConstantBitrate_InitializesTargetZero) {
  VideoBitrateAllocation allocation;

  ASSERT_EQ(allocation.GetSumBitrate().target_bps(), 0u);
}

TEST(VideoBitrateAllocationTest,
     Constructor_VariableBitrate_InitializesTargetZero) {
  VideoBitrateAllocation allocation(Bitrate::Mode::kVariable);

  ASSERT_EQ(allocation.GetSumBitrate().target_bps(), 0u);
}

TEST(VideoBitrateAllocationTest, SetAndGet) {
  uint32_t sum = 0u;
  uint32_t layer_rate = 0u;
  VideoBitrateAllocation allocation;
  for (size_t spatial_index = 0;
       spatial_index < VideoBitrateAllocation::kMaxSpatialLayers;
       ++spatial_index) {
    for (size_t temporal_index = 0;
         temporal_index < VideoBitrateAllocation::kMaxTemporalLayers;
         ++temporal_index) {
      sum += layer_rate;
      EXPECT_TRUE(
          allocation.SetBitrate(spatial_index, temporal_index, layer_rate++));
    }
  }
  EXPECT_EQ(sum, allocation.GetSumBps());

  layer_rate = 0u;
  for (size_t spatial_index = 0;
       spatial_index < VideoBitrateAllocation::kMaxSpatialLayers;
       ++spatial_index) {
    for (size_t temporal_index = 0;
         temporal_index < VideoBitrateAllocation::kMaxTemporalLayers;
         ++temporal_index) {
      EXPECT_EQ(allocation.GetBitrateBps(spatial_index, temporal_index),
                layer_rate++);
    }
  }
}

TEST(VideoBitrateAllocationTest, SetBitrate_VariableBitrate_CorrectSum) {
  VideoBitrateAllocation allocation(Bitrate::Mode::kVariable);
  allocation.SetBitrate(0, 0, 1u);
  allocation.SetBitrate(1, 0, 2u);
  allocation.SetBitrate(0, 1, 3u);
  allocation.SetBitrate(2, 2, 4u);
  allocation.SetBitrate(1, 2, 5u);

  ASSERT_EQ(15u, allocation.GetSumBps());
}

TEST(VideoBitrateAllocationTest, SetBitrate_PeakTooLow_IncreasesPeak) {
  VideoBitrateAllocation allocation(Bitrate::Mode::kVariable);
  allocation.SetPeakBps(50u);

  ASSERT_TRUE(allocation.SetBitrate(0, 0, 1000u));
  ASSERT_EQ(allocation.GetSumBitrate().peak_bps(), 1000u);
}

TEST(VideoBitrateAllocationTest, SetPeakBps_GreaterThanSum_Succeeds) {
  VideoBitrateAllocation allocation(Bitrate::Mode::kVariable);
  allocation.SetBitrate(0, 0, 500u);
  allocation.SetBitrate(0, 1, 500u);
  allocation.SetBitrate(1, 0, 500u);

  ASSERT_TRUE(allocation.SetPeakBps(2000u));
  ASSERT_EQ(allocation.GetSumBitrate().peak_bps(), 2000u);
}

TEST(VideoBitrateAllocationTest, SetPeakBps_EqualToSum_Succeeds) {
  VideoBitrateAllocation allocation(Bitrate::Mode::kVariable);
  allocation.SetBitrate(0, 0, 400u);
  allocation.SetBitrate(0, 1, 300u);
  allocation.SetBitrate(1, 0, 300u);

  EXPECT_EQ(allocation.GetSumBps(), 1000u);

  ASSERT_TRUE(allocation.SetPeakBps(1000u));
  ASSERT_EQ(allocation.GetSumBitrate().peak_bps(), 1000u);
}

TEST(VideoBitrateAllocationTest, SetPeakBps_ImplicitConstantBitrate_Fails) {
  VideoBitrateAllocation allocation;

  ASSERT_FALSE(allocation.SetPeakBps(1u));
}

TEST(VideoBitrateAllocationTest, SetPeakBps_ConstantBitrate_Fails) {
  VideoBitrateAllocation allocation(Bitrate::Mode::kConstant);

  ASSERT_FALSE(allocation.SetPeakBps(1u));
}

TEST(VideoBitrateAllocationTest, SetPeakBps_PeakLessThanSum_Fails) {
  VideoBitrateAllocation allocation(Bitrate::Mode::kVariable);
  allocation.SetBitrate(0, 0, 1000u);

  ASSERT_FALSE(allocation.SetPeakBps(999u));
}

TEST(VideoBitrateAllocationTest, CanSetMaxValue) {
  VideoBitrateAllocation allocation;
  // Single cell containing max value.
  EXPECT_TRUE(
      allocation.SetBitrate(0, 0, std::numeric_limits<uint32_t>::max()));
  // Setting to 0 is OK. Doesn't increase sum.
  EXPECT_TRUE(allocation.SetBitrate(0, 1, 0u));
  // Adding 1 will overflow.
  EXPECT_FALSE(allocation.SetBitrate(0, 2, 1u));

  EXPECT_EQ(std::numeric_limits<uint32_t>::max(), allocation.GetSumBps());
}

TEST(VideoBitrateAllocationTest, ValidatesSumWhenOverwriting) {
  VideoBitrateAllocation allocation;
  // Fill up to max sum.
  EXPECT_TRUE(
      allocation.SetBitrate(0, 0, std::numeric_limits<uint32_t>::max() - 2));
  EXPECT_TRUE(allocation.SetBitrate(0, 1, 2u));
  // This will overflow, old value kept.
  EXPECT_FALSE(allocation.SetBitrate(0, 1, 3u));
  EXPECT_EQ(allocation.GetBitrateBps(0, 1), 2u);
  // OK only since we subtract the previous 2.
  EXPECT_TRUE(allocation.SetBitrate(0, 1, 1u));
  EXPECT_EQ(allocation.GetBitrateBps(0, 1), 1u);

  EXPECT_EQ(std::numeric_limits<uint32_t>::max() - 1, allocation.GetSumBps());
}

TEST(VideoBitrateAllocationTest, CanCopyAndCompare) {
  VideoBitrateAllocation allocation;
  EXPECT_TRUE(allocation.SetBitrate(0, 0, 1000u));
  EXPECT_TRUE(allocation.SetBitrate(
      VideoBitrateAllocation::kMaxSpatialLayers - 1,
      VideoBitrateAllocation::kMaxTemporalLayers - 1, 2000u));

  VideoBitrateAllocation copy = allocation;
  EXPECT_EQ(copy, allocation);
  copy.SetBitrate(0, 0, 0u);
  EXPECT_NE(copy, allocation);
}

TEST(VideoBitrateAllocationTest, ToString) {
  VideoBitrateAllocation allocation;
  EXPECT_TRUE(allocation.SetBitrate(0, 0, 123u));
  EXPECT_TRUE(allocation.SetBitrate(0, 1, 456u));
  EXPECT_TRUE(allocation.SetBitrate(0, 2, 789u));
  EXPECT_EQ(allocation.ToString(),
            "active spatial layers: 1, {SL#0: {123, 456, 789}}, mode CBR");

  // Add spatial layer.
  EXPECT_TRUE(allocation.SetBitrate(1, 0, 789u));
  EXPECT_TRUE(allocation.SetBitrate(1, 1, 456u));
  EXPECT_EQ(allocation.ToString(),
            "active spatial layers: 2, {SL#0: {123, 456, 789}, SL#1: {789, "
            "456}}, mode CBR");

  // Reset the bottom spatial layer.
  EXPECT_TRUE(allocation.SetBitrate(0, 0, 0u));
  EXPECT_TRUE(allocation.SetBitrate(0, 1, 0u));
  EXPECT_TRUE(allocation.SetBitrate(0, 2, 0u));
  EXPECT_EQ(allocation.ToString(),
            "active spatial layers: 1, {SL#1: {789, 456}}, mode CBR");

  // Add one more spatial layer.
  EXPECT_TRUE(allocation.SetBitrate(2, 0, 123u));
  EXPECT_EQ(
      allocation.ToString(),
      "active spatial layers: 2, {SL#1: {789, 456}, SL#2: {123}}, mode CBR");

  // Reset all the spatial layers.
  EXPECT_TRUE(allocation.SetBitrate(1, 0, 0u));
  EXPECT_TRUE(allocation.SetBitrate(1, 1, 0u));
  EXPECT_TRUE(allocation.SetBitrate(2, 0, 0u));
  EXPECT_EQ(allocation.ToString(), "Empty VideoBitrateAllocation");
}

TEST(VideoBitrateAllocationTest, ToString_VariableBitrateAndSingleLayer) {
  VideoBitrateAllocation allocation(Bitrate::Mode::kVariable);
  allocation.SetBitrate(0, 0, 1u);

  EXPECT_EQ(allocation.ToString(),
            "active spatial layers: 1, {SL#0: {1}}, mode VBR with peak bps 1");
}

TEST(VideoBitrateAllocationTest, ToString_VariableBitrateAndMultiLayer) {
  VideoBitrateAllocation allocation(Bitrate::Mode::kVariable);
  allocation.SetBitrate(0, 0, 1u);
  allocation.SetBitrate(0, 1, 2u);
  allocation.SetBitrate(0, 2, 3u);
  allocation.SetBitrate(1, 0, 4u);
  allocation.SetBitrate(1, 1, 5u);
  allocation.SetBitrate(1, 2, 6u);
  allocation.SetPeakBps(100u);

  EXPECT_EQ(allocation.ToString(),
            "active spatial layers: 2, {SL#0: {1, 2, 3}, SL#1: {4, 5, 6}}, "
            "mode VBR with peak bps 100");
}

}  // namespace media
