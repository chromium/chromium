// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/socket_watcher.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace nqe {

namespace internal {

namespace {

class NetworkQualitySocketWatcherTest : public TestWithTaskEnvironment {
 protected:
  NetworkQualitySocketWatcherTest() { ResetExpectedCallbackParams(); }
  ~NetworkQualitySocketWatcherTest() override { ResetExpectedCallbackParams(); }

  static void OnUpdatedRTTAvailableStoreParams(
      SocketPerformanceWatcherFactory::Protocol protocol,
      const base::TimeDelta& rtt,
      const base::Optional<IPHash>& host) {
    // Need to verify before another callback is executed, or explicitly call
    // |ResetCallbackParams()|.
    ASSERT_FALSE(callback_executed_);
    callback_rtt_ = rtt;
    callback_host_ = host;
    callback_executed_ = true;
  }

  static void OnUpdatedRTTAvailable(
      SocketPerformanceWatcherFactory::Protocol protocol,
      const base::TimeDelta& rtt,
      const base::Optional<IPHash>& host) {
    // Need to verify before another callback is executed, or explicitly call
    // |ResetCallbackParams()|.
    ASSERT_FALSE(callback_executed_);
    callback_executed_ = true;
  }

  static void SetShouldNotifyRTTCallback(bool value) {
    should_notify_rtt_callback_ = value;
  }

  static bool ShouldNotifyRTTCallback(base::TimeTicks now) {
    return should_notify_rtt_callback_;
  }

  static void VerifyCallbackParams(const base::TimeDelta& rtt,
                                   const base::Optional<IPHash>& host) {
    ASSERT_TRUE(callback_executed_);
    EXPECT_EQ(rtt, callback_rtt_);
    if (host)
      EXPECT_EQ(host, callback_host_);
    else
      EXPECT_FALSE(callback_host_.has_value());
    ResetExpectedCallbackParams();
  }

  static void ResetExpectedCallbackParams() {
    callback_rtt_ = base::TimeDelta::FromMilliseconds(0);
    callback_host_ = base::nullopt;
    callback_executed_ = false;
    should_notify_rtt_callback_ = false;
  }

  static base::TimeDelta callback_rtt() { return callback_rtt_; }

 private:
  static base::TimeDelta callback_rtt_;
  static base::Optional<IPHash> callback_host_;
  static bool callback_executed_;
  static bool should_notify_rtt_callback_;

  DISALLOW_COPY_AND_ASSIGN(NetworkQualitySocketWatcherTest);
};

base::TimeDelta NetworkQualitySocketWatcherTest::callback_rtt_ =
    base::TimeDelta::FromMilliseconds(0);

base::Optional<IPHash> NetworkQualitySocketWatcherTest::callback_host_ =
    base::nullopt;

bool NetworkQualitySocketWatcherTest::callback_executed_ = false;

bool NetworkQualitySocketWatcherTest::should_notify_rtt_callback_ = false;

// Verify that the buffer size is never exceeded.
TEST_F(NetworkQualitySocketWatcherTest, NotificationsThrottled) {
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());

  // Use a public IP address so that the socket watcher runs the RTT callback.
  IPAddressList ip_list;
  IPAddress ip_address;
  ASSERT_TRUE(ip_address.AssignFromIPLiteral("157.0.0.1"));
  ip_list.push_back(ip_address);
  AddressList address_list =
      AddressList::CreateFromIPAddressList(ip_list, "canonical.example.com");

  SocketWatcher socket_watcher(
      SocketPerformanceWatcherFactory::PROTOCOL_TCP, address_list,
      base::TimeDelta::FromMilliseconds(2000), false,
      base::ThreadTaskRunnerHandle::Get(), base::Bind(OnUpdatedRTTAvailable),
      base::Bind(ShouldNotifyRTTCallback), &tick_clock);

  EXPECT_TRUE(socket_watcher.ShouldNotifyUpdatedRTT());
  socket_watcher.OnUpdatedRTTAvailable(base::TimeDelta::FromSeconds(10));
  base::RunLoop().RunUntilIdle();
  ResetExpectedCallbackParams();

  EXPECT_FALSE(socket_watcher.ShouldNotifyUpdatedRTT());

  tick_clock.Advance(base::TimeDelta::FromMilliseconds(1000));
  // Minimum interval between consecutive notifications is 2000 msec.
  EXPECT_FALSE(socket_watcher.ShouldNotifyUpdatedRTT());

  // Advance the clock by 1000 msec more so that the current time is at least
  // 2000 msec more than the last time |socket_watcher| received a notification.
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(socket_watcher.ShouldNotifyUpdatedRTT());
  ResetExpectedCallbackParams();
  socket_watcher.OnUpdatedRTTAvailable(base::TimeDelta::FromSeconds(10));

  EXPECT_FALSE(socket_watcher.ShouldNotifyUpdatedRTT());

  // RTT notification is allowed by the global check.
  SetShouldNotifyRTTCallback(true);
  EXPECT_TRUE(socket_watcher.ShouldNotifyUpdatedRTT());
}

TEST_F(NetworkQualitySocketWatcherTest, QuicFirstNotificationDropped) {
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());

  // Use a public IP address so that the socket watcher runs the RTT callback.
  IPAddressList ip_list;
  IPAddress ip_address;
  ASSERT_TRUE(ip_address.AssignFromIPLiteral("157.0.0.1"));
  ip_list.push_back(ip_address);
  AddressList address_list =
      AddressList::CreateFromIPAddressList(ip_list, "canonical.example.com");

