// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_group.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_change_notifier.h"
#include "net/base/privacy_mode.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_attempt_manager.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/log/net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "url/scheme_host_port.h"

namespace net {

using test::IsOk;

using Group = HttpStreamPool::Group;
using Job = HttpStreamPool::Job;

class HttpStreamPoolGroupTest : public TestWithTaskEnvironment {
 public:
  HttpStreamPoolGroupTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        default_test_key_(url::SchemeHostPort("http", "a.test", 80),
                          PRIVACY_MODE_DISABLED,
                          SocketTag(),
                          NetworkAnonymizationKey(),
                          SecureDnsPolicy::kAllow,
                          /*disable_cert_network_fetches=*/false) {
    feature_list_.InitAndEnableFeature(features::kHappyEyeballsV3);
    session_deps_.ignore_ip_address_changes = false;
    session_deps_.disable_idle_sockets_close_on_memory_pressure = false;
    InitializePool();
  }

 protected:
  void set_ignore_ip_address_changes(bool ignore_ip_address_changes) {
    session_deps_.ignore_ip_address_changes = ignore_ip_address_changes;
  }

  void set_disable_idle_sockets_close_on_memory_pressure(
      bool disable_idle_sockets_close_on_memory_pressure) {
    session_deps_.disable_idle_sockets_close_on_memory_pressure =
        disable_idle_sockets_close_on_memory_pressure;
  }

  void set_enable_quic(bool enable_quic) {
    session_deps_.enable_quic = enable_quic;
  }

  void InitializePool() {
    http_network_session_ =
        SpdySessionDependencies::SpdyCreateSession(&session_deps_);
  }

  Group& GetOrCreateTestGroup() {
    return pool().GetOrCreateGroupForTesting(default_test_key_);
  }

  Group* GetTestGroup() { return pool().GetGroupForTesting(default_test_key_); }

  HttpStreamPool& pool() { return *http_network_session_->http_stream_pool(); }

  void DestroyHttpNetworkSession() { http_network_session_.reset(); }

 private:
  base::MemoryPressureListenerRegistry memory_pressure_listener_registry_;
  base::test::ScopedFeatureList feature_list_;
  const HttpStreamKey default_test_key_;
  // For creating HttpNetworkSession.
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> http_network_session_;
};

TEST_F(HttpStreamPoolGroupTest, CreateTextBasedStream) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();

  Group& group = GetOrCreateTestGroup();
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::move(stream_socket), StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());
  CHECK(stream);
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);
}

TEST_F(HttpStreamPoolGroupTest, ReleaseStreamSocketUnused) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();

  Group& group = GetOrCreateTestGroup();
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::move(stream_socket), StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());
  CHECK(stream);

  stream.reset();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);

  FastForwardBy(Group::kUnusedIdleStreamSocketTimeout);
  group.CleanupTimedoutIdleStreamSocketsForTesting();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 0u);
}

// Regression test for crbug.com/399995424. If a socket returned to a Group
// immediately closes, the Group should destroy itself without accessing deleted
// `this`.
TEST_F(HttpStreamPoolGroupTest,
       ReleaseStreamSocketDisconnectAfterIsConnectedCall) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();
  stream_socket->set_was_ever_used(true);
  stream_socket->set_is_idle(true);
  stream_socket->set_is_connected(true);
  FakeStreamSocket* stream_socket_ptr = stream_socket.get();

  Group& group = GetOrCreateTestGroup();
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::move(stream_socket),
      StreamSocketHandle::SocketReuseType::kReusedIdle,
      LoadTimingInfo::ConnectTiming());
  CHECK(stream);

  stream_socket_ptr->DisconnectAfterIsConnectedCall();
  stream.reset();
  ASSERT_FALSE(GetTestGroup());
}

TEST_F(HttpStreamPoolGroupTest, ReleaseStreamSocketUsed) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();
  stream_socket->set_was_ever_used(true);

  Group& group = GetOrCreateTestGroup();
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::move(stream_socket), StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());
  CHECK(stream);

  stream.reset();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);

  static_assert(Group::kUnusedIdleStreamSocketTimeout <=
                Group::kUsedIdleStreamSocketTimeout);

  FastForwardBy(Group::kUnusedIdleStreamSocketTimeout);
  group.CleanupTimedoutIdleStreamSocketsForTesting();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);

  FastForwardBy(Group::kUsedIdleStreamSocketTimeout);
  group.CleanupTimedoutIdleStreamSocketsForTesting();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 0u);
}

TEST_F(HttpStreamPoolGroupTest, ReleaseStreamSocketNotIdle) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();
  stream_socket->set_is_idle(false);

  Group& group = GetOrCreateTestGroup();
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::move(stream_socket), StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());
  CHECK(stream);

  stream.reset();

  ASSERT_FALSE(GetTestGroup());
}

