// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/nqe/observation_buffer.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/nqe/network_quality_observation.h"
#include "net/nqe/network_quality_observation_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::nqe::internal {

namespace {

// Verify that the buffer size is never exceeded.
TEST(NetworkQualityObservationBufferTest, BoundedBuffer) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::Minutes(1));
  ObservationBuffer observation_buffer(&params, &tick_clock, 1.0, 1.0);
  const base::TimeTicks now = base::TimeTicks() + base::Seconds(1);
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
  tick_clock.Advance(base::Minutes(1));

  ObservationBuffer observation_buffer(&params, &tick_clock, 0.98, 1.0);
  const base::TimeTicks now = tick_clock.NowTicks();
  for (int i = 1; i <= 100; ++i) {
    tick_clock.Advance(base::Seconds(1));
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
    std::optional<int32_t> result_i = observation_buffer.GetPercentile(
        now, INT32_MIN, i, &observations_count);
    EXPECT_EQ(100u, observations_count);
    ASSERT_TRUE(result_i.has_value());
    result_lowest = std::min(result_lowest, result_i.value());

    result_highest = std::max(result_highest, result_i.value());

    std::optional<int32_t> result_i_1 = observation_buffer.GetPercentile(
        now, INT32_MIN, i - 1, &observations_count);
    EXPECT_EQ(100u, observations_count);
    ASSERT_TRUE(result_i_1.has_value());

    EXPECT_LE(result_i_1.value(), result_i.value());
  }
  EXPECT_LT(result_lowest, result_highest);
}

// Verifies that the percentiles are correctly computed. All observations have
// the same timestamp.
TEST(NetworkQualityObservationBufferTest, PercentileSameTimestamps) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::Minutes(1));
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
    std::optional<int32_t> result = buffer.GetPercentile(
        base::TimeTicks(), INT32_MIN, i, &observations_count);
    EXPECT_EQ(100u, observations_count);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), i, 1.0);
  }

  EXPECT_FALSE(buffer
                   .GetPercentile(now + base::Seconds(1), INT32_MIN, 50,
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
  tick_clock.Advance(base::Minutes(1));
  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  const base::TimeTicks now = tick_clock.NowTicks();
  const base::TimeTicks very_old = now - base::Days(7);

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
    std::optional<int32_t> result =
        buffer.GetPercentile(very_old, INT32_MIN, i, &observations_count);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 51 + 0.49 * i, 1);
    EXPECT_EQ(100u, observations_count);
  }

  EXPECT_FALSE(buffer.GetPercentile(now + base::Seconds(1), INT32_MIN, 50,
                                    &observations_count));
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
  tick_clock.Advance(base::Minutes(1));
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
    std::optional<int32_t> result =
        buffer.GetPercentile(now, high_rssi, i, nullptr);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 51 + 0.49 * i, 2);
  }

  // When the current RSSI is |low_rssi|, higher weight should be assigned
  // to observations that were taken at |low_rssi|.
  for (int i = 1; i < 100; ++i) {
    std::optional<int32_t> result =
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
  tick_clock.Advance(base::Minutes(1));

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
    std::optional<int32_t> result =
        buffer.GetPercentile(base::TimeTicks(), INT32_MIN, i, nullptr);
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
  tick_clock.Advance(base::Minutes(1));
  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  base::TimeTicks now = tick_clock.NowTicks();
  base::TimeTicks old = now - base::Milliseconds(1);
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
      {now + base::Seconds(10), false, base::Milliseconds(0)},
      {now, true, base::Milliseconds(100)},
      {now - base::Microseconds(500), true, base::Milliseconds(100)},

  };

  for (const auto& test : tests) {
    std::optional<int32_t> url_request_rtt =
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

}  // namespace net::nqe::internal
