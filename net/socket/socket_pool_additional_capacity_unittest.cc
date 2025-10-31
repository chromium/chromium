// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_pool_additional_capacity.h"

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace net {

namespace {

const SocketPoolAdditionalCapacity kEmptyPool =
    SocketPoolAdditionalCapacity::CreateForTest(0.0, 0, 0.0, 0.0);

TEST(SocketPoolAdditionalCapacityTest, CreateWithDisabledFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kTcpSocketPoolLimitRandomization);
  EXPECT_EQ(SocketPoolAdditionalCapacity::Create(), kEmptyPool);
}

TEST(SocketPoolAdditionalCapacityTest, CreateWithEnabledFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kTcpSocketPoolLimitRandomization,
      {
          {
              "TcpSocketPoolLimitRandomizationBase",
              "0.1",
          },
          {
              "TcpSocketPoolLimitRandomizationCapacity",
              "2",
          },
          {
              "TcpSocketPoolLimitRandomizationMinimum",
              "0.3",
          },
          {
              "TcpSocketPoolLimitRandomizationNoise",
              "0.4",
          },
      });
  EXPECT_EQ(SocketPoolAdditionalCapacity::Create(),
            SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 0.3, 0.4));
}

TEST(SocketPoolAdditionalCapacityTest, CreateForTest) {
  EXPECT_EQ(std::string(
                SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 0.3, 0.4)),
            "SocketPoolAdditionalCapacity(base:1.000000e-01,capacity:2,minimum:"
            "3.000000e-01,noise:4.000000e-01)");
}

TEST(SocketPoolAdditionalCapacityTest, InvalidCreation) {
  // base range
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(-0.1, 2, 0.3, 0.4),
            kEmptyPool);
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(1.1, 2, 0.3, 0.4),
            kEmptyPool);
  EXPECT_EQ(
      SocketPoolAdditionalCapacity::CreateForTest(std::nan(""), 2, 0.3, 0.4),
      kEmptyPool);

  // capacity range
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(0.1, -2, 0.3, 0.4),
            kEmptyPool);
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(0.1, 2000, 0.3, 0.4),
            kEmptyPool);

  // minimum range
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, -0.3, 0.4),
            kEmptyPool);
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 1.3, 0.4),
            kEmptyPool);
  EXPECT_EQ(
      SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, std::nan(""), 0.4),
      kEmptyPool);

  // noise range
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 0.3, -0.4),
            kEmptyPool);
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 0.3, 1.4),
            kEmptyPool);
  EXPECT_EQ(
      SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 0.3, std::nan("")),
      kEmptyPool);
}

TEST(SocketPoolAdditionalCapacityTest, NextStateBeforeAllocation) {
  // We use a base and noise of 0.0 with a minimum of 0.5 to ensure every roll
  // is a 50/50 shot so that we don't need to run the test millions of times
  // for flakes to be noticeable. The capacity of 2 is needed to test the logic.
  SocketPoolAdditionalCapacity pool_capacity =
      SocketPoolAdditionalCapacity::CreateForTest(0.0, 2, 0.5, 0.0);

  // Test out of bounds cases
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kUncapped, -2, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kCapped, -2, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kUncapped, 2, -2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kCapped, 2, -2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kUncapped, 5, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kCapped, 5, 2));

  // Below soft cap we are always uncapped
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateBeforeAllocation(
                                            SocketPoolState::kUncapped, 0, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateBeforeAllocation(
                                            SocketPoolState::kCapped, 0, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateBeforeAllocation(
                                            SocketPoolState::kUncapped, 1, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateBeforeAllocation(
                                            SocketPoolState::kCapped, 1, 2));

  // At hard cap we are always capped
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kCapped, 4, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kUncapped, 4, 2));

  // If capped at or above soft cap we always stay that way
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kCapped, 2, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kCapped, 3, 2));

  // When uncapped between soft and hard cap, we should be able to see some
  // distribution of each option. The probability inputs here should make it
  // a coin toss, but to prevent this from being flakey we run it 1000 times.
  bool did_see_uncapped = false;
  bool did_see_capped = false;
  for (int i = 0; i < 1000; ++i) {
    if (SocketPoolState::kUncapped == pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kUncapped, 3, 2)) {
      did_see_uncapped = true;
    } else {
      did_see_capped = true;
    }
  }
  EXPECT_TRUE(did_see_uncapped);
  EXPECT_TRUE(did_see_capped);
}

