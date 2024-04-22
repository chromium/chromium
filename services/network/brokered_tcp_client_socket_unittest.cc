// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/brokered_tcp_client_socket.h"

#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_source.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_server_socket.h"
#include "net/socket/transport_client_socket_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/brokered_client_socket_factory.h"
#include "services/network/test/test_socket_broker_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;
using testing::Not;

namespace network {

class BrokeredTcpClientSocketTest : public testing::Test,
                                    public net::WithTaskEnvironment {
 public:
  BrokeredTcpClientSocketTest()
      : receiver_(&socket_broker_impl_),
        client_socket_factory_(
            BrokeredClientSocketFactory(receiver_.BindNewPipeAndPassRemote())) {
  }

  ~BrokeredTcpClientSocketTest() override = default;

  void SetUp() override {
    // Open a server socket on an ephemeral port.
    listen_socket_ =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
    net::IPEndPoint local_address(net::IPAddress::IPv4Localhost(), 0);
    ASSERT_THAT(
        listen_socket_->Listen(local_address, 1, /*ipv6_only=*/std::nullopt),
        IsOk());
    // Get the server's address (including the actual port number).
    ASSERT_THAT(listen_socket_->GetLocalAddress(&local_address), IsOk());
    listen_socket_->Accept(&server_socket_, server_callback_.callback());
    net::AddressList addr = net::AddressList::CreateFromIPAddress(
        net::IPAddress::IPv4Localhost(), local_address.port());

    socket_ = client_socket_factory_.CreateTransportClientSocket(
        addr, nullptr, nullptr, net::NetLog::Get(), net::NetLogSource());

    // Confirm that we fail gracefully when making certain calls before
    // connecting.
    net::IPEndPoint test_address;
    EXPECT_EQ(socket_->GetPeerAddress(&test_address),
              net::ERR_SOCKET_NOT_CONNECTED);
    EXPECT_EQ(socket_->GetLocalAddress(&test_address),
              net::ERR_SOCKET_NOT_CONNECTED);
    EXPECT_EQ(socket_->SetReceiveBufferSize(256),
              net::ERR_SOCKET_NOT_CONNECTED);
    EXPECT_EQ(socket_->SetSendBufferSize(256), net::ERR_SOCKET_NOT_CONNECTED);
  }

  void ConnectClientSocket(net::TestCompletionCallback* connect_callback) {
    int connect_result = socket_->Connect(connect_callback->callback());

    if (connect_result != net::OK) {
      ASSERT_EQ(connect_result, net::ERR_IO_PENDING);
      int accept_result = server_callback_.WaitForResult();
      connect_result = connect_callback->WaitForResult();
      EXPECT_EQ(accept_result, net::OK);
      EXPECT_EQ(connect_result, net::OK);
    }
    EXPECT_TRUE(socket_->IsConnected());
    EXPECT_THAT(connect_callback->GetResult(connect_result), IsOk());
    net::IPEndPoint test_address;
    EXPECT_THAT(socket_->GetPeerAddress(&test_address), IsOk());
    EXPECT_THAT(socket_->GetLocalAddress(&test_address), IsOk());
    EXPECT_THAT(socket_->SetReceiveBufferSize(256), IsOk());
    EXPECT_THAT(socket_->SetSendBufferSize(256), IsOk());
  }

 protected:
  std::unique_ptr<net::TransportClientSocket> socket_;
  std::unique_ptr<net::TCPServerSocket> listen_socket_;
  std::unique_ptr<net::StreamSocket> server_socket_;
  mojo::Receiver<mojom::SocketBroker> receiver_;
  BrokeredClientSocketFactory client_socket_factory_;
  net::TestCompletionCallback server_callback_;

  net::MockClientSocketFactory mock_client_socket_factory_;
  net::StaticSocketDataProvider data_;

  TestSocketBrokerImpl socket_broker_impl_;

  bool close_server_socket_on_next_send_;
};

TEST_F(BrokeredTcpClientSocketTest, FailedConnect) {
  net::TestCompletionCallback callback;
  base::test::ScopedDisableRunLoopTimeout disable_timeout;

  socket_broker_impl_.SetConnectionFailure(true);

  int result = socket_->Connect(callback.callback());

  ASSERT_EQ(result, net::ERR_IO_PENDING);
  result = callback.WaitForResult();
  EXPECT_EQ(result, net::ERR_CONNECTION_FAILED);
}

TEST_F(BrokeredTcpClientSocketTest, Bind) {
  net::TestCompletionCallback callback;

  EXPECT_THAT(
      socket_->Bind(net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)),
      IsOk());
  EXPECT_EQ(socket_->IsConnected(), false);

