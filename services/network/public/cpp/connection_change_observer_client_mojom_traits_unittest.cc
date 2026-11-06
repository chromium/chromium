// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_change_observer_client_mojom_traits.h"

#include <cstdint>
#include <limits>
#include <optional>

#include "base/time/time.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/reconnect_notifier.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

bool CanRoundTrip(const net::ConnectionKeepAliveConfig& input) {
  net::ConnectionKeepAliveConfig output;
  const bool valid = input.IsValid();
  const bool round_tripped =
      mojo::test::SerializeAndDeserialize<mojom::ConnectionKeepAliveConfig>(
          input, output);
  EXPECT_EQ(round_tripped, valid);
  return round_tripped;
}

TEST(ConnectionChangeObserverClientMojomTraitsTest,
     DefaultConnectionKeepAliveConfigRoundTrip) {
  const net::ConnectionKeepAliveConfig original;
  net::ConnectionKeepAliveConfig deserialized;
  deserialized.idle_timeout = base::Seconds(60);
  deserialized.ping_interval = base::Seconds(15);
  deserialized.enable_connection_keep_alive = true;
  deserialized.initial_rtt = base::Milliseconds(25);
  deserialized.quic_connection_options = "DIRTY";

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ConnectionKeepAliveConfig>(
          original, deserialized));
  EXPECT_EQ(deserialized, original);
}

TEST(ConnectionChangeObserverClientMojomTraitsTest,
     PopulatedConnectionKeepAliveConfigRoundTrip) {
  net::ConnectionKeepAliveConfig original;
  original.idle_timeout = base::Seconds(60);
  original.ping_interval = base::Seconds(15);
  original.enable_connection_keep_alive = true;
  original.initial_rtt = base::Milliseconds(25);
  original.quic_connection_options = "ABCD,EFGH";

  net::ConnectionKeepAliveConfig deserialized;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ConnectionKeepAliveConfig>(
          original, deserialized));
  EXPECT_EQ(deserialized, original);
}

TEST(ConnectionChangeObserverClientMojomTraitsTest,
     IdleTimeoutWithDefaultPingIntervalRoundTrip) {
  // With default `ping_interval` (15s `kPingTimeoutSecs`) and client's 1s local
  // idle deduction, `idle_timeout` must exceed 16s.
  net::ConnectionKeepAliveConfig original;
  original.idle_timeout = base::Seconds(16) + base::Microseconds(1);

  net::ConnectionKeepAliveConfig deserialized;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ConnectionKeepAliveConfig>(
          original, deserialized));
  EXPECT_EQ(deserialized, original);
}

TEST(ConnectionChangeObserverClientMojomTraitsTest,
     InitialRttOnlyConfigRoundTrip) {
  net::ConnectionKeepAliveConfig original;
  original.initial_rtt = base::Microseconds(1);

  net::ConnectionKeepAliveConfig deserialized;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ConnectionKeepAliveConfig>(
          original, deserialized));
  EXPECT_EQ(deserialized, original);
}

TEST(ConnectionChangeObserverClientMojomTraitsTest,
     BoundaryExplicitTimeoutsRoundTrip) {
  net::ConnectionKeepAliveConfig min_timeouts;
  min_timeouts.ping_interval = base::Seconds(1);
  min_timeouts.idle_timeout = base::Seconds(2) + base::Microseconds(1);
  net::ConnectionKeepAliveConfig deserialized_min;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ConnectionKeepAliveConfig>(
          min_timeouts, deserialized_min));
  EXPECT_EQ(deserialized_min, min_timeouts);

  net::ConnectionKeepAliveConfig max_idle;
  max_idle.idle_timeout = base::Seconds(std::numeric_limits<uint32_t>::max());
  net::ConnectionKeepAliveConfig deserialized_max;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ConnectionKeepAliveConfig>(
          max_idle, deserialized_max));
  EXPECT_EQ(deserialized_max, max_idle);
}

