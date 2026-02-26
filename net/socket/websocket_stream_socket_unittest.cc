// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_stream_socket.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
namespace {

struct ConnectTimingAndResult {
  IoMode io_mode;
  Error connect_result;
};

constexpr ConnectTimingAndResult kConnectTimingAndResults[] = {
    {/*io_mode=*/SYNCHRONOUS, /*connect_result=*/OK},
    {/*io_mode=*/SYNCHRONOUS, /*connect_result=*/ERR_FAILED},
    {/*io_mode=*/ASYNC, /*connect_result=*/OK},
    {/*io_mode=*/ASYNC, /*connect_result=*/ERR_FAILED},
};

class WebSocketStreamSocketTest
    : public testing::TestWithParam<ConnectTimingAndResult>,
      public WithTaskEnvironment {
 public:
  WebSocketStreamSocketTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~WebSocketStreamSocketTest() override = default;

  std::unique_ptr<WebSocketStreamSocket> CreateSocket(
      MockConnect mock_connect) {
    auto data_provider = std::make_unique<SequencedSocketData>();
    data_provider->set_connect_data(mock_connect);
    // Create socket. Only the DataProvider matters.
    auto tcp_socket = std::make_unique<MockTCPClientSocket>(
        AddressList(kLocalhost), /*net_log=*/nullptr, data_provider.get());
    data_providers_.emplace_back(std::move(data_provider));
    return std::make_unique<WebSocketStreamSocket>(lock_manager_, kLocalhost,
                                                   std::move(tcp_socket));
  }

  IoMode io_mode() const { return GetParam().io_mode; }

  Error connect_result() const { return GetParam().connect_result; }

 protected:
  const IPEndPoint kLocalhost{IPAddress::IPv4Localhost(), 80};
  WebSocketEndpointLockManager lock_manager_;

  // Data providers for mock sockets. Mock sockets hold onto raw pointers to
  // these, so they have to be kept around and not moved.
  std::vector<std::unique_ptr<SequencedSocketData>> data_providers_;
};

INSTANTIATE_TEST_SUITE_P(,
                         WebSocketStreamSocketTest,
                         testing::ValuesIn(kConnectTimingAndResults));

TEST_P(WebSocketStreamSocketTest, OneSocket) {
  EXPECT_TRUE(lock_manager_.IsEmpty());

  base::test::TestFuture<int> callback;
  auto socket = CreateSocket(MockConnect(io_mode(), connect_result()));
  if (io_mode() == SYNCHRONOUS) {
    EXPECT_THAT(socket->Connect(callback.GetCallback()),
                test::IsError(connect_result()));
    EXPECT_FALSE(lock_manager_.IsEmpty());
    EXPECT_FALSE(callback.IsReady());
  } else {
    EXPECT_THAT(socket->Connect(callback.GetCallback()),
                test::IsError(ERR_IO_PENDING));
    // With one socket, even in the async case, the lock should be obtained
    // synchronously.
    EXPECT_FALSE(lock_manager_.IsEmpty());
    EXPECT_THAT(callback.Get(), test::IsError(connect_result()));
  }
}

TEST_P(WebSocketStreamSocketTest, TwoSockets) {
  EXPECT_TRUE(lock_manager_.IsEmpty());

  base::test::TestFuture<int> callback1;
  auto socket1 = CreateSocket(MockConnect(io_mode(), connect_result()));
  if (io_mode() == SYNCHRONOUS) {
    EXPECT_THAT(socket1->Connect(callback1.GetCallback()),
                test::IsError(connect_result()));
  } else {
    EXPECT_THAT(socket1->Connect(callback1.GetCallback()),
                test::IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback1.Get(), test::IsError(connect_result()));
  }
  EXPECT_FALSE(lock_manager_.IsEmpty());

  base::test::TestFuture<int> callback2;
  auto socket2 = CreateSocket(MockConnect(io_mode(), connect_result()));
  EXPECT_THAT(socket2->Connect(callback2.GetCallback()),
              test::IsError(ERR_IO_PENDING));

  // `socket1` should be preventing `socket2` from getting the lock. Advance
  // time to make there there's no unlock task queued that might free up the
  // lock. This value must be much longer than the unlock delay.
  FastForwardBy(base::Minutes(1));
  EXPECT_FALSE(callback2.IsReady());
  // Lock should still be held.
  EXPECT_FALSE(lock_manager_.IsEmpty());

  // Destroying the second socket should release the lock, albeit after some
  // delay.
  socket1.reset();
  EXPECT_THAT(callback2.Get(), test::IsError(connect_result()));
  // The lock should still be held.
  EXPECT_FALSE(lock_manager_.IsEmpty());
}

}  // namespace
}  // namespace net
