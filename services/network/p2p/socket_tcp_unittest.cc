// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/p2p/socket_tcp.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/webrtc/fake_ssl_client_socket.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
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
  // It is the helper method to get easy access to matcher.
  MOCK_METHOD(void,
              SinglePacketReceptionHelper,
              (const net::IPEndPoint& socket_address,
               base::span<const uint8_t> data,
               base::TimeTicks timestamp));

  explicit P2PSocketTcpTestBase(P2PSocketType type) : socket_type_(type) {}

  void SetUp() override {
    mojo::PendingRemote<mojom::P2PSocketClient> socket_client;
    mojo::PendingRemote<mojom::P2PSocket> socket;
    auto socket_receiver = socket.InitWithNewPipeAndPassReceiver();

    fake_client_ = std::make_unique<FakeSocketClient>(
        std::move(socket), socket_client.InitWithNewPipeAndPassReceiver());

    EXPECT_CALL(*fake_client_.get(), SocketCreated(_, _)).Times(1);

    // Unpack received batching packets for testing.
    ON_CALL(*fake_client_.get(), DataReceived(_))
        .WillByDefault(
            [this](const std::vector<network::mojom::P2PReceivedPacketPtr>
                       packets) {
              for (auto& packet : packets) {
                SinglePacketReceptionHelper(packet->socket_address,
                                            packet->data, packet->timestamp);
              }
              return;
            });

    if (socket_type_ == P2P_SOCKET_TCP_CLIENT) {
      socket_impl_ = std::make_unique<P2PSocketTcp>(
          &socket_delegate_, std::move(socket_client),
          std::move(socket_receiver), P2P_SOCKET_TCP_CLIENT,
          TRAFFIC_ANNOTATION_FOR_TESTS, nullptr);
    } else {
      socket_impl_ = std::make_unique<P2PSocketStunTcp>(
          &socket_delegate_, std::move(socket_client),
          std::move(socket_receiver), P2P_SOCKET_STUN_TCP_CLIENT,
          TRAFFIC_ANNOTATION_FOR_TESTS, nullptr);
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
    return std::string(base::as_string_view(
        base::U16ToBigEndian(base::checked_cast<uint16_t>(size))));
  }

  base::test::TaskEnvironment task_environment_;
  std::string sent_data_;
  std::unique_ptr<P2PSocketTcpBase> socket_impl_;
  raw_ptr<FakeSocket> socket_;  // Owned by |socket_impl_|.
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
  std::vector<uint8_t> packet1;
  CreateStunRequest(&packet1);
  socket_impl_->Send(packet1, P2PPacketInfo(dest_.ip_address, options, 0));

  std::vector<uint8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(packet2, P2PPacketInfo(dest_.ip_address, options, 0));

  std::vector<uint8_t> packet3;
  CreateStunError(&packet3);
  socket_impl_->Send(packet3, P2PPacketInfo(dest_.ip_address, options, 0));

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
  std::vector<uint8_t> packet1;
  CreateStunRequest(&packet1);
  socket_impl_->Send(packet1, P2PPacketInfo(dest_.ip_address, options, 0));

  std::vector<uint8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(packet2, P2PPacketInfo(dest_.ip_address, options, 0));

  std::vector<uint8_t> packet3;
  CreateStunError(&packet3);
  socket_impl_->Send(packet3, P2PPacketInfo(dest_.ip_address, options, 0));

  std::string received_data;
  received_data.append(IntToSize(packet1.size()));
  received_data.append(packet1.begin(), packet1.end());
  received_data.append(IntToSize(packet2.size()));
  received_data.append(packet2.begin(), packet2.end());
  received_data.append(IntToSize(packet3.size()));
  received_data.append(packet3.begin(), packet3.end());

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(3);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packet1), _));
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packet2), _));
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packet3), _));

  size_t pos = 0;
  size_t step_sizes[] = {3, 2, 1};
  size_t step = 0;
  while (pos < received_data.size()) {
    size_t step_size = std::min(step_sizes[step], received_data.size() - pos);
    socket_->AppendInputData(&received_data[pos], step_size);
    pos += step_size;
    if (++step >= std::size(step_sizes))
      step = 0;
  }

  base::RunLoop().RunUntilIdle();
}

