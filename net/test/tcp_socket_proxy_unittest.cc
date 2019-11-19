// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/tcp_socket_proxy.h"

#include "base/message_loop/message_pump_type.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace net {

class TcpSocketProxyTest : public TestWithTaskEnvironment {
 public:
  TcpSocketProxyTest() : io_thread_("TcpSocketProxyTest IO Thread") {
    EXPECT_TRUE(io_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0)));

    listen_socket_ =
        std::make_unique<TCPServerSocket>(nullptr, net::NetLogSource());
    int result =
        listen_socket_->Listen(IPEndPoint(IPAddress::IPv4Localhost(), 0), 5);
    EXPECT_THAT(result, IsOk());

    // Get local address.
    IPEndPoint address;
    result = listen_socket_->GetLocalAddress(&address);
    EXPECT_THAT(result, IsOk());

    proxy_ = std::make_unique<TcpSocketProxy>(io_thread_.task_runner());
    EXPECT_TRUE(proxy_->Initialize());

    proxy_address_ =
        IPEndPoint(IPAddress::IPv4Localhost(), proxy_->local_port());
    proxy_->Start(address);
  }

  void MakeConnection(std::unique_ptr<StreamSocket>* client_socket,
                      std::unique_ptr<StreamSocket>* server_socket) {
    TestCompletionCallback connect_callback;
    *client_socket = std::make_unique<TCPClientSocket>(
        AddressList(proxy_address_), nullptr, nullptr, NetLogSource());
    int connect_result = (*client_socket)->Connect(connect_callback.callback());

    TestCompletionCallback accept_callback;
    int result =
        listen_socket_->Accept(server_socket, accept_callback.callback());

    ASSERT_THAT(connect_callback.GetResult(connect_result), IsOk());
    ASSERT_THAT(accept_callback.GetResult(result), IsOk());

    EXPECT_TRUE((*server_socket)->IsConnected());
    EXPECT_TRUE((*client_socket)->IsConnected());
  }

  void SendAndReceiveData(StreamSocket* socket1, StreamSocket* socket2) {
    // Send just one byte to ensure we will need only one Write() and only one
    // Read().
    char test_message = '0';

    scoped_refptr<IOBuffer> write_buffer = base::MakeRefCounted<IOBuffer>(1);
    *write_buffer->data() = test_message;
    TestCompletionCallback write_callback;
    int write_result =
        socket1->Write(write_buffer.get(), 1, write_callback.callback(),
                       TRAFFIC_ANNOTATION_FOR_TESTS);

    scoped_refptr<IOBufferWithSize> read_buffer =
        base::MakeRefCounted<IOBufferWithSize>(1024);
    TestCompletionCallback read_callback;
    int read_result = socket2->Read(read_buffer.get(), read_buffer->size(),
                                    read_callback.callback());

    ASSERT_EQ(write_callback.GetResult(write_result), 1);
    ASSERT_EQ(read_callback.GetResult(read_result), 1);

    EXPECT_EQ(test_message, *read_buffer->data());
  }

  void ExpectClosed(StreamSocket* socket) {
    scoped_refptr<IOBufferWithSize> read_buffer =
        base::MakeRefCounted<IOBufferWithSize>(1024);
    TestCompletionCallback read_callback;
    int read_result = socket->Read(read_buffer.get(), read_buffer->size(),
                                   read_callback.callback());

    EXPECT_EQ(read_callback.GetResult(read_result), 0);
    EXPECT_FALSE(socket->IsConnected());
  }

 protected:
  base::Thread io_thread_;

  // Server socket that simulates testserver that TcpSocketProxy normally
  // would connect to.
  std::unique_ptr<TCPServerSocket> listen_socket_;

  std::unique_ptr<TcpSocketProxy> proxy_;

 private:
  IPEndPoint proxy_address_;
};

TEST_F(TcpSocketProxyTest, SendAndReceive) {
  std::unique_ptr<StreamSocket> client_socket;
  std::unique_ptr<StreamSocket> server_socket;
  MakeConnection(&client_socket, &server_socket);
  SendAndReceiveData(client_socket.get(), server_socket.get());
  SendAndReceiveData(server_socket.get(), client_socket.get());
}

TEST_F(TcpSocketProxyTest, TwoConnections) {
  std::unique_ptr<StreamSocket> client_socket1;
  std::unique_ptr<StreamSocket> server_socket1;
  MakeConnection(&client_socket1, &server_socket1);

  std::unique_ptr<StreamSocket> client_socket2;
  std::unique_ptr<StreamSocket> server_socket2;
  MakeConnection(&client_socket2, &server_socket2);

  SendAndReceiveData(client_socket1.get(), server_socket1.get());
  SendAndReceiveData(client_socket2.get(), server_socket2.get());
  SendAndReceiveData(server_socket1.get(), client_socket1.get());
  SendAndReceiveData(server_socket2.get(), client_socket2.get());
}

// Close socket on the server side and verify that it's closed on the client
// side.
// TODO(crbug.com/804429): This test hangs occasionally on iOS.
#if defined(OS_IOS)
#define MAYBE_DisconnectServer DISABLED_DisconnectServer
#else
#define MAYBE_DisconnectServer DisconnectServer
#endif
TEST_F(TcpSocketProxyTest, MAYBE_DisconnectServer) {
  std::unique_ptr<StreamSocket> client_socket;
  std::unique_ptr<StreamSocket> server_socket;
  MakeConnection(&client_socket, &server_socket);
  server_socket.reset();
  ExpectClosed(client_socket.get());
}

// Close socket on the client side and verify that it's closed on the server
// side.
// TODO(crbug.com/804429): This test hangs occasionally on iOS.
#if defined(OS_IOS)
#define MAYBE_DisconnectClient DISABLED_DisconnectClient
#else
#define MAYBE_DisconnectClient DisconnectClient
#endif
TEST_F(TcpSocketProxyTest, MAYBE_DisconnectClient) {
  std::unique_ptr<StreamSocket> client_socket;
  std::unique_ptr<StreamSocket> server_socket;
  MakeConnection(&client_socket, &server_socket);
  client_socket.reset();
  ExpectClosed(server_socket.get());
}

// Initialize() must fail if the port is in use.
TEST_F(TcpSocketProxyTest, PortInUse) {
  // Try initializing second proxy on the same port.
  auto proxy2 = std::make_unique<TcpSocketProxy>(io_thread_.task_runner());
  EXPECT_FALSE(proxy2->Initialize(proxy_->local_port()));
}

}  // namespace net
