// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/p2p/socket_tcp.h"

#include <stddef.h>
#include <stdint.h>

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/sys_byteorder.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "jingle/glue/fake_ssl_client_socket.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/p2p/socket_test_utils.h"
#include "services/network/proxy_resolving_client_socket_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Return;

namespace network {

class P2PSocketTcpTestBase : public testing::Test {
 protected:
  explicit P2PSocketTcpTestBase(P2PSocketType type) : socket_type_(type) {}

  void SetUp() override {
    mojo::PendingRemote<mojom::P2PSocketClient> socket_client;
    mojo::PendingRemote<mojom::P2PSocket> socket;
    auto socket_receiver = socket.InitWithNewPipeAndPassReceiver();

    fake_client_.reset(new FakeSocketClient(
        std::move(socket), socket_client.InitWithNewPipeAndPassReceiver()));

    EXPECT_CALL(*fake_client_.get(), SocketCreated(_, _)).Times(1);

    if (socket_type_ == P2P_SOCKET_TCP_CLIENT) {
      socket_impl_ = std::make_unique<P2PSocketTcp>(
          &socket_delegate_, std::move(socket_client),
          std::move(socket_receiver), P2P_SOCKET_TCP_CLIENT, nullptr);
    } else {
      socket_impl_ = std::make_unique<P2PSocketStunTcp>(
          &socket_delegate_, std::move(socket_client),
          std::move(socket_receiver), P2P_SOCKET_STUN_TCP_CLIENT, nullptr);
    }

    socket_ = new FakeSocket(&sent_data_);
    socket_->SetLocalAddress(ParseAddress(kTestLocalIpAddress, kTestPort1));
    socket_impl_->socket_.reset(socket_);

    dest_.ip_address = ParseAddress(kTestIpAddress1, kTestPort1);

    local_address_ = ParseAddress(kTestLocalIpAddress, kTestPort1);

    socket_impl_->remote_address_ = dest_;
    socket_impl_->OnConnected(net::OK);
    base::RunLoop().RunUntilIdle();
  }

  std::string IntToSize(int size) {
    std::string result;
    uint16_t size16 = base::HostToNet16(size);
    result.resize(sizeof(size16));
    memcpy(&result[0], &size16, sizeof(size16));
    return result;
  }

  base::test::TaskEnvironment task_environment_;
  std::string sent_data_;
  FakeSocket* socket_;  // Owned by |socket_impl_|.
  std::unique_ptr<P2PSocketTcpBase> socket_impl_;
  FakeP2PSocketDelegate socket_delegate_;
  std::unique_ptr<FakeSocketClient> fake_client_;

