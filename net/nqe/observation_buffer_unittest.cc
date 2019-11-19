// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/observation_buffer.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/nqe/network_quality_observation.h"
#include "net/nqe/network_quality_observation_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace nqe {

namespace internal {

namespace {

// Verify that the buffer size is never exceeded.
TEST(NetworkQualityObservationBufferTest, BoundedBuffer) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  ObservationBuffer observation_buffer(&params, &tick_clock, 1.0, 1.0);
  const base::TimeTicks now =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
  for (int i = 1; i <= 1000; ++i) {
    observation_buffer.AddObservation(
        Observation(i, now, INT32_MIN, NETWORK_QUALITY_OBSERVATION_SOURCE_TCP));
    // The number of entries should be at most the maximum buffer size.
    EXPECT_GE(300u, observation_buffer.Size());
  }
}

// Verify that the percentiles are monotonically non-decreasing when a weight is
// applied.
TEST(NetworkQualityObservationBufferTest, GetPercentileWithWeights) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));

  ObservationBuffer observation_buffer(&params, &tick_clock, 0.98, 1.0);
  const base::TimeTicks now = tick_clock.NowTicks();
  for (int i = 1; i <= 100; ++i) {
    tick_clock.Advance(base::TimeDelta::FromSeconds(1));
    observation_buffer.AddObservation(
        Observation(i, tick_clock.NowTicks(), INT32_MIN,
                    NETWORK_QUALITY_OBSERVATION_SOURCE_TCP));
  }
  EXPECT_EQ(100U, observation_buffer.Size());

  int32_t result_lowest = INT32_MAX;
  int32_t result_highest = INT32_MIN;

  for (int i = 1; i <= 100; ++i) {
    size_t observations_count = 0;
    // Verify that i'th percentile is more than i-1'th percentile.
    base::Optional<int32_t> result_i = observation_buffer.GetPercentile(
        now, INT32_MIN, i, &observations_count);
    EXPECT_EQ(100u, observations_count);
    ASSERT_TRUE(result_i.has_value());
    result_lowest = std::min(result_lowest, result_i.value());

    result_highest = std::max(result_highest, result_i.value());

    base::Optional<int32_t> result_i_1 = observation_buffer.GetPercentile(
        now, INT32_MIN, i - 1, &observations_count);
    EXPECT_EQ(100u, observations_count);
    ASSERT_TRUE(result_i_1.has_value());

    EXPECT_LE(result_i_1.value(), result_i.value());
  }
  EXPECT_LT(result_lowest, result_highest);
}