  int result = socket_->Connect(callback.callback());

  ASSERT_EQ(result, net::ERR_IO_PENDING);
  result = callback.WaitForResult();
  EXPECT_EQ(result, net::OK);
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40918119): Re-enable on Fuchsia once cause for failure is
// determined.
#define MAYBE_FailedBind DISABLED_FailedBind
#else
#define MAYBE_FailedBind FailedBind
#endif
TEST_F(BrokeredTcpClientSocketTest, MAYBE_FailedBind) {
  net::TestCompletionCallback callback;

  net::TCPServerSocket ipv6_server_socket(nullptr, net::NetLogSource());
  net::IPEndPoint local_address(net::IPAddress::IPv6Localhost(), 0);
  int listen_result =
      ipv6_server_socket.Listen(local_address, 1, /*ipv6_only=*/std::nullopt);
  if (listen_result != net::OK) {
    LOG(ERROR) << "Failed to listen on ::1 - probably because IPv6 is disabled."
                  " Skipping the test";
    return;
  }
  ASSERT_THAT(ipv6_server_socket.GetLocalAddress(&local_address), IsOk());

  net::AddressList addr = net::AddressList::CreateFromIPAddress(
      net::IPAddress::IPv6Localhost(), local_address.port());

  socket_ = client_socket_factory_.CreateTransportClientSocket(
      addr, nullptr, nullptr, net::NetLog::Get(), net::NetLogSource());

  // Bind to an ipv4 address
  EXPECT_THAT(
      socket_->Bind(net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)),
      IsOk());
  EXPECT_EQ(socket_->IsConnected(), false);

  // Attempt to connect to an ipv6 address after binding to an ipv4 address.
  int result = socket_->Connect(callback.callback());

  ASSERT_EQ(result, net::ERR_IO_PENDING);
  EXPECT_THAT(callback.GetResult(result), Not(IsOk()));
}

