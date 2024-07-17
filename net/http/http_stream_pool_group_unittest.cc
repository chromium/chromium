// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_group.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_change_notifier.h"
#include "net/base/privacy_mode.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/log/net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"

namespace net {

using test::IsOk;

using Group = HttpStreamPool::Group;

class HttpStreamPoolGroupTest : public TestWithTaskEnvironment {
 public:
  HttpStreamPoolGroupTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    http_network_session_ =
        SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    InitializePool();
  }

 protected:
  void InitializePool(bool cleanup_on_ip_address_change = true) {
    pool_ = std::make_unique<HttpStreamPool>(http_network_session_.get(),
                                             cleanup_on_ip_address_change);
  }

  HttpStreamPool& pool() { return *pool_; }

 private:
  // For creating HttpNetworkSession.
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> http_network_session_;
  std::unique_ptr<HttpStreamPool> pool_;
};

TEST_F(HttpStreamPoolGroupTest, CreateTextBasedStream) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();

  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());
  std::unique_ptr<HttpStream> stream =
      group.CreateTextBasedStream(std::move(stream_socket));
  CHECK(stream);
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);
}

TEST_F(HttpStreamPoolGroupTest, ReleaseStreamSocketUnused) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();

  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());
  std::unique_ptr<HttpStream> stream =
      group.CreateTextBasedStream(std::move(stream_socket));
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

TEST_F(HttpStreamPoolGroupTest, ReleaseStreamSocketUsed) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();
  stream_socket->set_was_ever_used(true);

  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());
  std::unique_ptr<HttpStream> stream =
      group.CreateTextBasedStream(std::move(stream_socket));
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

  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());
  std::unique_ptr<HttpStream> stream =
      group.CreateTextBasedStream(std::move(stream_socket));
  CHECK(stream);

  stream.reset();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 0u);
}

TEST_F(HttpStreamPoolGroupTest, IdleSocketDisconnected) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();
  FakeStreamSocket* raw_stream_socket = stream_socket.get();

  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());
  std::unique_ptr<HttpStream> stream =
      group.CreateTextBasedStream(std::move(stream_socket));
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

  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());
  std::unique_ptr<HttpStream> stream =
      group.CreateTextBasedStream(std::move(stream_socket));
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
  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());
  ASSERT_FALSE(group.GetIdleStreamSocket());

  auto stream_socket = std::make_unique<FakeStreamSocket>();
  group.AddIdleStreamSocket(std::move(stream_socket));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  std::unique_ptr<StreamSocket> socket = group.GetIdleStreamSocket();
  ASSERT_TRUE(socket);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolGroupTest, GetIdleStreamSocketPreferUsed) {
  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());

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
  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());
  ASSERT_FALSE(group.GetIdleStreamSocket());

  auto stream_socket = std::make_unique<FakeStreamSocket>();
  FakeStreamSocket* raw_stream_socket = stream_socket.get();
  group.AddIdleStreamSocket(std::move(stream_socket));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  raw_stream_socket->set_is_connected(false);
  ASSERT_FALSE(group.GetIdleStreamSocket());
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolGroupTest, GetIdleStreamSocketTimedout) {
  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());

  auto stream_socket = std::make_unique<FakeStreamSocket>();
  group.AddIdleStreamSocket(std::move(stream_socket));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  FastForwardBy(HttpStreamPool::Group::kUnusedIdleStreamSocketTimeout);

  ASSERT_FALSE(group.GetIdleStreamSocket());
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolGroupTest, IPAddressChangeCleanupIdleSocket) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();

  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());
  std::unique_ptr<HttpStream> stream =
      group.CreateTextBasedStream(std::move(stream_socket));
  CHECK(stream);

  stream.reset();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);

  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  RunUntilIdle();

  group.CleanupTimedoutIdleStreamSocketsForTesting();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolGroupTest, IPAddressChangeReleaseStreamSocket) {
  auto stream_socket = std::make_unique<FakeStreamSocket>();

  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());
  std::unique_ptr<HttpStream> stream =
      group.CreateTextBasedStream(std::move(stream_socket));
  CHECK(stream);

  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);

  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  RunUntilIdle();

  stream.reset();

  group.CleanupTimedoutIdleStreamSocketsForTesting();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(pool().TotalActiveStreamCount(), 0u);
}

TEST_F(HttpStreamPoolGroupTest, IPAddressChangeIgnored) {
  InitializePool(/*cleanup_on_ip_address_change=*/false);

  auto stream_socket = std::make_unique<FakeStreamSocket>();
  Group& group = pool().GetOrCreateGroupForTesting(HttpStreamKey());
  std::unique_ptr<HttpStream> stream =
      group.CreateTextBasedStream(std::move(stream_socket));
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

}  // namespace net