// Verifies that the percentiles are correctly computed when results must be
// update for each individual host. All observations can have the same timestamp
// or different timestamps.
TEST(NetworkQualityObservationBufferTest, GetPercentileStatsForAllHosts) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  // The observation buffer holds mixed observations for different hosts.
  ObservationBuffer mixed_buffer(&params, &tick_clock, 0.5, 1.0);
  const base::TimeTicks now = tick_clock.NowTicks();
  const base::TimeTicks history = now - base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks future = now + base::TimeDelta::FromMilliseconds(1);
  const uint64_t host_1 = 0x101010UL;
  const uint64_t host_2 = 0x202020UL;
  const size_t total_observaions_count = 100;

  // Inserts samples from {1,2,3,...,100} for |host_1|. Insert samples from
  // {1,1,2,2,3,3,...,50,50} for |host_2|. Verifies all percentiles are
  // computed correctly for both hosts.
  for (size_t i = 1; i <= total_observaions_count; ++i) {
    mixed_buffer.AddObservation(Observation(
        i, now, INT32_MIN, NETWORK_QUALITY_OBSERVATION_SOURCE_TCP, host_1));
    mixed_buffer.AddObservation(
        Observation((i + 1) / 2, now, INT32_MIN,
                    NETWORK_QUALITY_OBSERVATION_SOURCE_TCP, host_2));
  }
  EXPECT_EQ(total_observaions_count * 2, mixed_buffer.Size());

  std::set<uint64_t> empty_hosts_set;
  std::map<uint64_t, CanonicalStats> recent_rtt_stats =
      mixed_buffer.GetCanonicalStatsKeyedByHosts(history, empty_hosts_set);

  // All observations are categories into two groups keyed by two hosts.
  // In each group, all percentile statistics are updated and the number of
  // available observations are also updated correctly.
  EXPECT_EQ(2u, recent_rtt_stats.size());
  EXPECT_EQ(total_observaions_count,
            recent_rtt_stats[host_1].observation_count);
  EXPECT_EQ(total_observaions_count,
            recent_rtt_stats[host_2].observation_count);

  // Checks all canonical percentile values are correct.
  // For |host_1|, percentile_val = percentile.
  EXPECT_EQ(1, recent_rtt_stats[host_1].canonical_pcts[kStatVal0p]);
  EXPECT_EQ(5, recent_rtt_stats[host_1].canonical_pcts[kStatVal5p]);
  EXPECT_EQ(50, recent_rtt_stats[host_1].canonical_pcts[kStatVal50p]);
  EXPECT_EQ(95, recent_rtt_stats[host_1].canonical_pcts[kStatVal95p]);
  EXPECT_EQ(99, recent_rtt_stats[host_1].canonical_pcts[kStatVal99p]);
  // For |host_2|, percentile_val = (percentile + 1) / 2.
  EXPECT_EQ(1, recent_rtt_stats[host_2].canonical_pcts[kStatVal0p]);
  EXPECT_EQ(3, recent_rtt_stats[host_2].canonical_pcts[kStatVal5p]);
  EXPECT_EQ(25, recent_rtt_stats[host_2].canonical_pcts[kStatVal50p]);
  EXPECT_EQ(48, recent_rtt_stats[host_2].canonical_pcts[kStatVal95p]);
  EXPECT_EQ(50, recent_rtt_stats[host_2].canonical_pcts[kStatVal99p]);

  // Checks results are cleared because all buffered observations expire.
  // Expects the result map is empty.
  recent_rtt_stats =
      mixed_buffer.GetCanonicalStatsKeyedByHosts(future, empty_hosts_set);

  EXPECT_TRUE(recent_rtt_stats.empty());

  // Checks results contain stats only for hosts that were in the set.
  std::set<uint64_t> target_hosts_set = {host_1};
  recent_rtt_stats =
      mixed_buffer.GetCanonicalStatsKeyedByHosts(history, target_hosts_set);
  EXPECT_EQ(1u, recent_rtt_stats.size());
  EXPECT_EQ(total_observaions_count,
            recent_rtt_stats[host_1].observation_count);
  EXPECT_EQ(1, recent_rtt_stats[host_1].canonical_pcts[kStatVal0p]);
  EXPECT_EQ(5, recent_rtt_stats[host_1].canonical_pcts[kStatVal5p]);
  EXPECT_EQ(50, recent_rtt_stats[host_1].canonical_pcts[kStatVal50p]);
  EXPECT_EQ(95, recent_rtt_stats[host_1].canonical_pcts[kStatVal95p]);
  EXPECT_EQ(99, recent_rtt_stats[host_1].canonical_pcts[kStatVal99p]);
  // Checks that host 2 does not present in the results.
  EXPECT_TRUE(recent_rtt_stats.find(host_2) == recent_rtt_stats.end());

  bool deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_MAX] = {
      false};
  deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_TCP] = true;
  mixed_buffer.RemoveObservationsWithSource(deleted_observation_sources);
  EXPECT_EQ(0u, mixed_buffer.Size());
}

