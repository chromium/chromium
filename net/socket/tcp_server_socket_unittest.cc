// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_server_socket.h"

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_client_socket.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {
const int kListenBacklog = 5;

class TCPServerSocketTest : public PlatformTest, public WithTaskEnvironment {
 protected:
  TCPServerSocketTest() : socket_(nullptr, NetLogSource()) {}

  void SetUpIPv4() {
    IPEndPoint address(IPAddress::IPv4Localhost(), 0);
    ASSERT_THAT(socket_.Listen(address, kListenBacklog), IsOk());
    ASSERT_THAT(socket_.GetLocalAddress(&local_address_), IsOk());
  }

  void SetUpIPv6(bool* success) {
    *success = false;
    IPEndPoint address(IPAddress::IPv6Localhost(), 0);
    if (socket_.Listen(address, kListenBacklog) != 0) {
      LOG(ERROR) << "Failed to listen on ::1 - probably because IPv6 is "
          "disabled. Skipping the test";
      return;
    }
    ASSERT_THAT(socket_.GetLocalAddress(&local_address_), IsOk());
    *success = true;
  }

  static IPEndPoint GetPeerAddress(StreamSocket* socket) {
    IPEndPoint address;
    EXPECT_THAT(socket->GetPeerAddress(&address), IsOk());
    return address;
  }

  AddressList local_address_list() const {
    return AddressList(local_address_);
  }

  TCPServerSocket socket_;
  IPEndPoint local_address_;
};

TEST_F(TCPServerSocketTest, Accept) {
  ASSERT_NO_FATAL_FAILURE(SetUpIPv4());

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<StreamSocket> accepted_socket;
  int result = socket_.Accept(&accepted_socket, accept_callback.callback());
  result = accept_callback.GetResult(result);
  ASSERT_THAT(result, IsOk());

  ASSERT_TRUE(accepted_socket.get() != nullptr);

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(GetPeerAddress(accepted_socket.get()).address(),
            local_address_.address());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
}

// Test Accept() callback.
TEST_F(TCPServerSocketTest, AcceptAsync) {
  ASSERT_NO_FATAL_FAILURE(SetUpIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<StreamSocket> accepted_socket;

  ASSERT_THAT(socket_.Accept(&accepted_socket, accept_callback.callback()),
              IsError(ERR_IO_PENDING));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());
  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());

  EXPECT_TRUE(accepted_socket != nullptr);

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(GetPeerAddress(accepted_socket.get()).address(),
            local_address_.address());
}

// Accept two connections simultaneously.
TEST_F(TCPServerSocketTest, Accept2Connections) {
  ASSERT_NO_FATAL_FAILURE(SetUpIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<StreamSocket> accepted_socket;

  ASSERT_EQ(ERR_IO_PENDING,
            socket_.Accept(&accepted_socket, accept_callback.callback()));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback connect_callback2;
  TCPClientSocket connecting_socket2(local_address_list(), nullptr, nullptr,
                                     NetLogSource());
  int connect_result2 =
      connecting_socket2.Connect(connect_callback2.callback());

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());

  TestCompletionCallback accept_callback2;
  std::unique_ptr<StreamSocket> accepted_socket2;
  int result = socket_.Accept(&accepted_socket2, accept_callback2.callback());
  result = accept_callback2.GetResult(result);
  ASSERT_THAT(result, IsOk());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
  EXPECT_THAT(connect_callback2.GetResult(connect_result2), IsOk());

  EXPECT_TRUE(accepted_socket != nullptr);
  EXPECT_TRUE(accepted_socket2 != nullptr);
  EXPECT_NE(accepted_socket.get(), accepted_socket2.get());

  EXPECT_EQ(GetPeerAddress(accepted_socket.get()).address(),
            local_address_.address());
  EXPECT_EQ(GetPeerAddress(accepted_socket2.get()).address(),
            local_address_.address());
}

TEST_F(TCPServerSocketTest, AcceptIPv6) {
  bool initialized = false;
  ASSERT_NO_FATAL_FAILURE(SetUpIPv6(&initialized));
  if (!initialized)
    return;

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<StreamSocket> accepted_socket;
  int result = socket_.Accept(&accepted_socket, accept_callback.callback());
  result = accept_callback.GetResult(result);
  ASSERT_THAT(result, IsOk());

  ASSERT_TRUE(accepted_socket.get() != nullptr);

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(GetPeerAddress(accepted_socket.get()).address(),
            local_address_.address());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
}

TEST_F(TCPServerSocketTest, AcceptIO) {
  ASSERT_NO_FATAL_FAILURE(SetUpIPv4());

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<StreamSocket> accepted_socket;
  int result = socket_.Accept(&accepted_socket, accept_callback.callback());
  ASSERT_THAT(accept_callback.GetResult(result), IsOk());

  ASSERT_TRUE(accepted_socket.get() != nullptr);

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(GetPeerAddress(accepted_socket.get()).address(),
            local_address_.address());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

  const std::string message("test message");
  std::vector<char> buffer(message.size());

  size_t bytes_written = 0;
  while (bytes_written < message.size()) {
    scoped_refptr<IOBufferWithSize> write_buffer =
        base::MakeRefCounted<IOBufferWithSize>(message.size() - bytes_written);
    memmove(write_buffer->data(), message.data(), message.size());

    TestCompletionCallback write_callback;
    int write_result = accepted_socket->Write(
        write_buffer.get(), write_buffer->size(), write_callback.callback(),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    write_result = write_callback.GetResult(write_result);
    ASSERT_TRUE(write_result >= 0);
    ASSERT_TRUE(bytes_written + write_result <= message.size());
    bytes_written += write_result;
  }

  size_t bytes_read = 0;
  while (bytes_read < message.size()) {
    scoped_refptr<IOBufferWithSize> read_buffer =
        base::MakeRefCounted<IOBufferWithSize>(message.size() - bytes_read);
    TestCompletionCallback read_callback;
    int read_result = connecting_socket.Read(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    read_result = read_callback.GetResult(read_result);
    ASSERT_TRUE(read_result >= 0);
    ASSERT_TRUE(bytes_read + read_result <= message.size());
    memmove(&buffer[bytes_read], read_buffer->data(), read_result);
    bytes_read += read_result;
  }

  std::string received_message(buffer.begin(), buffer.end());
  ASSERT_EQ(message, received_message);
}

}  // namespace

}  // namespace net