TEST_F(BrokeredTcpClientSocketTest, WasEverUsed) {
  net::TestCompletionCallback callback;
  EXPECT_FALSE(socket_->WasEverUsed());

  // Writing before connecting should return ERR_SOCKET_NOT_CONNECTED
  const char kRequest[] = "GET / HTTP/1.0";
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kRequest);
  net::TestCompletionCallback write_callback_before_connect;
  int result = socket_->Write(write_buffer.get(), write_buffer->size(),
                              write_callback_before_connect.callback(),
                              TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(result, net::ERR_SOCKET_NOT_CONNECTED);
  ConnectClientSocket(&callback);

  // Just connecting the socket should not set WasEverUsed.
  EXPECT_FALSE(socket_->WasEverUsed());

  // Writing some data to the socket _should_ set WasEverUsed.
  net::TestCompletionCallback write_callback_after_connect;
  result = socket_->Write(write_buffer.get(), write_buffer->size(),
                          write_callback_after_connect.callback(),
                          TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(socket_->WasEverUsed());
  socket_->Disconnect();
  EXPECT_FALSE(socket_->IsConnected());

  EXPECT_TRUE(socket_->WasEverUsed());

  socket_->Disconnect();
}

TEST_F(BrokeredTcpClientSocketTest, SetKeepAlive) {
  net::TestCompletionCallback callback;

  // Non-connected sockets should not be able to set KeepAlive.
  ASSERT_FALSE(socket_->IsConnected());
  EXPECT_FALSE(socket_->SetKeepAlive(true /* enable */, 14 /* delay */));

  ConnectClientSocket(&callback);

  // Connected sockets should be able to enable and disable KeepAlive.
  ASSERT_TRUE(socket_->IsConnected());
  EXPECT_TRUE(socket_->SetKeepAlive(true /* enable */, 22 /* delay */));
  EXPECT_TRUE(socket_->SetKeepAlive(false /* disable */, 3 /* delay */));

  socket_->Disconnect();
}

TEST_F(BrokeredTcpClientSocketTest, SetNoDelay) {
  net::TestCompletionCallback callback;

  // Non-connected sockets should not be able to set NoDelay.
  ASSERT_FALSE(socket_->IsConnected());
  EXPECT_FALSE(socket_->SetNoDelay(true /* no_delay */));

  ConnectClientSocket(&callback);

  // Connected sockets should be able to enable and disable NoDelay.
  ASSERT_TRUE(socket_->IsConnected());
  EXPECT_TRUE(socket_->SetNoDelay(true /* no_delay */));
  EXPECT_TRUE(socket_->SetNoDelay(false /* no_delay */));

  socket_->Disconnect();
}

TEST_F(BrokeredTcpClientSocketTest, CancelReadIfReady) {
  net::TestCompletionCallback callback;
  ASSERT_FALSE(socket_->IsConnected());

  net::TestCompletionCallback read_callback;
  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(1);

  // Check that we gracefully fail when attempting to read or cancel reads
  // before connecting.
  ASSERT_EQ(socket_->Read(read_buf.get(), 1, read_callback.callback()),
            net::ERR_SOCKET_NOT_CONNECTED);
  ASSERT_EQ(socket_->ReadIfReady(read_buf.get(), 1, read_callback.callback()),
            net::ERR_SOCKET_NOT_CONNECTED);
  ASSERT_EQ(socket_->CancelReadIfReady(), net::ERR_SOCKET_NOT_CONNECTED);

  // Confirm no bytes have been read
  ASSERT_EQ(socket_->GetTotalReceivedBytes(), 0);

  ConnectClientSocket(&callback);

  // Attempt to read from the socket. There will not be anything to read.
  // Cancel the read immediately afterwards.
  int read_ret =
      socket_->ReadIfReady(read_buf.get(), 1, read_callback.callback());
  ASSERT_THAT(read_ret, IsError(net::ERR_IO_PENDING));
  ASSERT_THAT(socket_->CancelReadIfReady(), IsOk());

  // After the client writes data, the server should still not pick up a result.
  auto write_buf = base::MakeRefCounted<net::StringIOBuffer>("a");
  net::TestCompletionCallback write_callback;
  ASSERT_EQ(write_callback.GetResult(server_socket_->Write(
                write_buf.get(), write_buf->size(), write_callback.callback(),
                TRAFFIC_ANNOTATION_FOR_TESTS)),
            write_buf->size());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  // After a canceled read, future reads are still possible.
  while (true) {
    net::TestCompletionCallback read_callback2;
    read_ret =
        socket_->ReadIfReady(read_buf.get(), 1, read_callback2.callback());
    if (read_ret != net::ERR_IO_PENDING) {
      break;
    }
    ASSERT_THAT(read_callback2.GetResult(read_ret), IsOk());
  }
  EXPECT_EQ(socket_->GetTotalReceivedBytes(), 1);
  ASSERT_EQ(1, read_ret);
  EXPECT_EQ(read_buf->data()[0], 'a');

  socket_->Disconnect();
}

// IsConnected, FullDuplex_ReadFirst, and FullDuplex_WriteFirst are duplicated
// from transport_client_socket_unittest.cc since tests in //net can't depend on
// anything outside of //net.
TEST_F(BrokeredTcpClientSocketTest, IsConnected) {
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(4096);
  net::TestCompletionCallback callback;
  uint32_t bytes_read;
  const char kServerReply[] = "HTTP/1.1 404 Not Found";

  EXPECT_FALSE(socket_->IsConnected());
  EXPECT_FALSE(socket_->IsConnectedAndIdle());

  ConnectClientSocket(&callback);

  EXPECT_TRUE(socket_->IsConnected());
  EXPECT_TRUE(socket_->IsConnectedAndIdle());

  // Send the request and wait for the server to respond.
  net::SendRequestAndResponse(socket_.get(), server_socket_.get());

  // Drain a single byte so we know we've received some data.
  bytes_read =
      net::DrainStreamSocket(socket_.get(), buf.get(), 1, 1, &callback);
  ASSERT_EQ(bytes_read, 1u);

  // Socket should be considered connected, but not idle, due to
  // pending data.
  EXPECT_TRUE(socket_->IsConnected());
  EXPECT_FALSE(socket_->IsConnectedAndIdle());

  bytes_read = net::DrainStreamSocket(socket_.get(), buf.get(), 4096,
                                      strlen(kServerReply) - 1, &callback);
  ASSERT_EQ(bytes_read, strlen(kServerReply) - 1);

  // After draining the data, the socket should be back to connected
  // and idle.
  EXPECT_TRUE(socket_->IsConnected());
  EXPECT_TRUE(socket_->IsConnectedAndIdle());

  // This time close the server socket immediately after the server response.
  net::SendRequestAndResponse(socket_.get(), server_socket_.get());
  server_socket_.reset();

  bytes_read =
      net::DrainStreamSocket(socket_.get(), buf.get(), 1, 1, &callback);
  ASSERT_EQ(bytes_read, 1u);

  // As above because of data.
  EXPECT_TRUE(socket_->IsConnected());
  EXPECT_FALSE(socket_->IsConnectedAndIdle());

  bytes_read = net::DrainStreamSocket(socket_.get(), buf.get(), 4096,
                                      strlen(kServerReply) - 1, &callback);
  ASSERT_EQ(bytes_read, strlen(kServerReply) - 1);

  // Once the data is drained, the socket should now be seen as not
  // connected.
  if (socket_->IsConnected()) {
    // In the unlikely event that the server's connection closure is not
    // processed in time, wait for the connection to be closed.
    int rv = socket_->Read(buf.get(), 4096, callback.callback());
    EXPECT_EQ(0, callback.GetResult(rv));
    EXPECT_FALSE(socket_->IsConnected());
  }
  EXPECT_FALSE(socket_->IsConnectedAndIdle());
}

TEST_F(BrokeredTcpClientSocketTest, FullDuplex_ReadFirst) {
  net::TestCompletionCallback callback;
  ConnectClientSocket(&callback);

  // Read first.  There's no data, so it should return ERR_IO_PENDING.
  const int kBufLen = 4096;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kBufLen);
  int rv = socket_->Read(buf.get(), kBufLen, callback.callback());
  EXPECT_THAT(rv, IsError(net::ERR_IO_PENDING));

  const int kWriteBufLen = 64 * 1024;
  auto request_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kWriteBufLen);
  char* request_data = request_buffer->data();
  memset(request_data, 'A', kWriteBufLen);
  net::TestCompletionCallback write_callback;

  int bytes_written = 0;
  while (true) {
    rv =
        socket_->Write(request_buffer.get(), kWriteBufLen,
                       write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    ASSERT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);
    if (rv == net::ERR_IO_PENDING) {
      net::ReadDataOfExpectedLength(server_socket_.get(), bytes_written);
      net::SendServerResponse(server_socket_.get());
      rv = write_callback.WaitForResult();
      break;
    }
    bytes_written += rv;
  }

  // At this point, both read and write have returned ERR_IO_PENDING, and the
  // write callback has executed.  We wait for the read callback to run now to
  // make sure that the socket can handle full duplex communications.

  rv = callback.WaitForResult();
  EXPECT_GE(rv, 0);
}

