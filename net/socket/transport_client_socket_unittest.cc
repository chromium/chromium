// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_client_socket_test_util.h"

#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
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

const char kServerReply[] = "HTTP/1.1 404 Not Found";

}  // namespace

class TransportClientSocketTest : public ::testing::Test,
                                  public WithTaskEnvironment {
 public:
  TransportClientSocketTest()
      : socket_factory_(ClientSocketFactory::GetDefaultFactory()) {}

  ~TransportClientSocketTest() override = default;

  // Testcase hooks
  void SetUp() override;

  void CloseServerSocket() {
    // delete the connected_sock_, which will close it.
    connected_sock_.reset();
  }

  void AcceptCallback(int res) {
    ASSERT_THAT(res, IsOk());
    connect_loop_.Quit();
  }

  // Establishes a connection to the server.
  void EstablishConnection(TestCompletionCallback* callback);

 protected:
  base::RunLoop connect_loop_;
  uint16_t listen_port_ = 0;
  RecordingNetLogObserver net_log_observer_;
  const raw_ptr<ClientSocketFactory> socket_factory_;
  std::unique_ptr<StreamSocket> sock_;
  std::unique_ptr<StreamSocket> connected_sock_;

 private:
  std::unique_ptr<TCPServerSocket> listen_sock_;
};

void TransportClientSocketTest::SetUp() {
  // Open a server socket on an ephemeral port.
  listen_sock_ = std::make_unique<TCPServerSocket>(nullptr, NetLogSource());
  IPEndPoint local_address(IPAddress::IPv4Localhost(), 0);
  ASSERT_THAT(
      listen_sock_->Listen(local_address, 1, /*ipv6_only=*/std::nullopt),
      IsOk());
  // Get the server's address (including the actual port number).
  ASSERT_THAT(listen_sock_->GetLocalAddress(&local_address), IsOk());
  listen_port_ = local_address.port();
  listen_sock_->Accept(
      &connected_sock_,
      base::BindOnce(&TransportClientSocketTest::AcceptCallback,
                     base::Unretained(this)));

  AddressList addr = AddressList::CreateFromIPAddress(
      IPAddress::IPv4Localhost(), listen_port_);
  sock_ = socket_factory_->CreateTransportClientSocket(
      addr, nullptr, nullptr, NetLog::Get(), NetLogSource());
}

void TransportClientSocketTest::EstablishConnection(
    TestCompletionCallback* callback) {
  int rv = sock_->Connect(callback->callback());
  // Wait for |listen_sock_| to accept a connection.
  connect_loop_.Run();
  // Now wait for the client socket to accept the connection.
  EXPECT_THAT(callback->GetResult(rv), IsOk());
}

TEST_F(TransportClientSocketTest, Connect) {
  TestCompletionCallback callback;
  EXPECT_FALSE(sock_->IsConnected());

  int rv = sock_->Connect(callback.callback());
  // Wait for |listen_sock_| to accept a connection.
  connect_loop_.Run();

  auto net_log_entries = net_log_observer_.GetEntries();
  EXPECT_TRUE(
      LogContainsBeginEvent(net_log_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(
      LogContainsBeginEvent(net_log_entries, 1, NetLogEventType::TCP_CONNECT));
  // Now wait for the client socket to accept the connection.
  if (rv != OK) {
    ASSERT_EQ(rv, ERR_IO_PENDING);
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, OK);
  }

  EXPECT_TRUE(sock_->IsConnected());
  net_log_entries = net_log_observer_.GetEntries();
  EXPECT_TRUE(
      LogContainsEndEvent(net_log_entries, -1, NetLogEventType::TCP_CONNECT));

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());
}

TEST_F(TransportClientSocketTest, IsConnected) {
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  TestCompletionCallback callback;
  uint32_t bytes_read;

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(sock_->IsConnectedAndIdle());

  EstablishConnection(&callback);

  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_TRUE(sock_->IsConnectedAndIdle());

  // Send the request and wait for the server to respond.
  SendRequestAndResponse(sock_.get(), connected_sock_.get());

  // Drain a single byte so we know we've received some data.
  bytes_read = DrainStreamSocket(sock_.get(), buf.get(), 1, 1, &callback);
  ASSERT_EQ(bytes_read, 1u);

  // Socket should be considered connected, but not idle, due to
  // pending data.
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_FALSE(sock_->IsConnectedAndIdle());

  bytes_read = DrainStreamSocket(sock_.get(), buf.get(), 4096,
                                 strlen(kServerReply) - 1, &callback);
  ASSERT_EQ(bytes_read, strlen(kServerReply) - 1);

  // After draining the data, the socket should be back to connected
  // and idle.
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_TRUE(sock_->IsConnectedAndIdle());

  // This time close the server socket immediately after the server response.
  SendRequestAndResponse(sock_.get(), connected_sock_.get());
  CloseServerSocket();

  bytes_read = DrainStreamSocket(sock_.get(), buf.get(), 1, 1, &callback);
  ASSERT_EQ(bytes_read, 1u);

  // As above because of data.
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_FALSE(sock_->IsConnectedAndIdle());

  bytes_read = DrainStreamSocket(sock_.get(), buf.get(), 4096,
                                 strlen(kServerReply) - 1, &callback);
  ASSERT_EQ(bytes_read, strlen(kServerReply) - 1);

  // Once the data is drained, the socket should now be seen as not
  // connected.
  if (sock_->IsConnected()) {
    // In the unlikely event that the server's connection closure is not
    // processed in time, wait for the connection to be closed.
    int rv = sock_->Read(buf.get(), 4096, callback.callback());
    EXPECT_EQ(0, callback.GetResult(rv));
    EXPECT_FALSE(sock_->IsConnected());
  }
  EXPECT_FALSE(sock_->IsConnectedAndIdle());
}