// Verify that we can't send data before we've received STUN response
// from the other side.
TEST_F(P2PSocketTcpTest, SendDataNoAuth) {
  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);

  socket_ = nullptr;  // Since about to give up ownership of `socket_impl_`.
  auto* socket_impl_ptr = socket_impl_.get();
  socket_delegate_.ExpectDestruction(std::move(socket_impl_));
  socket_impl_ptr->Send(packet, P2PPacketInfo(dest_.ip_address, options, 0));

  EXPECT_EQ(0U, sent_data_.size());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_client_->connection_error());
}

// Verify that we can send data after we've received STUN response
// from the other side.
TEST_F(P2PSocketTcpTest, SendAfterStunRequest) {
  // Receive packet from |dest_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  std::string received_data;
  received_data.append(IntToSize(request_packet.size()));
  received_data.append(request_packet.begin(), request_packet.end());

  EXPECT_CALL(*fake_client_.get(), SendComplete(_));

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->AppendInputData(&received_data[0], received_data.size());

  rtc::PacketOptions options;
  // Now we should be able to send any data to |dest_|.
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);
  socket_impl_->Send(packet, P2PPacketInfo(dest_.ip_address, options, 0));

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
  std::vector<uint8_t> packet1;
  CreateStunRequest(&packet1);

  socket_impl_->Send(packet1, P2PPacketInfo(dest_.ip_address, options, 0));

  std::vector<uint8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(packet2, P2PPacketInfo(dest_.ip_address, options, 0));

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
  std::vector<uint8_t> packet1;
  CreateStunRequest(&packet1);

  socket_impl_->Send(packet1, P2PPacketInfo(dest_.ip_address, options, 0));

  base::RunLoop().RunUntilIdle();

  std::string expected_data;
  expected_data.append(IntToSize(packet1.size()));
  expected_data.append(packet1.begin(), packet1.end());

  EXPECT_EQ(expected_data, sent_data_);
}

TEST_F(P2PSocketTcpTest, SendDataWithPacketOptions) {
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  std::string received_data;
  received_data.append(IntToSize(request_packet.size()));
  received_data.append(request_packet.begin(), request_packet.end());

  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(1);
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->AppendInputData(&received_data[0], received_data.size());

  rtc::PacketOptions options;
  options.packet_time_params.rtp_sendtime_extension_id = 3;
  // Now we should be able to send any data to |dest_|.
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);
  // Make it a RTP packet.
  base::span(packet).first<2>().copy_from(
      base::U16ToBigEndian(uint16_t{0x8000}));
  socket_impl_->Send(packet, P2PPacketInfo(dest_.ip_address, options, 0));

  std::string expected_data;
  expected_data.append(IntToSize(packet.size()));
  expected_data.append(packet.begin(), packet.end());

  EXPECT_EQ(expected_data, sent_data_);

  base::RunLoop().RunUntilIdle();
}

// Verify that we ignore an empty frame.
TEST_F(P2PSocketTcpTest, IgnoreEmptyFrame) {
  std::vector<uint8_t> response_packet;
  CreateStunResponse(&response_packet);

  std::string received_data;
  received_data.append(IntToSize(response_packet.size()));
  received_data.append(response_packet.begin(), response_packet.end());
  socket_->AppendInputData(&received_data[0], received_data.size());

  std::vector<uint8_t> empty_packet;
  received_data.resize(0);
  received_data.append(IntToSize(empty_packet.size()));
  received_data.append(empty_packet.begin(), empty_packet.end());
  socket_->AppendInputData(&received_data[0], received_data.size());
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(0);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, _, _)).Times(0);
}

