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
#include "net/http/http_stream.h"
#include "net/http/http_stream_pool.h"
#include "net/log/net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"

namespace net {

namespace {

class FakeStreamSocket : public MockClientSocket {
 public:
  FakeStreamSocket() : MockClientSocket(NetLogWithSource()) {
    connected_ = true;
  }

  FakeStreamSocket(const FakeStreamSocket&) = delete;
  FakeStreamSocket& operator=(const FakeStreamSocket&) = delete;

  ~FakeStreamSocket() override = default;

  void set_is_connected(bool connected) { connected_ = connected; }

  void set_is_idle(bool is_idle) { is_idle_ = is_idle; }

  void set_was_ever_used(bool was_ever_used) { was_ever_used_ = was_ever_used; }

  // StreamSocket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    return 0;
  }
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    return 0;
  }
  int Connect(CompletionOnceCallback callback) override { return OK; }
  bool IsConnected() const override { return connected_; }
  bool IsConnectedAndIdle() const override { return connected_ && is_idle_; }
  bool WasEverUsed() const override { return was_ever_used_; }
  bool GetSSLInfo(SSLInfo* ssl_info) override { return false; }

 private:
  bool is_idle_ = true;
  bool was_ever_used_ = false;
};

}  // namespace

using Group = HttpStreamPool::Group;

class HttpStreamPoolGroupTest : public TestWithTaskEnvironment {
 public:
  HttpStreamPoolGroupTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        pool_(std::make_unique<HttpStreamPool>()) {}

 protected:
  HttpStreamPool& pool() { return *pool_; }

 private:
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

  FastForwardBy(Group::kUnusedIdleStreamSocketTimeout);
  group.CleanupIdleStreamSocketsForTesting();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
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

  static_assert(Group::kUnusedIdleStreamSocketTimeout <=
                Group::kUsedIdleStreamSocketTimeout);

  FastForwardBy(Group::kUnusedIdleStreamSocketTimeout);
  group.CleanupIdleStreamSocketsForTesting();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  FastForwardBy(Group::kUsedIdleStreamSocketTimeout);
  group.CleanupIdleStreamSocketsForTesting();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
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

  raw_stream_socket->set_is_connected(false);
  group.CleanupIdleStreamSocketsForTesting();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

}  // namespace net