TEST_F(BrokeredTcpClientSocketTest, FullDuplex_WriteFirst) {
  net::TestCompletionCallback callback;
  ConnectClientSocket(&callback);

  const int kWriteBufLen = 64 * 1024;
  auto request_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kWriteBufLen);
  char* request_data = request_buffer->data();
  memset(request_data, 'A', kWriteBufLen);
  net::TestCompletionCallback write_callback;

  int bytes_written = 0;
  while (true) {
    int rv =
        socket_->Write(request_buffer.get(), kWriteBufLen,
                       write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    ASSERT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING) {
      break;
    }
    bytes_written += rv;
  }

  // Now we have the Write() blocked on ERR_IO_PENDING.  It's time to force the
  // Read() to block on ERR_IO_PENDING too.

  const int kBufLen = 4096;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kBufLen);
  while (true) {
    int rv = socket_->Read(buf.get(), kBufLen, callback.callback());
    ASSERT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);
    if (rv == net::ERR_IO_PENDING) {
      break;
    }
  }

  // At this point, both read and write have returned ERR_IO_PENDING.  Now we
  // run the write and read callbacks to make sure they can handle full duplex
  // communications.

  net::ReadDataOfExpectedLength(server_socket_.get(), bytes_written);
  net::SendServerResponse(server_socket_.get());
  int rv = write_callback.WaitForResult();
  EXPECT_GE(rv, 0);

  rv = callback.WaitForResult();
  EXPECT_GT(rv, 0);
}