TEST_F(HttpStreamPoolGroupTest, IdleSocketDisconnected) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();
  FakeStreamSocket* raw_stream_socket = stream_socket.get();

  Group& group = GetOrCreateTestGroup();
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::move(stream_socket), StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());
  CHECK(stream);

  stream.reset();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);

  raw_stream_socket->set_is_connected(false);
  group.CleanupTimedoutIdleStreamSocketsForTesting();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolGroupTest, IdleSocketReceivedDataUnexpectedly) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();
  FakeStreamSocket* raw_stream_socket = stream_socket.get();

  Group& group = GetOrCreateTestGroup();
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::move(stream_socket), StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());
  CHECK(stream);

  stream.reset();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);

  // Simulate the socket was used and not idle (received data).
  raw_stream_socket->set_was_ever_used(true);
  raw_stream_socket->set_is_idle(false);

  group.CleanupTimedoutIdleStreamSocketsForTesting();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolGroupTest, GetIdleStreamSocket) {
  Group& group = GetOrCreateTestGroup();
  ASSERT_FALSE(group.GetIdleStreamSocket());

  auto stream_socket = std::make_unique<FakeStreamSocket>();
  group.AddIdleStreamSocket(std::move(stream_socket));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  std::unique_ptr<StreamSocket> socket = group.GetIdleStreamSocket();
  ASSERT_TRUE(socket);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolGroupTest, GetIdleStreamSocketPreferUsed) {
  Group& group = GetOrCreateTestGroup();

  // Add 3 idle streams. the first and the third ones are marked as used.
  auto stream_socket1 = std::make_unique<FakeStreamSocket>();
  auto stream_socket2 = std::make_unique<FakeStreamSocket>();
  auto stream_socket3 = std::make_unique<FakeStreamSocket>();

  stream_socket1->set_was_ever_used(true);
  stream_socket3->set_was_ever_used(true);

  stream_socket1->set_peer_addr(IPEndPoint(IPAddress(192, 0, 2, 1), 80));
  stream_socket2->set_peer_addr(IPEndPoint(IPAddress(192, 0, 2, 2), 80));
  stream_socket3->set_peer_addr(IPEndPoint(IPAddress(192, 0, 2, 3), 80));

  group.AddIdleStreamSocket(std::move(stream_socket1));
  group.AddIdleStreamSocket(std::move(stream_socket2));
  group.AddIdleStreamSocket(std::move(stream_socket3));
  ASSERT_EQ(group.IdleStreamSocketCount(), 3u);

  std::unique_ptr<StreamSocket> socket = group.GetIdleStreamSocket();
  ASSERT_TRUE(socket);
  ASSERT_EQ(group.IdleStreamSocketCount(), 2u);

  IPEndPoint peer;
  int rv = socket->GetPeerAddress(&peer);
  EXPECT_THAT(rv, IsOk());
  EXPECT_THAT(peer, IPEndPoint(IPAddress(192, 0, 2, 3), 80));
}

TEST_F(HttpStreamPoolGroupTest, GetIdleStreamSocketDisconnectedDuringIdle) {
  Group& group = GetOrCreateTestGroup();
  ASSERT_FALSE(group.GetIdleStreamSocket());

  auto stream_socket = std::make_unique<FakeStreamSocket>();
  FakeStreamSocket* raw_stream_socket = stream_socket.get();
  group.AddIdleStreamSocket(std::move(stream_socket));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  raw_stream_socket->set_is_connected(false);
  ASSERT_FALSE(group.GetIdleStreamSocket());
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolGroupTest, GetIdleStreamSocketUsedSocketDisconnected) {
  Group& group = GetOrCreateTestGroup();
  ASSERT_FALSE(group.GetIdleStreamSocket());

  auto stream_socket = std::make_unique<FakeStreamSocket>();
  FakeStreamSocket* raw_stream_socket = stream_socket.get();
  group.AddIdleStreamSocket(std::move(stream_socket));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  raw_stream_socket->set_was_ever_used(true);
  raw_stream_socket->set_is_connected(false);
  ASSERT_FALSE(group.GetIdleStreamSocket());
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolGroupTest, GetIdleStreamSocketTimedout) {
  Group& group = GetOrCreateTestGroup();

  auto stream_socket = std::make_unique<FakeStreamSocket>();
  group.AddIdleStreamSocket(std::move(stream_socket));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  FastForwardBy(HttpStreamPool::Group::kUnusedIdleStreamSocketTimeout);

  ASSERT_FALSE(group.GetIdleStreamSocket());
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

// Test that a group is destroyed when closing an idle stream that is the last
// stream in the group.
TEST_F(HttpStreamPoolGroupTest, DestroyGroupAfterCloseOneIdleStream) {
  Group& group = GetOrCreateTestGroup();

  auto stream_socket = std::make_unique<FakeStreamSocket>();
  group.AddIdleStreamSocket(std::move(stream_socket));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  ASSERT_TRUE(group.CloseOneIdleStreamSocket());
  FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(GetTestGroup());
}

TEST_F(HttpStreamPoolGroupTest, IPAddressChangeCleanupIdleSocket) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();

  Group& group = GetOrCreateTestGroup();
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::move(stream_socket), StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());
  CHECK(stream);

  stream.reset();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);

  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(GetTestGroup());
}