// Verifies that the percentiles are correctly computed. All observations have
// the same timestamp.
TEST(NetworkQualityObservationBufferTest, PercentileSameTimestamps) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  ASSERT_EQ(0u, buffer.Size());
  ASSERT_LT(0u, buffer.Capacity());

  const base::TimeTicks now = tick_clock.NowTicks();

  size_t observations_count = 0;
  // Percentiles should be unavailable when no observations are available.
  EXPECT_FALSE(
      buffer
          .GetPercentile(base::TimeTicks(), INT32_MIN, 50,
                         &observations_count)
          .has_value());
  EXPECT_EQ(0u, observations_count);

  // Insert samples from {1,2,3,..., 100}. First insert odd samples, then even
  // samples. This helps in verifying that the order of samples does not matter.
  for (int i = 1; i <= 99; i += 2) {
    buffer.AddObservation(Observation(i, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
    EXPECT_TRUE(buffer.GetPercentile(base::TimeTicks(), INT32_MIN, 50, nullptr)
                    .has_value());
    ASSERT_EQ(static_cast<size_t>(i / 2 + 1), buffer.Size());
  }

  for (int i = 2; i <= 100; i += 2) {
    buffer.AddObservation(Observation(i, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
    EXPECT_TRUE(buffer.GetPercentile(base::TimeTicks(), INT32_MIN, 50, nullptr)
                    .has_value());
    ASSERT_EQ(static_cast<size_t>(i / 2 + 50), buffer.Size());
  }

  ASSERT_EQ(100u, buffer.Size());

  for (int i = 0; i <= 100; ++i) {
    // Checks if the difference between actual result and the computed result is
    // less than 1. This is required because computed percentiles may be
    // slightly different from what is expected due to floating point
    // computation errors and integer rounding off errors.
    base::Optional<int32_t> result = buffer.GetPercentile(
        base::TimeTicks(), INT32_MIN, i, &observations_count);
    EXPECT_EQ(100u, observations_count);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), i, 1.0);
  }

  EXPECT_FALSE(
      buffer
          .GetPercentile(now + base::TimeDelta::FromSeconds(1), INT32_MIN, 50,
                         &observations_count)
          .has_value());
  EXPECT_EQ(0u, observations_count);

  // Percentiles should be unavailable when no observations are available.
  buffer.Clear();
  EXPECT_FALSE(
      buffer
          .GetPercentile(base::TimeTicks(), INT32_MIN, 50,
                         &observations_count)
          .has_value());
  EXPECT_EQ(0u, observations_count);
}

// Verifies that the percentiles are correctly computed. Observations have
// different timestamps with half the observations being very old and the rest
// of them being very recent. Percentiles should factor in recent observations
// much more heavily than older samples.
TEST(NetworkQualityObservationBufferTest, PercentileDifferentTimestamps) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  const base::TimeTicks now = tick_clock.NowTicks();
  const base::TimeTicks very_old = now - base::TimeDelta::FromDays(7);

  size_t observations_count;

  // Network quality should be unavailable when no observations are available.
  EXPECT_FALSE(
      buffer
          .GetPercentile(base::TimeTicks(), INT32_MIN, 50,
                         &observations_count)
          .has_value());
  EXPECT_EQ(0u, observations_count);

  // First 50 samples have very old timestamps.
  for (int i = 1; i <= 50; ++i) {
    buffer.AddObservation(Observation(i, very_old, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
  }

  // Next 50 (i.e., from 51 to 100) have recent timestamps.
  for (int i = 51; i <= 100; ++i) {
    buffer.AddObservation(Observation(i, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
  }

  // Older samples have very little weight. So, all percentiles are >= 51
  // (lowest value among recent observations).
  for (int i = 1; i < 100; ++i) {
    // Checks if the difference between the two integers is less than 1. This is
    // required because computed percentiles may be slightly different from
    // what is expected due to floating point computation errors and integer
    // rounding off errors.
    base::Optional<int32_t> result =
        buffer.GetPercentile(very_old, INT32_MIN, i, &observations_count);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 51 + 0.49 * i, 1);
    EXPECT_EQ(100u, observations_count);
  }

  EXPECT_FALSE(buffer.GetPercentile(now + base::TimeDelta::FromSeconds(1),
                                    INT32_MIN, 50, &observations_count));
  EXPECT_EQ(0u, observations_count);
}

// Verifies that the percentiles are correctly computed. All observations have
// same timestamp with half the observations taken at low RSSI, and half the
// observations with high RSSI. Percentiles should be computed based on the
// current RSSI and the RSSI of the observations.
TEST(NetworkQualityObservationBufferTest, PercentileDifferentRSSI) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  ObservationBuffer buffer(&params, &tick_clock, 1.0, 0.25);
  const base::TimeTicks now = tick_clock.NowTicks();
  int32_t high_rssi = 4;
  int32_t low_rssi = 0;

  // Network quality should be unavailable when no observations are available.
  EXPECT_FALSE(buffer.GetPercentile(base::TimeTicks(), INT32_MIN, 50, nullptr)
                   .has_value());

  // First 50 samples have very low RSSI.
  for (int i = 1; i <= 50; ++i) {
    buffer.AddObservation(
        Observation(i, now, low_rssi, NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
  }

  // Next 50 (i.e., from 51 to 100) have high RSSI.
  for (int i = 51; i <= 100; ++i) {
    buffer.AddObservation(Observation(i, now, high_rssi,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
  }

  // When the current RSSI is |high_rssi|, higher weight should be assigned
  // to observations that were taken at |high_rssi|.
  for (int i = 1; i < 100; ++i) {
    base::Optional<int32_t> result =
        buffer.GetPercentile(now, high_rssi, i, nullptr);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 51 + 0.49 * i, 2);
  }

  // When the current RSSI is |low_rssi|, higher weight should be assigned
  // to observations that were taken at |low_rssi|.
  for (int i = 1; i < 100; ++i) {
    base::Optional<int32_t> result =
        buffer.GetPercentile(now, low_rssi, i, nullptr);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), i / 2, 2);
  }
}

// Verifies that the percentiles are correctly computed when some of the
// observation sources are disallowed. All observations have the same timestamp.
TEST(NetworkQualityObservationBufferTest, RemoveObservations) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));

  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  const base::TimeTicks now = tick_clock.NowTicks();

  // Insert samples from {1,2,3,..., 100}. First insert odd samples, then even
  // samples. This helps in verifying that the order of samples does not matter.
  for (int i = 1; i <= 99; i += 2) {
    buffer.AddObservation(Observation(i, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
  }
  EXPECT_EQ(50u, buffer.Size());

  // Add samples for TCP and QUIC observations which should not be taken into
  // account when computing the percentile.
  for (int i = 1; i <= 99; i += 2) {
    buffer.AddObservation(Observation(10000, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_TCP));
    buffer.AddObservation(Observation(10000, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC));
  }
  EXPECT_EQ(150u, buffer.Size());

  for (int i = 2; i <= 100; i += 2) {
    buffer.AddObservation(Observation(i, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
  }
  EXPECT_EQ(200u, buffer.Size());

  bool deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_MAX] = {
      false};

  // Since all entries in |deleted_observation_sources| are set to false, no
  // observations should be deleted.
  buffer.RemoveObservationsWithSource(deleted_observation_sources);
  EXPECT_EQ(200u, buffer.Size());

  // 50 TCP and 50 QUIC observations should be deleted.
  deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_TCP] = true;
  deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC] = true;
  buffer.RemoveObservationsWithSource(deleted_observation_sources);
  EXPECT_EQ(100u, buffer.Size());

  for (int i = 0; i <= 100; ++i) {
    // Checks if the difference between the two integers is less than 1. This is
    // required because computed percentiles may be slightly different from
    // what is expected due to floating point computation errors and integer
    // rounding off errors.
    base::Optional<int32_t> result =
        buffer.GetPercentile(base::TimeTicks(), INT32_MIN, i,
                             nullptr);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), i, 1);
  }

  deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP] = true;
  buffer.RemoveObservationsWithSource(deleted_observation_sources);
  EXPECT_EQ(0u, buffer.Size());
}

