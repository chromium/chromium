// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_congestion_analyzer.h"

#include <map>
#include <unordered_map>

#include "base/macros.h"
#include "base/optional.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "net/nqe/network_quality.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/nqe/observation_buffer.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace nqe {

namespace internal {

using NetworkCongestionAnalyzerTest = TestWithTaskEnvironment;

namespace {

constexpr float kEpsilon = 0.001f;

// These values should remain synchronized with the values in
// net/nqe/network_congestion_analyzer.cc.
constexpr int64_t kHighQueueingDelayMsec = 5000;
constexpr int64_t kMinEmptyQueueObservingTimeMsec = 1500;
constexpr base::TimeDelta
    kLowQueueingDelayThresholds[EFFECTIVE_CONNECTION_TYPE_LAST] = {
        base::TimeDelta::FromMilliseconds(400),
        base::TimeDelta::FromMilliseconds(400),
        base::TimeDelta::FromMilliseconds(400),
        base::TimeDelta::FromMilliseconds(400),
        base::TimeDelta::FromMilliseconds(40),
        base::TimeDelta::FromMilliseconds(15)};
static constexpr size_t kMinScoreForValidSamples = 50;

// Verifies that the network queueing delay is computed correctly based on RTT
// and downlink throughput observations.
TEST_F(NetworkCongestionAnalyzerTest, TestComputingQueueingDelay) {
  TestNetworkQualityEstimator network_quality_estimator;
  base::SimpleTestTickClock tick_clock;

  NetworkCongestionAnalyzer analyzer(&network_quality_estimator, &tick_clock);
  std::map<uint64_t, CanonicalStats> recent_rtt_stats;
  std::map<uint64_t, CanonicalStats> historical_rtt_stats;
  int32_t downlink_kbps = nqe::internal::INVALID_RTT_THROUGHPUT;

  // Checks that no result is updated when providing empty RTT observations and
  // an invalid downlink throughput observation.
  analyzer.ComputeRecentQueueingDelay(recent_rtt_stats, historical_rtt_stats,
                                      downlink_kbps);
  EXPECT_TRUE(analyzer.recent_queueing_delay().is_zero());

  const uint64_t host_1 = 0x101010UL;
  const uint64_t host_2 = 0x202020UL;
  // Checks that the queueing delay is updated based on hosts with valid RTT
  // observations. For example, the computation should be done by using data
  // from host 1 only because host 2 does not provide a valid min RTT value.
  std::map<int32_t, int32_t> recent_stat_host_1 = {{kStatVal0p, 1100}};
  std::map<int32_t, int32_t> historical_stat_host_1 = {{kStatVal0p, 600}};
  CanonicalStats recent_rtt_host_1 =
      CanonicalStats(recent_stat_host_1, 1400, 5);
  CanonicalStats historical_rtt_host_1 =
      CanonicalStats(historical_stat_host_1, 1400, 15);

  std::map<int32_t, int32_t> recent_stat_host_2 = {{kStatVal0p, 1200}};
  std::map<int32_t, int32_t> historical_stat_host_2 = {{kStatVal50p, 1200}};
  CanonicalStats recent_rtt_host_2 =
      CanonicalStats(recent_stat_host_2, 1600, 3);
  CanonicalStats historical_rtt_host_2 =
      CanonicalStats(historical_stat_host_2, 1600, 8);
  recent_rtt_stats.emplace(host_1, recent_rtt_host_1);
  recent_rtt_stats.emplace(host_2, recent_rtt_host_2);
  historical_rtt_stats.emplace(host_1, historical_rtt_host_1);
  historical_rtt_stats.emplace(host_2, historical_rtt_host_2);

  analyzer.ComputeRecentQueueingDelay(recent_rtt_stats, historical_rtt_stats,
                                      downlink_kbps);
  EXPECT_EQ(800, analyzer.recent_queueing_delay().InMilliseconds());

  // Checks that the queueing delay is updated correctly based on all hosts when
  // RTT observations and the throughput observation are valid.
  historical_rtt_stats[host_2].canonical_pcts[kStatVal0p] = 1000;
  downlink_kbps = 120;
  analyzer.ComputeRecentQueueingDelay(recent_rtt_stats, historical_rtt_stats,
                                      downlink_kbps);
  EXPECT_EQ(700, analyzer.recent_queueing_delay().InMilliseconds());
  EXPECT_NEAR(7.0, analyzer.recent_queue_length().value_or(0), kEpsilon);
}

}  // namespace

// Verifies that a measurement period starts correctly when an empty queue
// observation shows up. An empty queue observation is made when the queueing
// delay is low and the number of in-flight requests is also low.
TEST_F(NetworkCongestionAnalyzerTest, TestStartingNewMeasurementPeriod) {
  TestNetworkQualityEstimator network_quality_estimator;
  base::SimpleTestTickClock tick_clock;

  network_quality_estimator.set_effective_connection_type(
      EFFECTIVE_CONNECTION_TYPE_2G);
  NetworkCongestionAnalyzer analyzer(&network_quality_estimator, &tick_clock);
  base::TimeDelta low_queueing_delay_sample =
      kLowQueueingDelayThresholds[EFFECTIVE_CONNECTION_TYPE_2G];

  // Checks that a new measurement period starts immediately if the queueing
  // delay is low and the number of in-flight requests are equal or less than 1.
  EXPECT_FALSE(
      analyzer.ShouldStartNewMeasurement(low_queueing_delay_sample, 2));
  EXPECT_TRUE(analyzer.ShouldStartNewMeasurement(low_queueing_delay_sample, 1));
  EXPECT_TRUE(analyzer.ShouldStartNewMeasurement(low_queueing_delay_sample, 0));

  // Checks that a new measurement period starts after waiting for a sufficient
  // time interval when the number of in-flight requests is 2.
  EXPECT_FALSE(
      analyzer.ShouldStartNewMeasurement(low_queueing_delay_sample, 2));
  tick_clock.Advance(
      base::TimeDelta::FromMilliseconds(kMinEmptyQueueObservingTimeMsec / 2));
  EXPECT_FALSE(
      analyzer.ShouldStartNewMeasurement(low_queueing_delay_sample, 2));
  tick_clock.Advance(
      base::TimeDelta::FromMilliseconds(kMinEmptyQueueObservingTimeMsec / 2));
  EXPECT_TRUE(analyzer.ShouldStartNewMeasurement(low_queueing_delay_sample, 2));
}

// Verifies that the peak queueing delay is correctly mapped to the count of
// in-flight requests that are responsible for that delay.
TEST_F(NetworkCongestionAnalyzerTest, TestUpdatePeakDelayMapping) {
  TestNetworkQualityEstimator network_quality_estimator;
  base::SimpleTestTickClock tick_clock;

  network_quality_estimator.set_effective_connection_type(
      EFFECTIVE_CONNECTION_TYPE_2G);
  NetworkCongestionAnalyzer analyzer(&network_quality_estimator, &tick_clock);
  EXPECT_EQ(base::nullopt,
            analyzer.count_inflight_requests_causing_high_delay());

  // Checks that the count of in-flight requests for peak queueing delay is
  // correctly recorded.
  // Case #1: the peak queueing delay was observed after the max count (7) of
  // in-flight requests was observed.
  const size_t expected_count_requests_1 = 7;
  std::vector<std::pair<base::TimeDelta, size_t>> queueing_delay_samples_1 = {
      std::make_pair(base::TimeDelta::FromMilliseconds(10), 1),
      std::make_pair(base::TimeDelta::FromMilliseconds(10), 3),
      std::make_pair(base::TimeDelta::FromMilliseconds(400), 5),
      std::make_pair(base::TimeDelta::FromMilliseconds(800),
                     expected_count_requests_1),
      std::make_pair(base::TimeDelta::FromMilliseconds(kHighQueueingDelayMsec),
                     5),
      std::make_pair(base::TimeDelta::FromMilliseconds(1000), 3),
      std::make_pair(base::TimeDelta::FromMilliseconds(700), 3),
      std::make_pair(base::TimeDelta::FromMilliseconds(600), 1),
      std::make_pair(base::TimeDelta::FromMilliseconds(300), 0),
  };
  for (const auto& sample : queueing_delay_samples_1) {
    analyzer.UpdatePeakDelayMapping(sample.first, sample.second);
  }
  EXPECT_EQ(expected_count_requests_1,
            analyzer.count_inflight_requests_causing_high_delay().value_or(0));

  // Case #2: the peak queueing delay is observed before the max count (11) of
  // in-flight requests was observed. The 8 requests should be responsible for
  // the peak queueing delay.
  const size_t expected_count_requests_2 = 10;
  std::vector<std::pair<base::TimeDelta, size_t>> queueing_delay_samples_2 = {
      std::make_pair(base::TimeDelta::FromMilliseconds(10), 1),
      std::make_pair(base::TimeDelta::FromMilliseconds(10), 3),
      std::make_pair(base::TimeDelta::FromMilliseconds(400), 5),
      std::make_pair(base::TimeDelta::FromMilliseconds(800), 5),
      std::make_pair(base::TimeDelta::FromMilliseconds(kHighQueueingDelayMsec),
                     expected_count_requests_2),
      std::make_pair(base::TimeDelta::FromMilliseconds(3000), 11),
      std::make_pair(base::TimeDelta::FromMilliseconds(700), 3),
      std::make_pair(base::TimeDelta::FromMilliseconds(600), 1),
      std::make_pair(base::TimeDelta::FromMilliseconds(300), 0),
  };
  for (const auto& sample : queueing_delay_samples_2) {
    analyzer.UpdatePeakDelayMapping(sample.first, sample.second);
  }
  EXPECT_EQ(expected_count_requests_2,
            analyzer.count_inflight_requests_causing_high_delay().value_or(0));
}

// Verifies that the network congestion analyzer can correctly determine whether
// a queueing delay sample is low or not under different effective connection
// types (ECTs).
TEST_F(NetworkCongestionAnalyzerTest, TestDetectLowQueueingDelay) {
  TestNetworkQualityEstimator network_quality_estimator;
  base::SimpleTestTickClock tick_clock;
  NetworkCongestionAnalyzer analyzer(&network_quality_estimator, &tick_clock);

  // Checks that computations are done correctly under all different ECTs.
  for (int i = 0; i != net::EFFECTIVE_CONNECTION_TYPE_LAST; ++i) {
    auto type = static_cast<net::EffectiveConnectionType>(i);
    network_quality_estimator.set_effective_connection_type(type);
    base::TimeDelta low_queueing_delay = kLowQueueingDelayThresholds[type];

    EXPECT_TRUE(analyzer.IsQueueingDelayLow(low_queueing_delay));
    EXPECT_FALSE(analyzer.IsQueueingDelayLow(
        low_queueing_delay + base::TimeDelta::FromMilliseconds(1)));
  }
}

// Verifies that the network congestion analyzer can correctly bucketize the
// peak queueing delay samples, and map the peak queueing delay samples to their
// corresponding queueing delay levels from Level1 to Level10.
TEST_F(NetworkCongestionAnalyzerTest, TestComputeQueueingDelayLevel) {
  TestNetworkQualityEstimator network_quality_estimator;
  base::SimpleTestTickClock tick_clock;
  NetworkCongestionAnalyzer analyzer(&network_quality_estimator, &tick_clock);

  std::vector<std::pair<base::TimeDelta, size_t>> queueing_delay_level_samples =
      {std::make_pair(base::TimeDelta::FromMilliseconds(0), 1),
       std::make_pair(base::TimeDelta::FromMilliseconds(25), 1),
       std::make_pair(base::TimeDelta::FromMilliseconds(35), 2),
       std::make_pair(base::TimeDelta::FromMilliseconds(55), 2),
       std::make_pair(base::TimeDelta::FromMilliseconds(65), 3),
       std::make_pair(base::TimeDelta::FromMilliseconds(115), 3),
       std::make_pair(base::TimeDelta::FromMilliseconds(125), 4),
       std::make_pair(base::TimeDelta::FromMilliseconds(245), 4),
       std::make_pair(base::TimeDelta::FromMilliseconds(255), 5),
       std::make_pair(base::TimeDelta::FromMilliseconds(495), 5),
       std::make_pair(base::TimeDelta::FromMilliseconds(505), 6),
       std::make_pair(base::TimeDelta::FromMilliseconds(995), 6),
       std::make_pair(base::TimeDelta::FromMilliseconds(1005), 7),
       std::make_pair(base::TimeDelta::FromMilliseconds(1995), 7),
       std::make_pair(base::TimeDelta::FromMilliseconds(2005), 8),
       std::make_pair(base::TimeDelta::FromMilliseconds(3995), 8),
       std::make_pair(base::TimeDelta::FromMilliseconds(4005), 9),
       std::make_pair(base::TimeDelta::FromMilliseconds(7995), 9),
       std::make_pair(base::TimeDelta::FromMilliseconds(8005), 10),
       std::make_pair(base::TimeDelta::FromMilliseconds(20000), 10)};

  // Checks that all queueing delay samples are correctly mapped to
  // their corresponding levels.
  for (const auto& sample : queueing_delay_level_samples) {
    EXPECT_EQ(sample.second,
              analyzer.ComputePeakQueueingDelayLevel(sample.first));
  }
}

// Verifies that the mapping sample is correctly evaluated with the computation
// of the count-delay mapping score. Also, verifies that samples are inserted
// into the cache when their mapping scores exceed the threshold.
TEST_F(NetworkCongestionAnalyzerTest, TestComputePeakDelayMappingSampleScore) {
  TestNetworkQualityEstimator network_quality_estimator;
  base::SimpleTestTickClock tick_clock;
  NetworkCongestionAnalyzer analyzer(&network_quality_estimator, &tick_clock);

  // Insert these samples into the cache. They are used to evaluate whether a
  // new sample is valid or not.
  std::map<size_t, base::TimeDelta> cached_requests_count_peak_delay_sample = {
      {1, base::TimeDelta::FromMilliseconds(100)},
      {2, base::TimeDelta::FromMilliseconds(200)},
      {3, base::TimeDelta::FromMilliseconds(300)},
      {4, base::TimeDelta::FromMilliseconds(400)},
      {5, base::TimeDelta::FromMilliseconds(500)},
      {6, base::TimeDelta::FromMilliseconds(600)},
      {7, base::TimeDelta::FromMilliseconds(700)},
      {8, base::TimeDelta::FromMilliseconds(800)},
      {9, base::TimeDelta::FromMilliseconds(900)},
      {10, base::TimeDelta::FromMilliseconds(1000)}};

  for (const auto& sample : cached_requests_count_peak_delay_sample) {
    analyzer.count_inflight_requests_for_peak_queueing_delay_ = sample.first;
    analyzer.peak_queueing_delay_ = sample.second;
    analyzer.UpdateRequestsCountAndPeakQueueingDelayMapping();
  }

  // Checks that the scores are correctly computed.
  std::vector<std::pair<size_t, base::TimeDelta>> mapping_sample = {
      std::make_pair(1, base::TimeDelta::FromMilliseconds(100)),
      std::make_pair(2, base::TimeDelta::FromMilliseconds(200)),
      std::make_pair(3, base::TimeDelta::FromMilliseconds(400)),
      std::make_pair(4, base::TimeDelta::FromMilliseconds(800)),
      std::make_pair(5, base::TimeDelta::FromMilliseconds(400)),
      std::make_pair(6, base::TimeDelta::FromMilliseconds(200)),
      std::make_pair(7, base::TimeDelta::FromMilliseconds(200)),
      std::make_pair(8, base::TimeDelta::FromMilliseconds(200)),
      std::make_pair(9, base::TimeDelta::FromMilliseconds(200)),
      std::make_pair(10, base::TimeDelta::FromMilliseconds(900)),
      std::make_pair(11, base::TimeDelta::FromMilliseconds(500))};
  std::vector<size_t> ground_truth_mapping_score = {100, 100, 90, 60, 90, 60,
                                                    50,  40,  30, 90, 40};

  EXPECT_EQ(mapping_sample.size(), ground_truth_mapping_score.size());
  EXPECT_EQ(10u, analyzer.count_peak_queueing_delay_mapping_sample_);

  for (size_t i = 0; i < mapping_sample.size(); ++i) {
    EXPECT_EQ(analyzer.ComputePeakDelayMappingSampleScore(
                  mapping_sample[i].first, mapping_sample[i].second),
              ground_truth_mapping_score[i]);
  }

  // Checks that only samples with a score higher than a threshold would be
  // inserted into the cache.
  size_t inserted_samples_count = 0;
  size_t samples_count_before = 0;
  bool is_inserted = false;
  for (const auto& sample : mapping_sample) {
    samples_count_before =
        analyzer.count_inflight_requests_to_queueing_delay_[sample.first]
            .size();
    is_inserted =
        analyzer.ComputePeakDelayMappingSampleScore(sample.first, sample.second)
            .value_or(0) >= kMinScoreForValidSamples;
    // Inserts the new sample.
    analyzer.count_inflight_requests_for_peak_queueing_delay_ = sample.first;
    analyzer.peak_queueing_delay_ = sample.second;
    analyzer.UpdateRequestsCountAndPeakQueueingDelayMapping();
    if (is_inserted) {
      ++inserted_samples_count;
      EXPECT_EQ(
          samples_count_before + 1,
          analyzer.count_inflight_requests_to_queueing_delay_[sample.first]
              .size());
    } else {
      EXPECT_EQ(
          samples_count_before,
          analyzer.count_inflight_requests_to_queueing_delay_[sample.first]
              .size());
    }
  }

  EXPECT_EQ(10u + inserted_samples_count,
            analyzer.count_peak_queueing_delay_mapping_sample_);
}

}  // namespace internal

}  // namespace nqe

}  // namespace net