  net::IPEndPoint local_address_;
  P2PHostAndIPEndPoint dest_;
  P2PSocketType socket_type_;
};

class P2PSocketTcpTest : public P2PSocketTcpTestBase {
 protected:
  P2PSocketTcpTest() : P2PSocketTcpTestBase(P2P_SOCKET_TCP_CLIENT) {}
};

class P2PSocketStunTcpTest : public P2PSocketTcpTestBase {
 protected:
  P2PSocketStunTcpTest() : P2PSocketTcpTestBase(P2P_SOCKET_STUN_TCP_CLIENT) {}
};

// Verify that we can send STUN message and that they are formatted
// properly.
TEST_F(P2PSocketTcpTest, SendStunNoAuth) {
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(3);

  rtc::PacketOptions options;
  std::vector<int8_t> packet1;
  CreateStunRequest(&packet1);
  socket_impl_->Send(
      packet1, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::vector<int8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(
      packet2, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::vector<int8_t> packet3;
  CreateStunError(&packet3);
  socket_impl_->Send(
      packet3, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::string expected_data;
  expected_data.append(IntToSize(packet1.size()));
  expected_data.append(packet1.begin(), packet1.end());
  expected_data.append(IntToSize(packet2.size()));
  expected_data.append(packet2.begin(), packet2.end());
  expected_data.append(IntToSize(packet3.size()));
  expected_data.append(packet3.begin(), packet3.end());

  EXPECT_EQ(expected_data, sent_data_);

  base::RunLoop().RunUntilIdle();
}

// Verify that we can receive STUN messages from the socket, and that
// the messages are parsed properly.
TEST_F(P2PSocketTcpTest, ReceiveStun) {
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(3);

  rtc::PacketOptions options;
  std::vector<int8_t> packet1;
  CreateStunRequest(&packet1);
  socket_impl_->Send(
      packet1, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::vector<int8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(
      packet2, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::vector<int8_t> packet3;
  CreateStunError(&packet3);
  socket_impl_->Send(
      packet3, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::string received_data;
  received_data.append(IntToSize(packet1.size()));
  received_data.append(packet1.begin(), packet1.end());
  received_data.append(IntToSize(packet2.size()));
  received_data.append(packet2.begin(), packet2.end());
  received_data.append(IntToSize(packet3.size()));
  received_data.append(packet3.begin(), packet3.end());

  EXPECT_CALL(*fake_client_.get(), DataReceived(_, packet1, _));
  EXPECT_CALL(*fake_client_.get(), DataReceived(_, packet2, _));
  EXPECT_CALL(*fake_client_.get(), DataReceived(_, packet3, _));

  size_t pos = 0;
  size_t step_sizes[] = {3, 2, 1};
  size_t step = 0;
  while (pos < received_data.size()) {
    size_t step_size = std::min(step_sizes[step], received_data.size() - pos);
    socket_->AppendInputData(&received_data[pos], step_size);
    pos += step_size;
    if (++step >= base::size(step_sizes))
      step = 0;
  }

  base::RunLoop().RunUntilIdle();
}

// Verify that we can't send data before we've received STUN response
// from the other side.
TEST_F(P2PSocketTcpTest, SendDataNoAuth) {
  rtc::PacketOptions options;
  std::vector<int8_t> packet;
  CreateRandomPacket(&packet);

  auto* socket_impl_ptr = socket_impl_.get();
  socket_delegate_.ExpectDestruction(std::move(socket_impl_));
  socket_impl_ptr->Send(
      packet, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_EQ(0U, sent_data_.size());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_client_->connection_error());
}

// Verify that we can send data after we've received STUN response
// from the other side.
TEST_F(P2PSocketTcpTest, SendAfterStunRequest) {
  // Receive packet from |dest_|.
  std::vector<int8_t> request_packet;
  CreateStunRequest(&request_packet);

  std::string received_data;
  received_data.append(IntToSize(request_packet.size()));
  received_data.append(request_packet.begin(), request_packet.end());

  EXPECT_CALL(*fake_client_.get(), SendComplete(_));

  EXPECT_CALL(*fake_client_.get(), DataReceived(_, request_packet, _));
  socket_->AppendInputData(&received_data[0], received_data.size());

  rtc::PacketOptions options;
  // Now we should be able to send any data to |dest_|.
  std::vector<int8_t> packet;
  CreateRandomPacket(&packet);
  socket_impl_->Send(
      packet, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::string expected_data;
  expected_data.append(IntToSize(packet.size()));
  expected_data.append(packet.begin(), packet.end());

  EXPECT_EQ(expected_data, sent_data_);

  base::RunLoop().RunUntilIdle();
}

// Verify that asynchronous writes are handled correctly.
TEST_F(P2PSocketTcpTest, AsyncWrites) {
  socket_->set_async_write(true);

  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(2);

  rtc::PacketOptions options;
  std::vector<int8_t> packet1;
  CreateStunRequest(&packet1);

  socket_impl_->Send(
      packet1, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::vector<int8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(
      packet2, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  base::RunLoop().RunUntilIdle();

  std::string expected_data;
  expected_data.append(IntToSize(packet1.size()));
  expected_data.append(packet1.begin(), packet1.end());
  expected_data.append(IntToSize(packet2.size()));
  expected_data.append(packet2.begin(), packet2.end());

  EXPECT_EQ(expected_data, sent_data_);

  base::RunLoop().RunUntilIdle();
}

TEST_F(P2PSocketTcpTest, PacketIdIsPropagated) {
  socket_->set_async_write(true);

  const int32_t kRtcPacketId = 1234;

  int64_t now = rtc::TimeMillis();

  EXPECT_CALL(*fake_client_.get(),
              SendComplete(MatchSendPacketMetrics(kRtcPacketId, now)))
      .Times(1);

  rtc::PacketOptions options;
  options.packet_id = kRtcPacketId;
  std::vector<int8_t> packet1;
  CreateStunRequest(&packet1);

  socket_impl_->Send(
      packet1, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  base::RunLoop().RunUntilIdle();

  std::string expected_data;
  expected_data.append(IntToSize(packet1.size()));
  expected_data.append(packet1.begin(), packet1.end());

  EXPECT_EQ(expected_data, sent_data_);
}

TEST_F(P2PSocketTcpTest, SendDataWithPacketOptions) {
  std::vector<int8_t> request_packet;
  CreateStunRequest(&request_packet);

  std::string received_data;
  received_data.append(IntToSize(request_packet.size()));
  received_data.append(request_packet.begin(), request_packet.end());

  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(1);
  EXPECT_CALL(*fake_client_.get(), DataReceived(_, request_packet, _));
  socket_->AppendInputData(&received_data[0], received_data.size());

  rtc::PacketOptions options;
  options.packet_time_params.rtp_sendtime_extension_id = 3;
  // Now we should be able to send any data to |dest_|.
  std::vector<int8_t> packet;
  CreateRandomPacket(&packet);
  // Make it a RTP packet.
  *reinterpret_cast<uint16_t*>(&*packet.begin()) = base::HostToNet16(0x8000);
  socket_impl_->Send(
      packet, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::string expected_data;
  expected_data.append(IntToSize(packet.size()));
  expected_data.append(packet.begin(), packet.end());

  EXPECT_EQ(expected_data, sent_data_);

  base::RunLoop().RunUntilIdle();
}

// Verify that we can send STUN message and that they are formatted
// properly.
TEST_F(P2PSocketStunTcpTest, SendStunNoAuth) {
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(3);

  rtc::PacketOptions options;
  std::vector<int8_t> packet1;
  CreateStunRequest(&packet1);
  socket_impl_->Send(
      packet1, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::vector<int8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(
      packet2, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::vector<int8_t> packet3;
  CreateStunError(&packet3);
  socket_impl_->Send(
      packet3, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::string expected_data;
  expected_data.append(packet1.begin(), packet1.end());
  expected_data.append(packet2.begin(), packet2.end());
  expected_data.append(packet3.begin(), packet3.end());

  EXPECT_EQ(expected_data, sent_data_);

  base::RunLoop().RunUntilIdle();
}

// Verify that we can receive STUN messages from the socket, and that
// the messages are parsed properly.
TEST_F(P2PSocketStunTcpTest, ReceiveStun) {
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(3);

  rtc::PacketOptions options;
  std::vector<int8_t> packet1;
  CreateStunRequest(&packet1);
  socket_impl_->Send(
      packet1, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::vector<int8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(
      packet2, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::vector<int8_t> packet3;
  CreateStunError(&packet3);
  socket_impl_->Send(
      packet3, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::string received_data;
  received_data.append(packet1.begin(), packet1.end());
  received_data.append(packet2.begin(), packet2.end());
  received_data.append(packet3.begin(), packet3.end());

  EXPECT_CALL(*fake_client_.get(), DataReceived(_, packet1, _));
  EXPECT_CALL(*fake_client_.get(), DataReceived(_, packet2, _));
  EXPECT_CALL(*fake_client_.get(), DataReceived(_, packet3, _));

  size_t pos = 0;
  size_t step_sizes[] = {3, 2, 1};
  size_t step = 0;
  while (pos < received_data.size()) {
    size_t step_size = std::min(step_sizes[step], received_data.size() - pos);
    socket_->AppendInputData(&received_data[pos], step_size);
    pos += step_size;
    if (++step >= base::size(step_sizes))
      step = 0;
  }

  base::RunLoop().RunUntilIdle();
}

// Verify that we can't send data before we've received STUN response
// from the other side.
TEST_F(P2PSocketStunTcpTest, SendDataNoAuth) {
  rtc::PacketOptions options;
  std::vector<int8_t> packet;
  CreateRandomPacket(&packet);

  auto* socket_impl_ptr = socket_impl_.get();
  socket_delegate_.ExpectDestruction(std::move(socket_impl_));
  socket_impl_ptr->Send(
      packet, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_EQ(0U, sent_data_.size());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_client_->connection_error());
}

// Verify that asynchronous writes are handled correctly.
TEST_F(P2PSocketStunTcpTest, AsyncWrites) {
  socket_->set_async_write(true);

  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(2);

  rtc::PacketOptions options;
  std::vector<int8_t> packet1;
  CreateStunRequest(&packet1);
  socket_impl_->Send(
      packet1, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  std::vector<int8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(
      packet2, P2PPacketInfo(dest_.ip_address, options, 0),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  base::RunLoop().RunUntilIdle();

  std::string expected_data;
  expected_data.append(packet1.begin(), packet1.end());
  expected_data.append(packet2.begin(), packet2.end());

  EXPECT_EQ(expected_data, sent_data_);
}

// When pseudo-tls is used (e.g. for P2P_SOCKET_SSLTCP_CLIENT),
// ProxyResolvingClientSocket::Connect() won't be called twice.
// Regression test for crbug.com/840797.
TEST(P2PSocketTcpWithPseudoTlsTest, Basic) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  mojo::PendingRemote<mojom::P2PSocketClient> socket_client;
  mojo::PendingRemote<mojom::P2PSocket> socket;
  auto socket_receiver = socket.InitWithNewPipeAndPassReceiver();

  FakeSocketClient fake_client2(std::move(socket),
                                socket_client.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(fake_client2, SocketCreated(_, _)).Times(1);

  net::TestURLRequestContext context(true);
  net::MockClientSocketFactory mock_socket_factory;
  context.set_client_socket_factory(&mock_socket_factory);
  context.Init();
  ProxyResolvingClientSocketFactory factory(&context);

  base::StringPiece ssl_client_hello =
      jingle_glue::FakeSSLClientSocket::GetSslClientHello();
  base::StringPiece ssl_server_hello =
      jingle_glue::FakeSSLClientSocket::GetSslServerHello();
  net::MockRead reads[] = {
      net::MockRead(net::ASYNC, ssl_server_hello.data(),
                    ssl_server_hello.size()),
      net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)};
  net::MockWrite writes[] = {net::MockWrite(
      net::SYNCHRONOUS, ssl_client_hello.data(), ssl_client_hello.size())};
  net::StaticSocketDataProvider data_provider(reads, writes);
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  data_provider.set_connect_data(
      net::MockConnect(net::SYNCHRONOUS, net::OK, server_addr));
  mock_socket_factory.AddSocketDataProvider(&data_provider);

  FakeP2PSocketDelegate socket_delegate;
  P2PSocketTcp host(&socket_delegate, std::move(socket_client),
                    std::move(socket_receiver), P2P_SOCKET_SSLTCP_CLIENT,
                    &factory);
  P2PHostAndIPEndPoint dest;
  dest.ip_address = server_addr;
  host.Init(net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0), 0, 0, dest);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

class P2PSocketTcpWithTlsTest
    : public testing::TestWithParam<std::tuple<net::IoMode, P2PSocketType>> {};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    P2PSocketTcpWithTlsTest,
    ::testing::Combine(::testing::Values(net::SYNCHRONOUS, net::ASYNC),
                       ::testing::Values(P2P_SOCKET_TLS_CLIENT,
                                         P2P_SOCKET_STUN_TLS_CLIENT)));

// Tests that if a socket type satisfies IsTlsClientSocket(), TLS connection is
// established.
TEST_P(P2PSocketTcpWithTlsTest, Basic) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  mojo::PendingRemote<mojom::P2PSocketClient> socket_client;
  mojo::PendingRemote<mojom::P2PSocket> socket;
  auto socket_receiver = socket.InitWithNewPipeAndPassReceiver();

  FakeSocketClient fake_client2(std::move(socket),
                                socket_client.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(fake_client2, SocketCreated(_, _)).Times(1);

  net::TestURLRequestContext context(true);
  net::MockClientSocketFactory mock_socket_factory;
  context.set_client_socket_factory(&mock_socket_factory);
  context.Init();
  ProxyResolvingClientSocketFactory factory(&context);
  const net::IoMode io_mode = std::get<0>(GetParam());
  const P2PSocketType socket_type = std::get<1>(GetParam());
  // OnOpen() calls DoRead(), so populate the mock socket with a pending read.
  net::MockRead reads[] = {
      net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)};
  net::StaticSocketDataProvider data_provider(
      reads, base::span<const net::MockWrite>());
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  data_provider.set_connect_data(
      net::MockConnect(io_mode, net::OK, server_addr));
  net::SSLSocketDataProvider ssl_socket_provider(io_mode, net::OK);
  mock_socket_factory.AddSocketDataProvider(&data_provider);
  mock_socket_factory.AddSSLSocketDataProvider(&ssl_socket_provider);

  FakeP2PSocketDelegate socket_delegate;
  std::unique_ptr<P2PSocketTcpBase> host;
  if (socket_type == P2P_SOCKET_STUN_TLS_CLIENT) {
    host = std::make_unique<P2PSocketStunTcp>(
        &socket_delegate, std::move(socket_client), std::move(socket_receiver),
        socket_type, &factory);
  } else {
    host = std::make_unique<P2PSocketTcp>(
        &socket_delegate, std::move(socket_client), std::move(socket_receiver),
        socket_type, &factory);
  }
  P2PHostAndIPEndPoint dest;
  dest.ip_address = server_addr;
  host->Init(net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0), 0, 0, dest);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
  EXPECT_TRUE(ssl_socket_provider.ConnectDataConsumed());
}

}  // namespace network