TEST(SocketPoolAdditionalCapacityTest, NextStateAfterRelease) {
  // We use a base and noise of 0.0 with a minimum of 0.5 to ensure every roll
  // is a 50/50 shot so that we don't need to run the test millions of times
  // for flakes to be noticeable. The capacity of 2 is needed to test the logic.
  SocketPoolAdditionalCapacity pool_capacity =
      SocketPoolAdditionalCapacity::CreateForTest(0.0, 2, 0.5, 0.0);

  // Test out of bounds cases
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateAfterRelease(
                                          SocketPoolState::kUncapped, -2, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateAfterRelease(
                                          SocketPoolState::kCapped, -2, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateAfterRelease(
                                          SocketPoolState::kUncapped, 2, -2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateAfterRelease(
                                          SocketPoolState::kCapped, 2, -2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateAfterRelease(
                                          SocketPoolState::kUncapped, 5, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateAfterRelease(
                                          SocketPoolState::kCapped, 5, 2));

  // Below soft cap we are always uncapped
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateAfterRelease(
                                            SocketPoolState::kUncapped, 0, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateAfterRelease(
                                            SocketPoolState::kCapped, 0, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateAfterRelease(
                                            SocketPoolState::kUncapped, 1, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateAfterRelease(
                                            SocketPoolState::kCapped, 1, 2));

  // At hard cap we are always capped
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateAfterRelease(
                                          SocketPoolState::kCapped, 4, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateAfterRelease(
                                          SocketPoolState::kUncapped, 4, 2));

  // If uncapped at or above soft cap we always stay that way
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateAfterRelease(
                                            SocketPoolState::kUncapped, 2, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateAfterRelease(
                                            SocketPoolState::kUncapped, 3, 2));

  // When capped between soft and hard cap, we should be able to see some
  // distribution of each option. The probability inputs here should make it
  // a coin toss, but to prevent this from being flakey we run it 1000 times.
  bool did_see_uncapped = false;
  bool did_see_capped = false;
  for (int i = 0; i < 1000; ++i) {
    if (SocketPoolState::kUncapped ==
        pool_capacity.NextStateAfterRelease(SocketPoolState::kCapped, 3, 2)) {
      did_see_uncapped = true;
    } else {
      did_see_capped = true;
    }
  }
  EXPECT_TRUE(did_see_uncapped);
  EXPECT_TRUE(did_see_capped);
}

TEST(SocketPoolAdditionalCapacityTest, EmptyPool) {
  // No sockets in use
  EXPECT_EQ(
      SocketPoolState::kUncapped,
      kEmptyPool.NextStateBeforeAllocation(SocketPoolState::kUncapped, 0, 256));
  EXPECT_EQ(
      SocketPoolState::kUncapped,
      kEmptyPool.NextStateAfterRelease(SocketPoolState::kUncapped, 0, 256));
  EXPECT_EQ(SocketPoolState::kUncapped, kEmptyPool.NextStateBeforeAllocation(
                                            SocketPoolState::kCapped, 0, 256));
  EXPECT_EQ(SocketPoolState::kUncapped,
            kEmptyPool.NextStateAfterRelease(SocketPoolState::kCapped, 0, 256));

  // 50% of soft cap in use
  EXPECT_EQ(SocketPoolState::kUncapped,
            kEmptyPool.NextStateBeforeAllocation(SocketPoolState::kUncapped,
                                                 128, 256));
  EXPECT_EQ(
      SocketPoolState::kUncapped,
      kEmptyPool.NextStateAfterRelease(SocketPoolState::kUncapped, 128, 256));
  EXPECT_EQ(
      SocketPoolState::kUncapped,
      kEmptyPool.NextStateBeforeAllocation(SocketPoolState::kCapped, 128, 256));
  EXPECT_EQ(
      SocketPoolState::kUncapped,
      kEmptyPool.NextStateAfterRelease(SocketPoolState::kCapped, 128, 256));

  // 100% of soft cap in use
  EXPECT_EQ(SocketPoolState::kCapped,
            kEmptyPool.NextStateBeforeAllocation(SocketPoolState::kUncapped,
                                                 256, 256));
  EXPECT_EQ(
      SocketPoolState::kCapped,
      kEmptyPool.NextStateAfterRelease(SocketPoolState::kUncapped, 256, 256));
  EXPECT_EQ(SocketPoolState::kCapped, kEmptyPool.NextStateBeforeAllocation(
                                          SocketPoolState::kCapped, 256, 256));
  EXPECT_EQ(SocketPoolState::kCapped, kEmptyPool.NextStateAfterRelease(
                                          SocketPoolState::kCapped, 256, 256));
}

TEST(SocketPoolAdditionalCapacityTest,
     TestDefaultDistributionForFieldTrialConfig) {
  // We want to validate the default config in the field trial here.
  SocketPoolAdditionalCapacity pool_capacity =
      SocketPoolAdditionalCapacity::CreateForTest(0.000001, 256, 0.01, 0.2);

  // In order to do that we need an easy way to measure distributions.
  // Since we are applying noise, we run a ten thousand variants.
  auto percentage_transition_for_allocation_and_release =
      [&](int sockets_in_use) -> std::tuple<double, double> {
    int transition_allocation_count = 0;
    int transition_release_count = 0;
    for (int i = 0; i < 10000; ++i) {
      if (SocketPoolState::kCapped ==
          pool_capacity.NextStateBeforeAllocation(SocketPoolState::kUncapped,
                                                  sockets_in_use, 256)) {
        ++transition_allocation_count;
      }
      if (SocketPoolState::kUncapped ==
          pool_capacity.NextStateAfterRelease(SocketPoolState::kCapped,
                                              sockets_in_use, 256)) {
        ++transition_release_count;
      }
    }
    return {transition_allocation_count / 10000.0,
            transition_release_count / 10000.0};
  };

  // We want to validate the distribution at three points: 5%, 50%, and 95%.
  auto fifth_percentile = percentage_transition_for_allocation_and_release(268);
  auto fiftieth_percentile =
      percentage_transition_for_allocation_and_release(384);
  auto ninetyfifth_percentile =
      percentage_transition_for_allocation_and_release(500);

  // When allocating sockets and uncapped:
  // We expect a ~1% transition rate if 5% of additional sockets are in use.
  EXPECT_GT(std::get<0>(fifth_percentile), 0.00);
  EXPECT_LT(std::get<0>(fifth_percentile), 0.025);
  // We expect a ~1% transition rate if 50% of additional sockets are in use.
  EXPECT_GT(std::get<0>(fiftieth_percentile), 0.00);
  EXPECT_LT(std::get<0>(fiftieth_percentile), 0.025);
  // We expect a ~50% transition rate if 95% of additional sockets are in use.
  EXPECT_GT(std::get<0>(ninetyfifth_percentile), 0.35);
  EXPECT_LT(std::get<0>(ninetyfifth_percentile), 0.65);

  // When releasing sockets and capped:
  // We expect a ~50% transition rate if 5% of additional sockets are in use.
  EXPECT_GT(std::get<1>(fifth_percentile), 0.35);
  EXPECT_LT(std::get<1>(fifth_percentile), 0.65);
  // We expect a ~1% transition rate if 50% of additional sockets are in use.
  EXPECT_GT(std::get<1>(fiftieth_percentile), 0.00);
  EXPECT_LT(std::get<1>(fiftieth_percentile), 0.025);
  // We expect a ~1% transition rate if 95% of additional sockets are in use.
  EXPECT_GT(std::get<1>(ninetyfifth_percentile), 0.00);
  EXPECT_LT(std::get<1>(ninetyfifth_percentile), 0.025);
}

void ValidateRandomizedInputs(double base,
                              int capacity,
                              double minimum,
                              double noise,
                              bool capped,
                              int sockets_in_use,
                              int socket_soft_cap) {
  SocketPoolAdditionalCapacity pool =
      SocketPoolAdditionalCapacity::CreateForTest(base, capacity, minimum,
                                                  noise);
  // Because there's some randomization here, we want to run these a few times.
  for (int i = 0; i < 1000; ++i) {
    pool.NextStateBeforeAllocation(
        capped ? SocketPoolState::kCapped : SocketPoolState::kUncapped,
        sockets_in_use, socket_soft_cap);
    pool.NextStateAfterRelease(
        capped ? SocketPoolState::kCapped : SocketPoolState::kUncapped,
        sockets_in_use, socket_soft_cap);
  }
}
FUZZ_TEST(SocketPoolAdditionalCapacityTest, ValidateRandomizedInputs)
    .WithDomains(fuzztest::Arbitrary<double>(),
                 fuzztest::Arbitrary<int>(),
                 fuzztest::Arbitrary<double>(),
                 fuzztest::Arbitrary<double>(),
                 fuzztest::Arbitrary<bool>(),
                 fuzztest::Arbitrary<int>(),
                 fuzztest::Arbitrary<int>())
    .WithSeeds({
        {std::nan(""), 0, std::nan(""), std::nan(""), false, 0, 0},
        {0.0, 0, 0.0, 0.0, false, 0, 0},
        {0.3, 64, 0.1, 0.1, false, 96, 64},
        {0.6, 128, 0.2, 0.2, true, 192, 128},
        {1.0, 256, 1.0, 1.0, true, 320, 256},
        {1.0, 256, 1.0, 1.0, true, std::numeric_limits<int32_t>::max(),
         std::numeric_limits<int32_t>::max()},
    });

}  // namespace

}  // namespace net