TEST_F(HttpStreamPoolGroupTest, IPAddressChangeReleaseStreamSocket) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();

  Group& group = GetOrCreateTestGroup();
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::move(stream_socket), StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());
  CHECK(stream);

  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);

  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  RunUntilIdle();

  stream.reset();

  ASSERT_FALSE(GetTestGroup());
}

TEST_F(HttpStreamPoolGroupTest, IPAddressChangeIgnored) {
  set_ignore_ip_address_changes(true);
  InitializePool();

  auto stream_socket = std::make_unique<FakeStreamSocket>();
  Group& group = GetOrCreateTestGroup();
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::move(stream_socket), StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());
  CHECK(stream);

  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);

  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  RunUntilIdle();

  stream.reset();

  group.CleanupTimedoutIdleStreamSocketsForTesting();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);
}

TEST_F(HttpStreamPoolGroupTest, FlushIdleStreamsOnMemoryPressure) {
  set_disable_idle_sockets_close_on_memory_pressure(false);
  InitializePool();

  {
    Group& group = GetOrCreateTestGroup();
    ASSERT_FALSE(group.GetIdleStreamSocket());

    group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
    ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

    // Idle sockets should be flushed on moderate memory pressure and `group`
    // should be destroyed.
    base::MemoryPressureListener::SimulatePressureNotificationAsync(
        base::MEMORY_PRESSURE_LEVEL_MODERATE, QuitClosure());
    RunUntilQuit();

    ASSERT_EQ(group.IdleStreamSocketCount(), 0u);

    // Wait for group to be deleted.
    FastForwardUntilNoTasksRemain();
    EXPECT_FALSE(GetTestGroup());
  }

  {
    Group& group = GetOrCreateTestGroup();
    group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
    ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

    // Idle sockets should be flushed on critical memory pressure and `group`
    // should be destroyed.
    base::MemoryPressureListener::SimulatePressureNotificationAsync(
        base::MEMORY_PRESSURE_LEVEL_CRITICAL, QuitClosure());
    RunUntilQuit();

    ASSERT_EQ(group.IdleStreamSocketCount(), 0u);

    // Wait for group to be deleted.
    FastForwardUntilNoTasksRemain();
    EXPECT_FALSE(GetTestGroup());
  }
}

TEST_F(HttpStreamPoolGroupTest, MemoryPressureDisabled) {
  set_disable_idle_sockets_close_on_memory_pressure(true);
  InitializePool();

  Group& group = GetOrCreateTestGroup();
  ASSERT_FALSE(group.GetIdleStreamSocket());

  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  // Idle sockets should be not flushed on moderate memory pressure.
  base::MemoryPressureListener::SimulatePressureNotificationAsync(
      base::MEMORY_PRESSURE_LEVEL_MODERATE, QuitClosure());
  RunUntilQuit();

  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  // Idle sockets should be not flushed on critical memory pressure.
  base::MemoryPressureListener::SimulatePressureNotificationAsync(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL, QuitClosure());
  RunUntilQuit();

  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolGroupTest, DestroySessionWhileStreamAlive) {
  std::unique_ptr<HttpStream> stream =
      GetOrCreateTestGroup().CreateTextBasedStream(
          std::make_unique<FakeStreamSocket>(),
          StreamSocketHandle::SocketReuseType::kUnused,
          LoadTimingInfo::ConnectTiming());
  CHECK(stream);

  // Destroy the session. This should not cause a crash.
  DestroyHttpNetworkSession();
}

TEST_F(HttpStreamPoolGroupTest, EnableDisableQuic) {
  const url::SchemeHostPort kHost("https", "www.example.com", 443);

  set_enable_quic(true);
  InitializePool();
  ASSERT_TRUE(pool().CanUseQuic(kHost, NetworkAnonymizationKey(),
                                /*enable_alternative_services=*/true));

  set_enable_quic(false);
  InitializePool();
  ASSERT_FALSE(pool().CanUseQuic(kHost, NetworkAnonymizationKey(),
                                 /*enable_alternative_services=*/true));
}

}  // namespace net
