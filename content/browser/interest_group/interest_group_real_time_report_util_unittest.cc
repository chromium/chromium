// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_real_time_report_util.h"

#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class InterestGroupRealTimeReportUtilTest : public testing::Test {
 public:
  using RealTimeReportingContributions =
      std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>;

  InterestGroupRealTimeReportUtilTest() = default;

  ~InterestGroupRealTimeReportUtilTest() override = default;
};

// The proposed epsilon value of 1 yielding a flipping probability of ~0.378.
// Running the flipping a lot of times should get a flipping probability close
// to that.
TEST_F(InterestGroupRealTimeReportUtilTest, RapporFlippingProbability) {
  const size_t kIterations = 100;
  int kNumBuckets = 1024;
  int total_flipped = 0;

  for (size_t i = 0; i < kIterations; i++) {
    // Use std::nullopt as the bucket, so that the histogram before flipping
    // would be all 0s, and the sum of histogram after calling Rappor will be
    // the number of flipped bits.
    std::vector<uint8_t> histogram =
        Rappor(/*maybe_bucket=*/std::nullopt, /*epsilon=*/1.0,
               /*num_buckets=*/kNumBuckets);
    for (size_t j = 0; j < static_cast<size_t>(kNumBuckets); j++) {
      total_flipped += histogram[j];
    }
  }

  EXPECT_GT(total_flipped, 0.35 * kNumBuckets * kIterations);
  EXPECT_LT(total_flipped, 0.4 * kNumBuckets * kIterations);
}

// 0 and 1 in histogram are both flipped (and not flipped) randomly.
TEST_F(InterestGroupRealTimeReportUtilTest, RapporFlippingIsNonDeterministic) {
  // Two buckets, with the bucket being 1, so that before flipping, the
  // histogram is [0, 1].
  int kNumBuckets = 2;
  int32_t bucket = 1;
  base::flat_set<std::vector<uint8_t>> seen;
  while (seen.size() < 4) {
    std::vector<uint8_t> histogram =
        Rappor(bucket, /*epsilon=*/1.0, kNumBuckets);
    ASSERT_THAT(histogram, testing::AnyOf(testing::ElementsAreArray({0, 0}),
                                          testing::ElementsAreArray({0, 1}),
                                          testing::ElementsAreArray({1, 0}),
                                          testing::ElementsAreArray({1, 1})));
    seen.insert(histogram);
  }
}

TEST_F(InterestGroupRealTimeReportUtilTest, SampleContributionsNoContribution) {
  EXPECT_EQ(std::nullopt, SampleContributions(/*contributions=*/{}));
}

TEST_F(InterestGroupRealTimeReportUtilTest,
       SampleContributionsOneContribution) {
  auction_worklet::mojom::RealTimeReportingContribution histogram(
      /*bucket=*/100, /*priority_weight=*/0.5,
      /*latency_threshold=*/std::nullopt);
  RealTimeReportingContributions contributions;
  contributions.push_back(histogram.Clone());

  EXPECT_EQ(100, SampleContributions(contributions));
}

// All contributions should get a chance to be selected if we run the sampling
// a lot of times.
TEST_F(InterestGroupRealTimeReportUtilTest,
       SampleContributionsMultipleContributions) {
  auction_worklet::mojom::RealTimeReportingContribution contribution(
      /*bucket=*/1, /*priority_weight=*/0.95,
      /*latency_threshold=*/std::nullopt);
  auction_worklet::mojom::RealTimeReportingContribution contribution2(
      /*bucket=*/100, /*priority_weight=*/1,
      /*latency_threshold=*/std::nullopt);
  auction_worklet::mojom::RealTimeReportingContribution latency_contribution(
      /*bucket=*/200, /*priority_weight=*/1.05,
      /*latency_threshold=*/0);
  auction_worklet::mojom::RealTimeReportingContribution latency_contribution2(
      /*bucket=*/201, /*priority_weight=*/1,
      /*latency_threshold=*/100);

  RealTimeReportingContributions contributions;
  contributions.push_back(contribution.Clone());
  contributions.push_back(contribution2.Clone());
  contributions.push_back(latency_contribution.Clone());
  contributions.push_back(latency_contribution2.Clone());

  base::flat_set<int32_t> seen;
  while (seen.size() < 4) {
    std::optional<int32_t> bucket = SampleContributions(contributions);
    ASSERT_TRUE(bucket.has_value());
    ASSERT_THAT(*bucket, testing::AnyOf(1, 100, 200, 201));
    seen.insert(*bucket);
  }
}

