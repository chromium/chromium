// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cpu_performance/cpu_performance.h"

#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using CpuPerformanceTest = testing::Test;
using Tier = cpu_performance::Tier;

TEST_F(CpuPerformanceTest, GetTierFromCores) {
  std::vector<std::pair<int, Tier>> tests = {
      {1, Tier::kLow},    {2, Tier::kLow},     {3, Tier::kMid},
      {4, Tier::kMid},    {5, Tier::kHigh},    {8, Tier::kHigh},
      {12, Tier::kHigh},  {13, Tier::kUltra},  {16, Tier::kUltra},
      {96, Tier::kUltra}, {0, Tier::kUnknown}, {-42, Tier::kUnknown},
  };

  for (auto [cores, expected_tier] : tests) {
    Tier tier = cpu_performance::GetTierFromCores(cores);
    EXPECT_EQ(expected_tier, tier);
  }
}

}  // namespace content