// Tests that setting a socket option in the BeforeConnectCallback works. With
// real sockets, socket options often have to be set before the connect() call,
// and the BeforeConnectCallback is the only way to do that, with a
// TCPClientSocket.
TEST_F(BrokeredTcpClientSocketTest, BeforeConnectCallback) {
  net::TestCompletionCallback callback;

  EXPECT_FALSE(socket_->IsConnected());
  EXPECT_FALSE(socket_->IsConnectedAndIdle());

  bool callback_was_called = false;
  socket_->SetBeforeConnectCallback(base::BindLambdaForTesting([&] {
    EXPECT_FALSE(socket_->IsConnected());
    callback_was_called = true;
    return int{net::OK};
  }));

  ConnectClientSocket(&callback);

  EXPECT_TRUE(callback_was_called);
}

// Duplicated from tcp_client_socket_unittest.cc since tests in //net can't
// depend on anything outside of //net.
//
// On Android, where socket tagging is
// supported, verify that BrokeredTcpClientSocket::Tag works as expected.
#if BUILDFLAG(IS_ANDROID)
TEST_F(BrokeredTcpClientSocketTest, Tag) {
  if (!net::CanGetTaggedBytes()) {
    DVLOG(0) << "Skipping test - GetTaggedBytes unsupported.";
    return;
  }

  // Start test server.
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr_list;
  ASSERT_TRUE(test_server.GetAddressList(&addr_list));

  BrokeredTcpClientSocket client_socket(addr_list, nullptr, nullptr, nullptr,
                                        net::NetLogSource(),
                                        &client_socket_factory_);

  // Verify TCP connect packets are tagged and counted properly.
  int32_t tag_val1 = 0x12345678;
  uint64_t old_traffic = net::GetTaggedBytes(tag_val1);
  net::SocketTag tag1(net::SocketTag::UNSET_UID, tag_val1);
  client_socket.ApplySocketTag(tag1);

  // Connect socket.
  net::TestCompletionCallback connect_callback;
  int connect_result = client_socket.Connect(connect_callback.callback());
  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
  EXPECT_GT(net::GetTaggedBytes(tag_val1), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  int32_t tag_val2 = 0x87654321;
  old_traffic = net::GetTaggedBytes(tag_val2);
  net::SocketTag tag2(getuid(), tag_val2);
  client_socket.ApplySocketTag(tag2);
  const char kRequest1[] = "GET / HTTP/1.0";
  auto write_buffer1 = base::MakeRefCounted<net::StringIOBuffer>(kRequest1);
  net::TestCompletionCallback write_callback1;
  EXPECT_EQ(client_socket.Write(write_buffer1.get(), strlen(kRequest1),
                                write_callback1.callback(),
                                TRAFFIC_ANNOTATION_FOR_TESTS),
            static_cast<int>(strlen(kRequest1)));
  EXPECT_GT(net::GetTaggedBytes(tag_val2), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  old_traffic = net::GetTaggedBytes(tag_val1);
  client_socket.ApplySocketTag(tag1);
  const char kRequest2[] = "\n\n";
  scoped_refptr<net::IOBufferWithSize> write_buffer2 =
      base::MakeRefCounted<net::IOBufferWithSize>(strlen(kRequest2));
  memmove(write_buffer2->data(), kRequest2, strlen(kRequest2));
  net::TestCompletionCallback write_callback2;
  EXPECT_EQ(client_socket.Write(write_buffer2.get(), strlen(kRequest2),
                                write_callback2.callback(),
                                TRAFFIC_ANNOTATION_FOR_TESTS),
            static_cast<int>(strlen(kRequest2)));
  EXPECT_GT(net::GetTaggedBytes(tag_val1), old_traffic);

  client_socket.Disconnect();
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace network