// If all contributions have the same priority_weight, they should get roughly
// the same probability of being selected.
TEST_F(InterestGroupRealTimeReportUtilTest,
       SampleContributionsEqualPriorityWeight) {
  auction_worklet::mojom::RealTimeReportingContribution contribution1(
      /*bucket=*/1, /*priority_weight=*/1,
      /*latency_threshold=*/std::nullopt);
  auction_worklet::mojom::RealTimeReportingContribution contribution2(
      /*bucket=*/100, /*priority_weight=*/1,
      /*latency_threshold=*/std::nullopt);

  RealTimeReportingContributions contributions;
  contributions.push_back(contribution1.Clone());
  contributions.push_back(contribution2.Clone());

  const size_t kIterations = 10000;
  int contribution1_selected_times = 0;
  for (size_t i = 0; i < kIterations; i++) {
    std::optional<int32_t> bucket = SampleContributions(contributions);
    ASSERT_TRUE(bucket.has_value());
    contribution1_selected_times += *bucket == 1;
  }

  EXPECT_GT(contribution1_selected_times, 0.45 * kIterations);
  EXPECT_LT(contribution1_selected_times, 0.55 * kIterations);
}

// If one contribution has extremely larger priority_weight, it should be
// selected with much higher probability.
TEST_F(InterestGroupRealTimeReportUtilTest,
       SampleContributionsMuchLargerPriorityWeight) {
  auction_worklet::mojom::RealTimeReportingContribution
      contribution_large_priority(
          /*bucket=*/100,
          /*priority_weight=*/std::numeric_limits<double>::max(),
          /*latency_threshold=*/std::nullopt);
  auction_worklet::mojom::RealTimeReportingContribution
      contribution_small_priority(
          /*bucket=*/1, /*priority_weight=*/0.1,
          /*latency_threshold=*/std::nullopt);

  RealTimeReportingContributions contributions;
  contributions.push_back(contribution_large_priority.Clone());
  contributions.push_back(contribution_small_priority.Clone());

  const size_t kIterations = 10000;
  int large_priority_selected_times = 0;
  for (size_t i = 0; i < kIterations; i++) {
    std::optional<int32_t> bucket = SampleContributions(contributions);
    ASSERT_TRUE(bucket.has_value());
    large_priority_selected_times += *bucket == 100;
  }

  EXPECT_GT(large_priority_selected_times, 0.99 * kIterations);
}

TEST_F(InterestGroupRealTimeReportUtilTest,
       CalculateRealTimeReportingHistograms) {
  const url::Origin origin_a = url::Origin::Create(GURL("https://a.test/"));
  const url::Origin origin_b = url::Origin::Create(GURL("https://b.test/"));
  auction_worklet::mojom::RealTimeReportingContribution contribution1(
      /*bucket=*/1, /*priority_weight=*/1,
      /*latency_threshold=*/std::nullopt);
  auction_worklet::mojom::RealTimeReportingContribution contribution2(
      /*bucket=*/100, /*priority_weight=*/1,
      /*latency_threshold=*/std::nullopt);

  std::map<url::Origin, RealTimeReportingContributions> contributions_map;
  RealTimeReportingContributions empty_contributions = {};
  // `origin_a` has no contributions, but will still get a histogram after
  // calling CalculateRealTimeReportingHistograms().
  contributions_map[origin_a] = std::move(empty_contributions);

  RealTimeReportingContributions contributions;
  contributions.push_back(contribution1.Clone());
  contributions.push_back(contribution2.Clone());
  // `origin_b` has two contributions, but will still get one histogram after
  // calling CalculateRealTimeReportingHistograms().
  contributions_map[origin_b] = std::move(contributions);

  std::map<url::Origin, std::vector<uint8_t>> histograms_map =
      CalculateRealTimeReportingHistograms(std::move(contributions_map));
  for (const url::Origin& origin : {origin_a, origin_b}) {
    auto it = histograms_map.find(origin);
    CHECK(it != histograms_map.end());
    // A histogram is a vector of length kFledgeRealTimeReportingNumBuckets,
    // and each element is either 0 or 1.
    EXPECT_EQ(static_cast<unsigned>(
                  blink::features::kFledgeRealTimeReportingNumBuckets.Get()),
              it->second.size());
    EXPECT_TRUE(base::ranges::all_of(
        it->second, [](uint8_t bit) { return bit == 0 || bit == 1; }));
  }
}

TEST_F(InterestGroupRealTimeReportUtilTest, GetRealTimeReportDestination) {
  EXPECT_EQ(GURL("https://a.test/.well-known/interest-group/real-time-report"),
            GetRealTimeReportDestination(
                url::Origin::Create(GURL("https://a.test/"))));
}

}  // namespace content