// Verify that we can send STUN message and that they are formatted
// properly.
TEST_F(P2PSocketStunTcpTest, SendStunNoAuth) {
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(3);

  rtc::PacketOptions options;
  std::vector<uint8_t> packet1;
  CreateStunRequest(&packet1);
  socket_impl_->Send(packet1, P2PPacketInfo(dest_.ip_address, options, 0));

  std::vector<uint8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(packet2, P2PPacketInfo(dest_.ip_address, options, 0));

  std::vector<uint8_t> packet3;
  CreateStunError(&packet3);
  socket_impl_->Send(packet3, P2PPacketInfo(dest_.ip_address, options, 0));

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
  std::vector<uint8_t> packet1;
  CreateStunRequest(&packet1);
  socket_impl_->Send(packet1, P2PPacketInfo(dest_.ip_address, options, 0));

  std::vector<uint8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(packet2, P2PPacketInfo(dest_.ip_address, options, 0));

  std::vector<uint8_t> packet3;
  CreateStunError(&packet3);
  socket_impl_->Send(packet3, P2PPacketInfo(dest_.ip_address, options, 0));

  std::string received_data;
  received_data.append(packet1.begin(), packet1.end());
  received_data.append(packet2.begin(), packet2.end());
  received_data.append(packet3.begin(), packet3.end());

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(3);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packet1), _));
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packet2), _));
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packet3), _));

  size_t pos = 0;
  size_t step_sizes[] = {3, 2, 1};
  size_t step = 0;
  while (pos < received_data.size()) {
    size_t step_size = std::min(step_sizes[step], received_data.size() - pos);
    socket_->AppendInputData(&received_data[pos], step_size);
    pos += step_size;
    if (++step >= std::size(step_sizes))
      step = 0;
  }

  base::RunLoop().RunUntilIdle();
}

// Verify that we can't send data before we've received STUN response
// from the other side.
TEST_F(P2PSocketStunTcpTest, SendDataNoAuth) {
  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);

  socket_ = nullptr;  // Since about to give up ownership of `socket_impl_`.
  auto* socket_impl_ptr = socket_impl_.get();
  socket_delegate_.ExpectDestruction(std::move(socket_impl_));
  socket_impl_ptr->Send(packet, P2PPacketInfo(dest_.ip_address, options, 0));

  EXPECT_EQ(0U, sent_data_.size());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_client_->connection_error());
}

// Verify that asynchronous writes are handled correctly.
TEST_F(P2PSocketStunTcpTest, AsyncWrites) {
  socket_->set_async_write(true);

  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(2);

  rtc::PacketOptions options;
  std::vector<uint8_t> packet1;
  CreateStunRequest(&packet1);
  socket_impl_->Send(packet1, P2PPacketInfo(dest_.ip_address, options, 0));

  std::vector<uint8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(packet2, P2PPacketInfo(dest_.ip_address, options, 0));

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

  net::MockClientSocketFactory mock_socket_factory;
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  context_builder->set_client_socket_factory_for_testing(&mock_socket_factory);
  auto context = context_builder->Build();
  ProxyResolvingClientSocketFactory factory(context.get());

  std::string_view ssl_client_hello =
      webrtc::FakeSSLClientSocket::GetSslClientHello();
  std::string_view ssl_server_hello =
      webrtc::FakeSSLClientSocket::GetSslServerHello();
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
                    TRAFFIC_ANNOTATION_FOR_TESTS, &factory);
  P2PHostAndIPEndPoint dest;
  dest.ip_address = server_addr;
  host.Init(net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0), 0, 0, dest,
            net::NetworkAnonymizationKey());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Test the case where P2PHostAndIPEndPoint::hostname is populated. Make sure