TEST(NetworkQualityObservationBufferTest, TestGetMedianRTTSince) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  base::TimeTicks now = tick_clock.NowTicks();
  base::TimeTicks old = now - base::TimeDelta::FromMilliseconds(1);
  ASSERT_NE(old, now);

  // First sample has very old timestamp.
  buffer.AddObservation(
      Observation(1, old, INT32_MIN, NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));

  buffer.AddObservation(Observation(100, now, INT32_MIN,
                                    NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));

  const struct {
    base::TimeTicks start_timestamp;
    bool expect_network_quality_available;
    base::TimeDelta expected_url_request_rtt;
  } tests[] = {
      {now + base::TimeDelta::FromSeconds(10), false,
       base::TimeDelta::FromMilliseconds(0)},
      {now, true, base::TimeDelta::FromMilliseconds(100)},
      {now - base::TimeDelta::FromMicroseconds(500), true,
       base::TimeDelta::FromMilliseconds(100)},

  };

  for (const auto& test : tests) {
    base::Optional<int32_t> url_request_rtt =
        buffer.GetPercentile(test.start_timestamp, INT32_MIN, 50, nullptr);
    EXPECT_EQ(test.expect_network_quality_available,
              url_request_rtt.has_value());

    if (test.expect_network_quality_available) {
      EXPECT_EQ(test.expected_url_request_rtt.InMillisecondsF(),
                url_request_rtt.value());
    }
  }
}


}  // namespace

}  // namespace internal

}  // namespace nqe

}  // namespace net
