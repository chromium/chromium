// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/abr_algorithm.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media::hls {

TEST(AbrAlgorithmTest, EwmaUpdateSpeed) {
  EwmaAbrAlgorithm abr;
  abr.UpdateNetworkSpeed(1000);
  // weighted_old_ = 0 * 0.8 + 1000 * 0.2 = 200
  // weighted_new_ = 0 * 0.2 + 1000 * 0.8 = 800
  EXPECT_EQ(abr.GetABRSpeed(), 200u);

  abr.UpdateNetworkSpeed(2000);
  // weighted_old_ = 200 * 0.8 + 2000 * 0.2 = 160 + 400 = 560
  // weighted_new_ = 800 * 0.2 + 2000 * 0.8 = 160 + 1600 = 1760
  EXPECT_EQ(abr.GetABRSpeed(), 560u);

  abr.UpdateNetworkSpeed(1000);
  // weighted_old_ = 560 * 0.8 + 1000 * 0.2 = 448 + 200 = 648
  // weighted_new_ = 1760 * 0.2 + 1000 * 0.8 = 352 + 800 = 1152
  EXPECT_EQ(abr.GetABRSpeed(), 648u);
}

}  // namespace media::hls
