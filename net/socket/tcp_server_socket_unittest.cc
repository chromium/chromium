// Copyright 2012 The Chromium Authors
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
    ASSERT_THAT(
        socket_.Listen(address, kListenBacklog, /*ipv6_only=*/std::nullopt),
        IsOk());
    ASSERT_THAT(socket_.GetLocalAddress(&local_address_), IsOk());
  }

  void SetUpIPv6(bool* success) {
    *success = false;
    IPEndPoint address(IPAddress::IPv6Localhost(), 0);
    if (socket_.Listen(address, kListenBacklog, /*ipv6_only=*/std::nullopt) !=
        0) {
      LOG(ERROR) << "Failed to listen on ::1 - probably because IPv6 is "
          "disabled. Skipping the test";
      return;
    }
    ASSERT_THAT(socket_.GetLocalAddress(&local_address_), IsOk());
    *success = true;
  }

  void SetUpIPv6AllInterfaces(bool ipv6_only) {
    IPEndPoint address(IPAddress::IPv6AllZeros(), 0);
    ASSERT_THAT(socket_.Listen(address, kListenBacklog, ipv6_only), IsOk());
    ASSERT_THAT(socket_.GetLocalAddress(&local_address_), IsOk());
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
                                    nullptr, NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<StreamSocket> accepted_socket;
  IPEndPoint peer_address;
  int result = socket_.Accept(&accepted_socket, accept_callback.callback(),
                              &peer_address);
  result = accept_callback.GetResult(result);
  ASSERT_THAT(result, IsOk());

  ASSERT_TRUE(accepted_socket.get() != nullptr);

  // |peer_address| should be correctly populated.
  EXPECT_EQ(peer_address.address(), local_address_.address());

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
  IPEndPoint peer_address;

  ASSERT_THAT(socket_.Accept(&accepted_socket, accept_callback.callback(),
                             &peer_address),
              IsError(ERR_IO_PENDING));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());
  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());

  EXPECT_TRUE(accepted_socket != nullptr);

  // |peer_address| should be correctly populated.
  EXPECT_EQ(peer_address.address(), local_address_.address());

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(GetPeerAddress(accepted_socket.get()).address(),
            local_address_.address());
}

// Test Accept() when client disconnects right after trying to connect.
TEST_F(TCPServerSocketTest, AcceptClientDisconnectAfterConnect) {
  ASSERT_NO_FATAL_FAILURE(SetUpIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<StreamSocket> accepted_socket;
  IPEndPoint peer_address;

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());
  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

  int accept_result = socket_.Accept(&accepted_socket,
                                     accept_callback.callback(), &peer_address);
  connecting_socket.Disconnect();

  EXPECT_THAT(accept_callback.GetResult(accept_result), IsOk());

  EXPECT_TRUE(accepted_socket != nullptr);

  // |peer_address| should be correctly populated.
  EXPECT_EQ(peer_address.address(), local_address_.address());
}

// Accept two connections simultaneously.
TEST_F(TCPServerSocketTest, Accept2Connections) {
  ASSERT_NO_FATAL_FAILURE(SetUpIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<StreamSocket> accepted_socket;
  IPEndPoint peer_address;

  ASSERT_EQ(ERR_IO_PENDING,
            socket_.Accept(&accepted_socket, accept_callback.callback(),
                           &peer_address));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback connect_callback2;
  TCPClientSocket connecting_socket2(local_address_list(), nullptr, nullptr,
                                     nullptr, NetLogSource());
  int connect_result2 =
      connecting_socket2.Connect(connect_callback2.callback());

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());

  TestCompletionCallback accept_callback2;
  std::unique_ptr<StreamSocket> accepted_socket2;
  IPEndPoint peer_address2;
  int result = socket_.Accept(&accepted_socket2, accept_callback2.callback(),
                              &peer_address2);
  result = accept_callback2.GetResult(result);
  ASSERT_THAT(result, IsOk());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
  EXPECT_THAT(connect_callback2.GetResult(connect_result2), IsOk());

  EXPECT_TRUE(accepted_socket != nullptr);
  EXPECT_TRUE(accepted_socket2 != nullptr);
  EXPECT_NE(accepted_socket.get(), accepted_socket2.get());

  EXPECT_EQ(peer_address.address(), local_address_.address());
  EXPECT_EQ(GetPeerAddress(accepted_socket.get()).address(),
            local_address_.address());
  EXPECT_EQ(peer_address2.address(), local_address_.address());
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
                                    nullptr, NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<StreamSocket> accepted_socket;
  IPEndPoint peer_address;
  int result = socket_.Accept(&accepted_socket, accept_callback.callback(),
                              &peer_address);
  result = accept_callback.GetResult(result);
  ASSERT_THAT(result, IsOk());

  ASSERT_TRUE(accepted_socket.get() != nullptr);

  // |peer_address| should be correctly populated.
  EXPECT_EQ(peer_address.address(), local_address_.address());

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(GetPeerAddress(accepted_socket.get()).address(),
            local_address_.address());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
}