TEST(ConnectionChangeObserverClientMojomTraitsTest,
     RejectsInvalidConnectionKeepAliveConfig) {
  // Invalid `initial_rtt` boundaries (`-1us`, `Min()`, `Max()`).
  net::ConnectionKeepAliveConfig negative_initial_rtt;
  negative_initial_rtt.initial_rtt = base::Microseconds(-1);
  EXPECT_FALSE(CanRoundTrip(negative_initial_rtt));

  net::ConnectionKeepAliveConfig max_initial_rtt;
  max_initial_rtt.initial_rtt = base::TimeDelta::Max();
  EXPECT_FALSE(CanRoundTrip(max_initial_rtt));

  net::ConnectionKeepAliveConfig min_initial_rtt;
  min_initial_rtt.initial_rtt = base::TimeDelta::Min();
  EXPECT_FALSE(CanRoundTrip(min_initial_rtt));

  // Invalid `ping_interval` boundaries (`-1us`, `Min()`, `Max()`, `1s - 1us`).
  net::ConnectionKeepAliveConfig negative_ping_interval;
  negative_ping_interval.idle_timeout = base::Seconds(30);
  negative_ping_interval.ping_interval = base::Microseconds(-1);
  EXPECT_FALSE(CanRoundTrip(negative_ping_interval));

  net::ConnectionKeepAliveConfig min_ping_interval;
  min_ping_interval.idle_timeout = base::Seconds(30);
  min_ping_interval.ping_interval = base::TimeDelta::Min();
  EXPECT_FALSE(CanRoundTrip(min_ping_interval));

  net::ConnectionKeepAliveConfig max_ping_interval;
  max_ping_interval.idle_timeout = base::Seconds(30);
  max_ping_interval.ping_interval = base::TimeDelta::Max();
  EXPECT_FALSE(CanRoundTrip(max_ping_interval));

  net::ConnectionKeepAliveConfig subsecond_ping_interval;
  subsecond_ping_interval.idle_timeout = base::Seconds(30);
  subsecond_ping_interval.ping_interval =
      base::Seconds(1) - base::Microseconds(1);
  EXPECT_FALSE(CanRoundTrip(subsecond_ping_interval));

  // Invalid `idle_timeout` boundaries (`-1us`, `Min()`, `Max()`, `< 2s`, `2s`,
  // `UINT32_MAX + 1s`).
  net::ConnectionKeepAliveConfig negative_idle_timeout;
  negative_idle_timeout.idle_timeout = base::Microseconds(-1);
  EXPECT_FALSE(CanRoundTrip(negative_idle_timeout));

  net::ConnectionKeepAliveConfig min_idle_timeout;
  min_idle_timeout.idle_timeout = base::TimeDelta::Min();
  EXPECT_FALSE(CanRoundTrip(min_idle_timeout));

  net::ConnectionKeepAliveConfig max_idle_timeout;
  max_idle_timeout.idle_timeout = base::TimeDelta::Max();
  EXPECT_FALSE(CanRoundTrip(max_idle_timeout));

  net::ConnectionKeepAliveConfig below_two_seconds_idle;
  below_two_seconds_idle.idle_timeout =
      base::Seconds(2) - base::Microseconds(1);
  below_two_seconds_idle.ping_interval = base::Seconds(1);
  EXPECT_FALSE(CanRoundTrip(below_two_seconds_idle));

  net::ConnectionKeepAliveConfig exact_two_seconds_idle;
  exact_two_seconds_idle.idle_timeout = base::Seconds(2);
  exact_two_seconds_idle.ping_interval = base::Seconds(1);
  EXPECT_FALSE(CanRoundTrip(exact_two_seconds_idle));

  net::ConnectionKeepAliveConfig overflow_idle_timeout;
  overflow_idle_timeout.idle_timeout =
      base::Seconds(int64_t{std::numeric_limits<uint32_t>::max()} + 1);
  EXPECT_FALSE(CanRoundTrip(overflow_idle_timeout));

  // `ping_interval` must be strictly less than `idle_timeout - 1s` (client's
  // local 1s deduction and equal-deadline idle-first alarm order).
  net::ConnectionKeepAliveConfig equal_local_deadline;
  equal_local_deadline.ping_interval = base::Seconds(15);
  equal_local_deadline.idle_timeout = base::Seconds(16);
  EXPECT_FALSE(CanRoundTrip(equal_local_deadline));

  // Non-zero `ping_interval` with unset (zero) `idle_timeout`.
  net::ConnectionKeepAliveConfig ping_without_idle;
  ping_without_idle.ping_interval = base::Seconds(15);
  EXPECT_FALSE(CanRoundTrip(ping_without_idle));

  // Default `ping_interval` (15s) equals `idle_timeout - 1s` at `16s`.
  net::ConnectionKeepAliveConfig idle_ties_default_ping;
  idle_ties_default_ping.idle_timeout = base::Seconds(16);
  EXPECT_FALSE(CanRoundTrip(idle_ties_default_ping));

  // Raw wire `QuicKeepAliveTimeouts` must enforce both canonical nullability
  // and `ConnectionKeepAliveConfig::IsValid()` bounds/coupling directly.
  QuicKeepAliveTimeouts out_timeouts;
  QuicKeepAliveTimeouts valid_wire{base::Seconds(30), base::Seconds(10)};
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::QuicKeepAliveTimeouts>(
      valid_wire, out_timeouts));
  EXPECT_EQ(out_timeouts.idle_timeout, base::Seconds(30));
  EXPECT_EQ(out_timeouts.ping_interval, base::Seconds(10));

  QuicKeepAliveTimeouts zero_idle_wire{base::TimeDelta(), std::nullopt};
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::QuicKeepAliveTimeouts>(
          zero_idle_wire, out_timeouts));

  QuicKeepAliveTimeouts non_null_zero_ping_wire{base::Seconds(30),
                                                base::TimeDelta()};
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::QuicKeepAliveTimeouts>(
          non_null_zero_ping_wire, out_timeouts));

  QuicKeepAliveTimeouts sub_two_second_idle_wire{base::Seconds(1),
                                                 std::nullopt};
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::QuicKeepAliveTimeouts>(
          sub_two_second_idle_wire, out_timeouts));

  QuicKeepAliveTimeouts sub_second_ping_wire{base::Seconds(30),
                                             base::Milliseconds(500)};
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::QuicKeepAliveTimeouts>(
          sub_second_ping_wire, out_timeouts));

  QuicKeepAliveTimeouts coupled_invalid_wire{base::Seconds(30),
                                             base::Seconds(29)};
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::QuicKeepAliveTimeouts>(
          coupled_invalid_wire, out_timeouts));

  // Null represents an unset `initial_rtt`; reject non-null zero on the wire.
  mojom::ConnectionKeepAliveConfigPtr non_null_zero_initial_rtt_wire =
      mojom::ConnectionKeepAliveConfig::New();
  non_null_zero_initial_rtt_wire->initial_rtt = base::TimeDelta();
  net::ConnectionKeepAliveConfig out_config;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::ConnectionKeepAliveConfig>(
          non_null_zero_initial_rtt_wire, out_config));
}

}  // namespace
}  // namespace network