  SocketWatcher socket_watcher(
      SocketPerformanceWatcherFactory::PROTOCOL_QUIC, address_list,
      base::TimeDelta::FromMilliseconds(2000), false,
      base::ThreadTaskRunnerHandle::Get(),
      base::Bind(OnUpdatedRTTAvailableStoreParams),
      base::Bind(ShouldNotifyRTTCallback), &tick_clock);

  EXPECT_TRUE(socket_watcher.ShouldNotifyUpdatedRTT());
  socket_watcher.OnUpdatedRTTAvailable(base::TimeDelta::FromSeconds(10));
  base::RunLoop().RunUntilIdle();
  // First notification from a QUIC connection should be dropped, and it should
  // be possible to notify the |socket_watcher| again.
  EXPECT_TRUE(NetworkQualitySocketWatcherTest::callback_rtt().is_zero());
  EXPECT_TRUE(socket_watcher.ShouldNotifyUpdatedRTT());
  ResetExpectedCallbackParams();

  socket_watcher.OnUpdatedRTTAvailable(base::TimeDelta::FromSeconds(2));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::TimeDelta::FromSeconds(2),
            NetworkQualitySocketWatcherTest::callback_rtt());
  ResetExpectedCallbackParams();

  EXPECT_FALSE(socket_watcher.ShouldNotifyUpdatedRTT());

  tick_clock.Advance(base::TimeDelta::FromMilliseconds(1000));
  // Minimum interval between consecutive notifications is 2000 msec.
  EXPECT_FALSE(socket_watcher.ShouldNotifyUpdatedRTT());

  // Advance the clock by 1000 msec more so that the current time is at least
  // 2000 msec more than the last time |socket_watcher| received a notification.
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(socket_watcher.ShouldNotifyUpdatedRTT());
}

TEST_F(NetworkQualitySocketWatcherTest, PrivateAddressRTTNotNotified) {
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());

  const struct {
    std::string ip_address;
    bool expect_should_notify_rtt;
  } tests[] = {
      {"157.0.0.1", true},    {"127.0.0.1", false},
      {"192.168.0.1", false}, {"::1", false},
      {"0.0.0.0", false},     {"2607:f8b0:4006:819::200e", true},
  };

  for (const auto& test : tests) {
    IPAddressList ip_list;
    IPAddress ip_address;
    ASSERT_TRUE(ip_address.AssignFromIPLiteral(test.ip_address));
    ip_list.push_back(ip_address);
    AddressList address_list =
        AddressList::CreateFromIPAddressList(ip_list, "canonical.example.com");

    SocketWatcher socket_watcher(
        SocketPerformanceWatcherFactory::PROTOCOL_TCP, address_list,
        base::TimeDelta::FromMilliseconds(2000), false,
        base::ThreadTaskRunnerHandle::Get(), base::Bind(OnUpdatedRTTAvailable),
        base::Bind(ShouldNotifyRTTCallback), &tick_clock);

    EXPECT_EQ(test.expect_should_notify_rtt,
              socket_watcher.ShouldNotifyUpdatedRTT());
    socket_watcher.OnUpdatedRTTAvailable(base::TimeDelta::FromSeconds(10));
    base::RunLoop().RunUntilIdle();
    ResetExpectedCallbackParams();

    EXPECT_FALSE(socket_watcher.ShouldNotifyUpdatedRTT());
  }
}

TEST_F(NetworkQualitySocketWatcherTest, RemoteHostIPHashComputedCorrectly) {
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  const struct {
    std::string ip_address;
    uint64_t host;
  } tests[] = {
      {"112.112.112.100", 0x0000000070707064UL},  // IPv4.
      {"112.112.112.250", 0x00000000707070faUL},
      {"2001:0db8:85a3:0000:0000:8a2e:0370:7334",
       0x20010db885a30000UL},                                 // IPv6.
      {"2001:db8:85a3::8a2e:370:7334", 0x20010db885a30000UL}  // Shortened IPv6.
  };

  for (const auto& test : tests) {
    IPAddressList ip_list;
    IPAddress ip_address;
    ASSERT_TRUE(ip_address.AssignFromIPLiteral(test.ip_address));
    ip_list.push_back(ip_address);
    AddressList address_list =
        AddressList::CreateFromIPAddressList(ip_list, "canonical.example.com");

    SocketWatcher socket_watcher(
        SocketPerformanceWatcherFactory::PROTOCOL_TCP, address_list,
        base::TimeDelta::FromMilliseconds(2000), false,
        base::ThreadTaskRunnerHandle::Get(),
        base::Bind(OnUpdatedRTTAvailableStoreParams),
        base::Bind(ShouldNotifyRTTCallback), &tick_clock);
    EXPECT_TRUE(socket_watcher.ShouldNotifyUpdatedRTT());
    socket_watcher.OnUpdatedRTTAvailable(base::TimeDelta::FromSeconds(10));
    base::RunLoop().RunUntilIdle();
    VerifyCallbackParams(base::TimeDelta::FromSeconds(10), test.host);
    EXPECT_FALSE(socket_watcher.ShouldNotifyUpdatedRTT());
  }
}

}  // namespace

}  // namespace internal

}  // namespace nqe

}  // namespace net