// there's a DNS lookup using the right hostname and NetworkAnonymizationKey.
TEST(P2PSocketTcpWithPseudoTlsTest, Hostname) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kPartitionConnectionsByNetworkIsolationKey);

  const char kHostname[] = "foo.test";
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  mojo::PendingRemote<mojom::P2PSocketClient> socket_client;
  mojo::PendingRemote<mojom::P2PSocket> socket;
  auto socket_receiver = socket.InitWithNewPipeAndPassReceiver();

  FakeSocketClient fake_client2(std::move(socket),
                                socket_client.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(fake_client2, SocketCreated(_, _)).Times(1);

  net::MockClientSocketFactory mock_socket_factory;
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  context_builder->set_client_socket_factory_for_testing(&mock_socket_factory);
  auto host_resolver = std::make_unique<net::MockCachingHostResolver>();
  host_resolver->rules()->AddRule(kHostname, "1.2.3.4");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto context = context_builder->Build();
  ProxyResolvingClientSocketFactory factory(context.get());

  std::string_view ssl_client_hello =
      webrtc::FakeSSLClientSocket::GetSslClientHello();
  std::string_view ssl_server_hello =
      webrtc::FakeSSLClientSocket::GetSslServerHello();
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
                    TRAFFIC_ANNOTATION_FOR_TESTS, &factory);
  P2PHostAndIPEndPoint dest;
  dest.ip_address = server_addr;
  dest.hostname = kHostname;
  net::NetworkAnonymizationKey network_anonymization_key =
      net::NetworkAnonymizationKey::CreateTransient();
  host.Init(net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0), 0, 0, dest,
            network_anonymization_key);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());

  // Check that the URL in kHostname is in the HostCache, with
  // |network_anonymization_key|.
  const net::HostPortPair kHostPortPair = net::HostPortPair(kHostname, 0);
  net::HostResolver::ResolveHostParameters params;
  params.source = net::HostResolverSource::LOCAL_ONLY;
  std::unique_ptr<net::HostResolver::ResolveHostRequest> request1 =
      context->host_resolver()->CreateRequest(kHostPortPair,
                                              network_anonymization_key,
                                              net::NetLogWithSource(), params);
  net::TestCompletionCallback callback1;
  int result = request1->Start(callback1.callback());
  EXPECT_EQ(net::OK, callback1.GetResult(result));

  // Check that the hostname is not in the DNS cache for other possible NIKs.
  const url::Origin kDestinationOrigin =
      url::Origin::Create(GURL(base::StringPrintf("https://%s", kHostname)));
  const net::NetworkAnonymizationKey kOtherNaks[] = {
      net::NetworkAnonymizationKey(),
      net::NetworkAnonymizationKey::CreateSameSite(
          net::SchemefulSite(kDestinationOrigin)),
  };
  for (const auto& other_nak : kOtherNaks) {
    std::unique_ptr<net::HostResolver::ResolveHostRequest> request2 =
        context->host_resolver()->CreateRequest(
            kHostPortPair, other_nak, net::NetLogWithSource(), params);
    net::TestCompletionCallback callback2;
    result = request2->Start(callback2.callback());
    EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, callback2.GetResult(result));
  }
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

  net::MockClientSocketFactory mock_socket_factory;
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  context_builder->set_client_socket_factory_for_testing(&mock_socket_factory);
  auto context = context_builder->Build();
  ProxyResolvingClientSocketFactory factory(context.get());
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
        socket_type, TRAFFIC_ANNOTATION_FOR_TESTS, &factory);
  } else {
    host = std::make_unique<P2PSocketTcp>(
        &socket_delegate, std::move(socket_client), std::move(socket_receiver),
        socket_type, TRAFFIC_ANNOTATION_FOR_TESTS, &factory);
  }
  P2PHostAndIPEndPoint dest;
  dest.ip_address = server_addr;
  host->Init(net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0), 0, 0, dest,
             net::NetworkAnonymizationKey());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
  EXPECT_TRUE(ssl_socket_provider.ConnectDataConsumed());
}

}  // namespace network