TEST_F(TransportClientSocketTest, Read) {
  TestCompletionCallback callback;
  EstablishConnection(&callback);

  SendRequestAndResponse(sock_.get(), connected_sock_.get());

  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  uint32_t bytes_read = DrainStreamSocket(sock_.get(), buf.get(), 4096,
                                          strlen(kServerReply), &callback);
  ASSERT_EQ(bytes_read, strlen(kServerReply));
  ASSERT_EQ(std::string(kServerReply), std::string(buf->data(), bytes_read));

  // All data has been read now.  Read once more to force an ERR_IO_PENDING, and
  // then close the server socket, and note the close.

  int rv = sock_->Read(buf.get(), 4096, callback.callback());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  CloseServerSocket();
  EXPECT_EQ(0, callback.WaitForResult());
}

TEST_F(TransportClientSocketTest, Read_SmallChunks) {
  TestCompletionCallback callback;
  EstablishConnection(&callback);

  SendRequestAndResponse(sock_.get(), connected_sock_.get());

  auto buf = base::MakeRefCounted<IOBufferWithSize>(1);
  uint32_t bytes_read = 0;
  while (bytes_read < strlen(kServerReply)) {
    int rv = sock_->Read(buf.get(), 1, callback.callback());
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    rv = callback.GetResult(rv);

    ASSERT_EQ(1, rv);
    bytes_read += rv;
  }

  // All data has been read now.  Read once more to force an ERR_IO_PENDING, and
  // then close the server socket, and note the close.

  int rv = sock_->Read(buf.get(), 1, callback.callback());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  CloseServerSocket();
  EXPECT_EQ(0, callback.WaitForResult());
}

TEST_F(TransportClientSocketTest, Read_Interrupted) {
  TestCompletionCallback callback;
  EstablishConnection(&callback);

  SendRequestAndResponse(sock_.get(), connected_sock_.get());

  // Do a partial read and then exit.  This test should not crash!
  auto buf = base::MakeRefCounted<IOBufferWithSize>(16);
  int rv = sock_->Read(buf.get(), 16, callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  rv = callback.GetResult(rv);

  EXPECT_NE(0, rv);
}

TEST_F(TransportClientSocketTest, FullDuplex_ReadFirst) {
  TestCompletionCallback callback;
  EstablishConnection(&callback);

  // Read first.  There's no data, so it should return ERR_IO_PENDING.
  const int kBufLen = 4096;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(kBufLen);
  int rv = sock_->Read(buf.get(), kBufLen, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  const int kWriteBufLen = 64 * 1024;
  auto request_buffer = base::MakeRefCounted<IOBufferWithSize>(kWriteBufLen);
  char* request_data = request_buffer->data();
  memset(request_data, 'A', kWriteBufLen);
  TestCompletionCallback write_callback;

  int bytes_written = 0;
  while (true) {
    rv = sock_->Write(request_buffer.get(), kWriteBufLen,
                      write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    ASSERT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);
    if (rv == ERR_IO_PENDING) {
      ReadDataOfExpectedLength(connected_sock_.get(), bytes_written);
      SendServerResponse(connected_sock_.get());
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

TEST_F(TransportClientSocketTest, FullDuplex_WriteFirst) {
  TestCompletionCallback callback;
  EstablishConnection(&callback);

  const int kWriteBufLen = 64 * 1024;
  auto request_buffer = base::MakeRefCounted<IOBufferWithSize>(kWriteBufLen);
  char* request_data = request_buffer->data();
  memset(request_data, 'A', kWriteBufLen);
  TestCompletionCallback write_callback;

  int bytes_written = 0;
  while (true) {
    int rv =
        sock_->Write(request_buffer.get(), kWriteBufLen,
                     write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    ASSERT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      break;
    bytes_written += rv;
  }

  // Now we have the Write() blocked on ERR_IO_PENDING.  It's time to force the
  // Read() to block on ERR_IO_PENDING too.

  const int kBufLen = 4096;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(kBufLen);
  while (true) {
    int rv = sock_->Read(buf.get(), kBufLen, callback.callback());
    ASSERT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);
    if (rv == ERR_IO_PENDING)
      break;
  }

  // At this point, both read and write have returned ERR_IO_PENDING.  Now we
  // run the write and read callbacks to make sure they can handle full duplex
  // communications.

  ReadDataOfExpectedLength(connected_sock_.get(), bytes_written);
  SendServerResponse(connected_sock_.get());
  int rv = write_callback.WaitForResult();
  EXPECT_GE(rv, 0);

  rv = callback.WaitForResult();
  EXPECT_GT(rv, 0);
}

}  // namespace net