class TCPServerSocketTestWithIPv6Only
    : public TCPServerSocketTest,
      public testing::WithParamInterface<bool> {
 public:
  void AttemptToConnect(const IPAddress& dest_addr, bool should_succeed) {
    TCPClientSocket connecting_socket(
        AddressList(IPEndPoint(dest_addr, local_address_.port())), nullptr,
        nullptr, nullptr, NetLogSource());

    TestCompletionCallback connect_cb;
    int connect_result = connecting_socket.Connect(connect_cb.callback());
    if (!should_succeed) {
      connect_result = connect_cb.GetResult(connect_result);
      ASSERT_EQ(connect_result, net::ERR_CONNECTION_REFUSED);
      return;
    }

    std::unique_ptr<StreamSocket> accepted_socket;
    IPEndPoint peer_address;

    TestCompletionCallback accept_cb;
    int accept_result =
        socket_.Accept(&accepted_socket, accept_cb.callback(), &peer_address);
    ASSERT_EQ(accept_cb.GetResult(accept_result), net::OK);
    ASSERT_EQ(connect_cb.GetResult(connect_result), net::OK);

    // |accepted_socket| should be available.
    ASSERT_NE(accepted_socket.get(), nullptr);

    // |peer_address| should be correctly populated.
    if (peer_address.address().IsIPv4MappedIPv6()) {
      ASSERT_EQ(ConvertIPv4MappedIPv6ToIPv4(peer_address.address()), dest_addr);
    } else {
      ASSERT_EQ(peer_address.address(), dest_addr);
    }
  }
};

TEST_P(TCPServerSocketTestWithIPv6Only, AcceptIPv6Only) {
  const bool ipv6_only = GetParam();
  ASSERT_NO_FATAL_FAILURE(SetUpIPv6AllInterfaces(ipv6_only));
  ASSERT_FALSE(local_address_list().empty());

  // 127.0.0.1 succeeds when |ipv6_only| is false and vice versa.
  AttemptToConnect(IPAddress::IPv4Localhost(), /*should_succeed=*/!ipv6_only);

  // ::1 succeeds regardless of |ipv6_only|.
  AttemptToConnect(IPAddress::IPv6Localhost(), /*should_succeed=*/true);
}

INSTANTIATE_TEST_SUITE_P(All, TCPServerSocketTestWithIPv6Only, testing::Bool());

TEST_F(TCPServerSocketTest, AcceptIO) {
  ASSERT_NO_FATAL_FAILURE(SetUpIPv4());

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<StreamSocket> accepted_socket;
  IPEndPoint peer_address;
  int result = socket_.Accept(&accepted_socket, accept_callback.callback(),
                              &peer_address);
  ASSERT_THAT(accept_callback.GetResult(result), IsOk());

  ASSERT_TRUE(accepted_socket.get() != nullptr);

  // |peer_address| should be correctly populated.
  EXPECT_EQ(peer_address.address(), local_address_.address());

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
