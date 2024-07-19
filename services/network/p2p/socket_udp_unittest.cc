// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/p2p/socket_udp.h"

#include <stdint.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/p2p/socket_test_utils.h"
#include "services/network/p2p/socket_throttler.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "services/network/throttling/network_conditions.h"
#include "services/network/throttling/throttling_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Return;

namespace {

// TODO(nisse): We can't currently use rtc::ScopedFakeClock, because
// we don't link with webrtc rtc_base_tests_utils. So roll our own.

// Creating an object of this class makes rtc::TimeMicros() and
// related functions return zero unless the clock is advanced.
class ScopedFakeClock : public rtc::ClockInterface {
 public:
  ScopedFakeClock() { prev_clock_ = rtc::SetClockForTesting(this); }
  ~ScopedFakeClock() override { rtc::SetClockForTesting(prev_clock_); }
  // ClockInterface implementation.
  int64_t TimeNanos() const override { return time_nanos_; }
  void SetTimeNanos(uint64_t time_nanos) { time_nanos_ = time_nanos; }

 private:
  raw_ptr<ClockInterface> prev_clock_;
  uint64_t time_nanos_ = 0;
};

class FakeDatagramServerSocket : public net::DatagramServerSocket {
 public:
  typedef std::
      tuple<net::IPEndPoint, std::vector<uint8_t>, std::optional<uint64_t>>
          UDPPacket;

  // P2PSocketUdp destroys a socket on errors so sent packets
  // need to be stored outside of this object.
  FakeDatagramServerSocket(base::circular_deque<UDPPacket>* sent_packets,
                           std::vector<uint16_t>* used_ports,
                           ScopedFakeClock* fake_clock)
      : sent_packets_(sent_packets),
        recv_address_(nullptr),
        recv_size_(0),
        used_ports_(used_ports),
        fake_clock_ptr_(fake_clock) {}

  void Close() override {}

  int GetPeerAddress(net::IPEndPoint* address) const override {
    NOTREACHED_IN_MIGRATION();
    return net::ERR_SOCKET_NOT_CONNECTED;
  }

  int GetLocalAddress(net::IPEndPoint* address) const override {
    *address = address_;
    return 0;
  }

  void UseNonBlockingIO() override {}

  int Listen(const net::IPEndPoint& address) override {
    if (used_ports_) {
      for (auto used_port : *used_ports_) {
        if (used_port == address.port())
          return -1;
      }
      used_ports_->push_back(address.port());
    }

    address_ = address;
    return 0;
  }

  int RecvFrom(net::IOBuffer* buf,
               int buf_len,
               net::IPEndPoint* address,
               net::CompletionOnceCallback callback) override {
    CHECK(recv_callback_.is_null());
    if (incoming_packets_.size() > 0) {
      scoped_refptr<net::IOBuffer> buffer(buf);
      int size = std::min(
          static_cast<int>(std::get<1>(incoming_packets_.front()).size()),
          buf_len);
      memcpy(buffer->data(), &*(std::get<1>(incoming_packets_.front())).begin(),
             size);
      *address = std::get<0>(incoming_packets_.front());
      std::optional<uint64_t> received_time =
          std::get<2>(incoming_packets_.front());
      if (received_time) {
        fake_clock_ptr_->SetTimeNanos(*received_time);
      }
      incoming_packets_.pop_front();
      return size;
    } else {
      recv_callback_ = std::move(callback);
      recv_buffer_ = buf;
      recv_size_ = buf_len;
      recv_address_ = address;
      return net::ERR_IO_PENDING;
    }
  }

  int SendTo(net::IOBuffer* buf,
             int buf_len,
             const net::IPEndPoint& address,
             net::CompletionOnceCallback callback) override {
    scoped_refptr<net::IOBuffer> buffer(buf);
    std::vector<uint8_t> data_vector(buffer->data(), buffer->data() + buf_len);
    sent_packets_->push_back(UDPPacket(address, data_vector, std::nullopt));
    return buf_len;
  }

  int SetReceiveBufferSize(int32_t size) override { return net::OK; }

  int SetSendBufferSize(int32_t size) override { return net::OK; }

  int SetDoNotFragment() override { return net::OK; }

  int SetRecvTos() override {
    is_recv_ecn_enabled_ = true;
    return net::OK;
  }

  void SetMsgConfirm(bool confirm) override {}

  void ReceivePacket(const net::IPEndPoint& address,
                     std::vector<uint8_t> data) {
    AddRecvPacket(address, data);
    FireRecvCallback();
  }

  // Add a packet into the buffer, and specify the fake clock time when
  // the packet is received by socket.
  void AddRecvPacket(
      const net::IPEndPoint& address,
      const std::vector<uint8_t> data,
      const std::optional<uint64_t> received_time = std::nullopt) {
    incoming_packets_.push_back(UDPPacket(address, data, received_time));
  }

  void FireRecvCallback() {
    if (!recv_callback_.is_null()) {
      DCHECK(!incoming_packets_.empty());
      int size = std::min(
          recv_size_,
          static_cast<int>(std::get<1>(incoming_packets_.front()).size()));
      memcpy(recv_buffer_->data(),
             &*std::get<1>(incoming_packets_.front()).begin(), size);
      *recv_address_ = std::get<0>(incoming_packets_.front());
      std::optional<uint64_t> received_time =
          std::get<2>(incoming_packets_.front());
      if (received_time) {
        fake_clock_ptr_->SetTimeNanos(*received_time);
      }
      incoming_packets_.pop_front();
      recv_buffer_ = nullptr;
      std::move(recv_callback_).Run(size);
    }
  }

  const net::NetLogWithSource& NetLog() const override { return net_log_; }

  void AllowAddressReuse() override { NOTIMPLEMENTED(); }

  void AllowBroadcast() override { NOTIMPLEMENTED(); }

  void AllowAddressSharingForMulticast() override { NOTIMPLEMENTED(); }

  int JoinGroup(const net::IPAddress& group_address) const override {
    NOTIMPLEMENTED();
    return net::ERR_NOT_IMPLEMENTED;
  }

  int LeaveGroup(const net::IPAddress& group_address) const override {
    NOTIMPLEMENTED();
    return net::ERR_NOT_IMPLEMENTED;
  }

  int SetMulticastInterface(uint32_t interface_index) override {
    NOTIMPLEMENTED();
    return net::ERR_NOT_IMPLEMENTED;
  }

  int SetMulticastTimeToLive(int time_to_live) override {
    NOTIMPLEMENTED();
    return net::ERR_NOT_IMPLEMENTED;
  }

  int SetMulticastLoopbackMode(bool loopback) override {
    NOTIMPLEMENTED();
    return net::ERR_NOT_IMPLEMENTED;
  }

  int SetDiffServCodePoint(net::DiffServCodePoint dscp) override {
    NOTIMPLEMENTED();
    return net::ERR_NOT_IMPLEMENTED;
  }

  int SetTos(net::DiffServCodePoint dscp, net::EcnCodePoint ecn) override {
    NOTIMPLEMENTED();
    return net::ERR_NOT_IMPLEMENTED;
  }

  void DetachFromThread() override { NOTIMPLEMENTED(); }

  net::DscpAndEcn GetLastTos() const override {
    if (!is_recv_ecn_enabled_) {
      return {net::DSCP_DEFAULT, net::ECN_DEFAULT};
    } else {
      return {net::DSCP_DEFAULT, net::ECN_ECT1};
    }
  }

 private:
  bool is_recv_ecn_enabled_ = false;
  net::IPEndPoint address_;
  raw_ptr<base::circular_deque<UDPPacket>> sent_packets_;
  base::circular_deque<UDPPacket> incoming_packets_;
  net::NetLogWithSource net_log_;

  scoped_refptr<net::IOBuffer> recv_buffer_;
  raw_ptr<net::IPEndPoint> recv_address_;
  int recv_size_;
  net::CompletionOnceCallback recv_callback_;
  raw_ptr<std::vector<uint16_t>> used_ports_;

  // Owned by |P2PSocketUdpTest|.
  raw_ptr<ScopedFakeClock> fake_clock_ptr_;
};

std::unique_ptr<net::DatagramServerSocket> CreateFakeDatagramServerSocket(
    base::circular_deque<FakeDatagramServerSocket::UDPPacket>* sent_packets,
    std::vector<uint16_t>* used_ports,
    ScopedFakeClock* fake_clock,
    net::NetLog* net_log) {
  return std::make_unique<FakeDatagramServerSocket>(sent_packets, used_ports,
                                                    fake_clock);
}

}  // namespace

namespace network {

class P2PSocketUdpTest : public testing::Test {
 public:
  P2PSocketUdpTest() : P2PSocketUdpTest(std::nullopt) {}

 protected:
  explicit P2PSocketUdpTest(
      std::optional<base::UnguessableToken> devtools_token)
      : devtools_token_(devtools_token),
        net_log_with_source_(
            net::NetLogWithSource::Make(net::NetLog::Get(),
                                        net::NetLogSourceType::UDP_SOCKET)) {}

  // It is the helper method to get easy access to matcher.
  MOCK_METHOD(void,
              SinglePacketReceptionHelper,
              (const net::IPEndPoint& socket_address,
               base::span<const uint8_t> data,
               base::TimeTicks timestamp));

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
            [this](std::vector<network::mojom::P2PReceivedPacketPtr> packets) {
              for (auto& packet : packets) {
                SinglePacketReceptionHelper(packet->socket_address,
                                            packet->data, packet->timestamp);
                received_packets_.emplace_back(std::move(packet));
              }
              return;
            });

    socket_impl_ = std::make_unique<P2PSocketUdp>(
        &socket_delegate_, std::move(socket_client), std::move(socket_receiver),
        &throttler_, TRAFFIC_ANNOTATION_FOR_TESTS,
        net_log_with_source_.net_log(),
        base::BindRepeating(&CreateFakeDatagramServerSocket, &sent_packets_,
                            nullptr, &fake_clock_),
        devtools_token_);

    local_address_ = ParseAddress(kTestLocalIpAddress, kTestPort1);
    socket_impl_->Init(
        local_address_, 0, 0,
        P2PHostAndIPEndPoint(std::string(),
                             ParseAddress(kTestIpAddress1, kTestPort1)),
        net::NetworkAnonymizationKey());
    socket_ = GetSocketFromHost(socket_impl_.get());

    dest1_ = ParseAddress(kTestIpAddress1, kTestPort1);
    dest2_ = ParseAddress(kTestIpAddress2, kTestPort2);
  }

  static FakeDatagramServerSocket* GetSocketFromHost(
      P2PSocketUdp* socket_host) {
    return static_cast<FakeDatagramServerSocket*>(socket_host->socket_.get());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  P2PMessageThrottler throttler_;
  ScopedFakeClock fake_clock_;
  base::circular_deque<FakeDatagramServerSocket::UDPPacket> sent_packets_;
  base::circular_deque<mojo::StructPtr<network::mojom::P2PReceivedPacket>>
      received_packets_;
  FakeP2PSocketDelegate socket_delegate_;
  std::unique_ptr<P2PSocketUdp> socket_impl_;
  raw_ptr<FakeDatagramServerSocket> socket_;  // Owned by |socket_impl_|.
  std::unique_ptr<FakeSocketClient> fake_client_;
  std::optional<base::UnguessableToken> devtools_token_ = std::nullopt;
  net::NetLogWithSource net_log_with_source_;

  net::IPEndPoint local_address_;

  net::IPEndPoint dest1_;
  net::IPEndPoint dest2_;
};

// Verify that we can send STUN messages before we receive anything
// from the other side.
TEST_F(P2PSocketUdpTest, SendStunNoAuth) {
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(3);

  rtc::PacketOptions options;
  std::vector<uint8_t> packet1;
  CreateStunRequest(&packet1);
  socket_impl_->Send(packet1, P2PPacketInfo(dest1_, options, 0));

  std::vector<uint8_t> packet2;
  CreateStunResponse(&packet2);
  socket_impl_->Send(packet2, P2PPacketInfo(dest1_, options, 0));

  std::vector<uint8_t> packet3;
  CreateStunError(&packet3);
  socket_impl_->Send(packet3, P2PPacketInfo(dest1_, options, 0));

  ASSERT_EQ(sent_packets_.size(), 3U);
  ASSERT_EQ(std::get<1>(sent_packets_[0]), packet1);
  ASSERT_EQ(std::get<1>(sent_packets_[1]), packet2);
  ASSERT_EQ(std::get<1>(sent_packets_[2]), packet3);

  base::RunLoop().RunUntilIdle();
}

// Verify that no data packets can be sent before STUN binding has
// finished.
TEST_F(P2PSocketUdpTest, SendDataNoAuth) {
  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);

  socket_ = nullptr;  // Since about to give up `socket_impl_`.
  auto* socket_impl_ptr = socket_impl_.get();
  socket_delegate_.ExpectDestruction(std::move(socket_impl_));
  socket_impl_ptr->Send(packet, P2PPacketInfo(dest1_, options, 0));

  ASSERT_EQ(sent_packets_.size(), 0U);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_client_->connection_error());
}

// Verify that we can send data after we've received STUN request
// from the other side.
TEST_F(P2PSocketUdpTest, SendAfterStunRequest) {
  // Receive packet from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  // Now we should be able to send any data to |dest1_|.
  EXPECT_CALL(*fake_client_.get(), SendComplete(_));

  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));

  ASSERT_EQ(1U, sent_packets_.size());
  ASSERT_EQ(dest1_, std::get<0>(sent_packets_[0]));

  base::RunLoop().RunUntilIdle();
}

// Verify that we can send data after we've received STUN response
// from the other side.
TEST_F(P2PSocketUdpTest, SendAfterStunResponse) {
  // Receive packet from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunResponse(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  // Now we should be able to send any data to |dest1_|.
  EXPECT_CALL(*fake_client_.get(), SendComplete(_));

  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));

  ASSERT_EQ(1U, sent_packets_.size());
  ASSERT_EQ(dest1_, std::get<0>(sent_packets_[0]));

  base::RunLoop().RunUntilIdle();
}

// Verify messages still cannot be sent to an unathorized host after
// successful binding with different host.
TEST_F(P2PSocketUdpTest, SendAfterStunResponseDifferentHost) {
  // Receive packet from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunResponse(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  // Should fail when trying to send the same packet to |dest2_|.
  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);

  socket_ = nullptr;  // Since about to give up `socket_impl_`.
  auto* socket_impl_ptr = socket_impl_.get();
  socket_delegate_.ExpectDestruction(std::move(socket_impl_));
  socket_impl_ptr->Send(packet, P2PPacketInfo(dest2_, options, 0));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_client_->connection_error());
}

TEST_F(P2PSocketUdpTest, BatchesSendAfterSendingAllowed) {
  // Open for sends to `dest1_`.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);
  socket_->ReceivePacket(dest1_, request_packet);

  network::P2PPacketInfo info;
  info.destination = dest1_;
  std::vector<network::mojom::P2PSendPacketPtr> batch;
  info.packet_id = 1;
  std::vector<uint8_t> packet1;
  CreateRandomPacket(&packet1);
  batch.push_back(network::mojom::P2PSendPacket::New(packet1, info));
  info.packet_id = 2;
  std::vector<uint8_t> packet2;
  CreateRandomPacket(&packet2);
  batch.push_back(network::mojom::P2PSendPacket::New(packet2, info));
  socket_impl_->SendBatch(std::move(batch));
  ASSERT_EQ(sent_packets_.size(), 2u);
  EXPECT_CALL(*fake_client_, SendBatchComplete(ElementsAre(
                                 Field(&P2PSendPacketMetrics::packet_id, 1),
                                 Field(&P2PSendPacketMetrics::packet_id, 2))));
  base::RunLoop().RunUntilIdle();
}

// Verify throttler not allowing unlimited sending of ICE messages to
// any destination.
TEST_F(P2PSocketUdpTest, ThrottleAfterLimit) {
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(3);

  rtc::PacketOptions options;
  std::vector<uint8_t> packet1;
  CreateStunRequest(&packet1);
  throttler_.SetSendIceBandwidth(packet1.size() * 2);
  socket_impl_->Send(packet1, P2PPacketInfo(dest1_, options, 0));
  socket_impl_->Send(packet1, P2PPacketInfo(dest2_, options, 0));

  net::IPEndPoint dest3 = ParseAddress(kTestIpAddress1, 2222);
  // This packet must be dropped by the throttler.
  socket_impl_->Send(packet1, P2PPacketInfo(dest3, options, 0));
  ASSERT_EQ(sent_packets_.size(), 2U);

  base::RunLoop().RunUntilIdle();
}

// Verify we can send packets to a known destination when ICE throttling is
// active.
TEST_F(P2PSocketUdpTest, ThrottleAfterLimitAfterReceive) {
  // Receive packet from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _))
      .Times(1);
  socket_->ReceivePacket(dest1_, request_packet);

  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(6);

  rtc::PacketOptions options;
  std::vector<uint8_t> packet1;
  CreateStunRequest(&packet1);
  throttler_.SetSendIceBandwidth(packet1.size());
  // |dest1_| is known address, throttling will not be applied.
  socket_impl_->Send(packet1, P2PPacketInfo(dest1_, options, 0));
  // Trying to send the packet to dest1_ in the same window. It should go.
  socket_impl_->Send(packet1, P2PPacketInfo(dest1_, options, 0));

  // Throttler should allow this packet to go through.
  socket_impl_->Send(packet1, P2PPacketInfo(dest2_, options, 0));

  net::IPEndPoint dest3 = ParseAddress(kTestIpAddress1, 2223);
  // This packet will be dropped, as limit only for a single packet.
  socket_impl_->Send(packet1, P2PPacketInfo(dest3, options, 0));
  net::IPEndPoint dest4 = ParseAddress(kTestIpAddress1, 2224);
  // This packet should also be dropped.
  socket_impl_->Send(packet1, P2PPacketInfo(dest4, options, 0));
  // |dest1| is known, we can send as many packets to it.
  socket_impl_->Send(packet1, P2PPacketInfo(dest1_, options, 0));
  ASSERT_EQ(sent_packets_.size(), 4U);

  base::RunLoop().RunUntilIdle();
}

// Test that once the limit is hit, the throttling stops at the expected time,
// allowing packets to be sent again.
TEST_F(P2PSocketUdpTest, ThrottlingStopsAtExpectedTimes) {
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(12);

  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateStunRequest(&packet);
  // Limit of 2 packets per second.
  throttler_.SetSendIceBandwidth(packet.size() * 2);
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));
  socket_impl_->Send(packet, P2PPacketInfo(dest2_, options, 0));
  EXPECT_EQ(2U, sent_packets_.size());

  // These packets must be dropped by the throttler since the limit was hit and
  // the time hasn't advanced.
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));
  socket_impl_->Send(packet, P2PPacketInfo(dest2_, options, 0));
  EXPECT_EQ(2U, sent_packets_.size());

  // Advance the time to 0.999 seconds; throttling should still just barely be
  // active.
  fake_clock_.SetTimeNanos(rtc::kNumNanosecsPerMillisec * 999);
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));
  socket_impl_->Send(packet, P2PPacketInfo(dest2_, options, 0));
  EXPECT_EQ(2U, sent_packets_.size());

  // After hitting the second mark, we should be able to send again.
  // Add an extra millisecond to account for rounding errors.
  fake_clock_.SetTimeNanos(rtc::kNumNanosecsPerMillisec * 1001);
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));
  EXPECT_EQ(3U, sent_packets_.size());

  // This time, hit the limit in the middle of the period.
  fake_clock_.SetTimeNanos(rtc::kNumNanosecsPerMillisec * 1500);
  socket_impl_->Send(packet, P2PPacketInfo(dest2_, options, 0));
  EXPECT_EQ(4U, sent_packets_.size());

  // Again, throttling should be active until the next second mark.
  fake_clock_.SetTimeNanos(rtc::kNumNanosecsPerMillisec * 1999);
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));
  socket_impl_->Send(packet, P2PPacketInfo(dest2_, options, 0));
  EXPECT_EQ(4U, sent_packets_.size());
  fake_clock_.SetTimeNanos(rtc::kNumNanosecsPerMillisec * 2002);
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));
  socket_impl_->Send(packet, P2PPacketInfo(dest2_, options, 0));
  EXPECT_EQ(6U, sent_packets_.size());

  base::RunLoop().RunUntilIdle();
}

// Verify that we can open UDP sockets listening in a given port range,
// and fail if all ports in the range are already in use.
TEST_F(P2PSocketUdpTest, PortRangeImplicitPort) {
  const uint16_t min_port = 10000;
  const uint16_t max_port = 10001;
  base::circular_deque<FakeDatagramServerSocket::UDPPacket> sent_packets;
  std::vector<uint16_t> used_ports;
  P2PSocketUdp::DatagramServerSocketFactory fake_socket_factory =
      base::BindRepeating(&CreateFakeDatagramServerSocket, &sent_packets,
                          &used_ports, &fake_clock_);
  P2PMessageThrottler throttler;

  mojo::PendingRemote<mojom::P2PSocketClient> socket_client;
  auto socket_client_receiver = socket_client.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::P2PSocket> socket;
  auto socket_receiver = socket.InitWithNewPipeAndPassReceiver();

  FakeSocketClient fake_client2(std::move(socket),
                                std::move(socket_client_receiver));
  EXPECT_CALL(fake_client2, SocketCreated(_, _)).Times(max_port - min_port + 1);

  for (unsigned port = min_port; port <= max_port; ++port) {
    std::unique_ptr<P2PSocketUdp> socket_impl(new P2PSocketUdp(
        &socket_delegate_, std::move(socket_client), std::move(socket_receiver),
        &throttler, TRAFFIC_ANNOTATION_FOR_TESTS, /*net_log=*/nullptr,
        fake_socket_factory, std::nullopt));
    net::IPEndPoint local_address = ParseAddress(kTestLocalIpAddress, 0);
    socket_impl->Init(
        local_address, min_port, max_port,
        P2PHostAndIPEndPoint(std::string(),
                             ParseAddress(kTestIpAddress1, kTestPort1)),
        net::NetworkAnonymizationKey());

    FakeDatagramServerSocket* datagram_socket =
        GetSocketFromHost(socket_impl.get());
    net::IPEndPoint bound_address;
    datagram_socket->GetLocalAddress(&bound_address);
    EXPECT_EQ(port, bound_address.port());

    base::RunLoop().RunUntilIdle();

    socket_client = socket_impl->ReleaseClientForTesting();
    socket_receiver = socket_impl->ReleaseReceiverForTesting();
  }

  std::unique_ptr<P2PSocketUdp> socket_impl(new P2PSocketUdp(
      &socket_delegate_, std::move(socket_client), std::move(socket_receiver),
      &throttler, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*net_log=*/nullptr, std::move(fake_socket_factory), std::nullopt));
  net::IPEndPoint local_address = ParseAddress(kTestLocalIpAddress, 0);

  auto* socket_impl_ptr = socket_impl.get();
  socket_delegate_.ExpectDestruction(std::move(socket_impl));
  socket_impl_ptr->Init(
      local_address, min_port, max_port,
      P2PHostAndIPEndPoint(std::string(),
                           ParseAddress(kTestIpAddress1, kTestPort1)),
      net::NetworkAnonymizationKey());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_client2.connection_error());
}

// Verify that we can open a UDP socket listening in a given port included in
// a given valid range.
TEST_F(P2PSocketUdpTest, PortRangeExplictValidPort) {
  const uint16_t min_port = 10000;
  const uint16_t max_port = 10001;
  const uint16_t valid_port = min_port;
  base::circular_deque<FakeDatagramServerSocket::UDPPacket> sent_packets;
  std::vector<uint16_t> used_ports;
  P2PSocketUdp::DatagramServerSocketFactory fake_socket_factory =
      base::BindRepeating(&CreateFakeDatagramServerSocket, &sent_packets,
                          &used_ports, &fake_clock_);
  P2PMessageThrottler throttler;

  mojo::PendingRemote<mojom::P2PSocketClient> socket_client;
  mojo::PendingRemote<mojom::P2PSocket> socket;
  auto socket_receiver = socket.InitWithNewPipeAndPassReceiver();

  FakeSocketClient fake_client2(std::move(socket),
                                socket_client.InitWithNewPipeAndPassReceiver());

  EXPECT_CALL(fake_client2, SocketCreated(_, _)).Times(1);

  std::unique_ptr<P2PSocketUdp> socket_host(new P2PSocketUdp(
      &socket_delegate_, std::move(socket_client), std::move(socket_receiver),
      &throttler, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*net_log=*/nullptr, std::move(fake_socket_factory), std::nullopt));
  net::IPEndPoint local_address = ParseAddress(kTestLocalIpAddress, valid_port);
  socket_host->Init(
      local_address, min_port, max_port,
      P2PHostAndIPEndPoint(std::string(),
                           ParseAddress(kTestIpAddress1, kTestPort1)),
      net::NetworkAnonymizationKey());

  FakeDatagramServerSocket* fake_socket = GetSocketFromHost(socket_host.get());
  net::IPEndPoint bound_address;
  fake_socket->GetLocalAddress(&bound_address);
  EXPECT_EQ(local_address.port(), bound_address.port());

  base::RunLoop().RunUntilIdle();
}

// Verify that we cannot open a UDP socket listening in a given port not
// included in a given valid range.
TEST_F(P2PSocketUdpTest, PortRangeExplictInvalidPort) {
  const uint16_t min_port = 10000;
  const uint16_t max_port = 10001;
  const uint16_t invalid_port = max_port + 1;
  base::circular_deque<FakeDatagramServerSocket::UDPPacket> sent_packets;
  std::vector<uint16_t> used_ports;
  P2PSocketUdp::DatagramServerSocketFactory fake_socket_factory =
      base::BindRepeating(&CreateFakeDatagramServerSocket, &sent_packets,
                          &used_ports, &fake_clock_);
  P2PMessageThrottler throttler;

  mojo::PendingRemote<mojom::P2PSocketClient> socket_client;
  mojo::PendingRemote<mojom::P2PSocket> socket;
  auto socket_receiver = socket.InitWithNewPipeAndPassReceiver();

  FakeSocketClient fake_client2(std::move(socket),
                                socket_client.InitWithNewPipeAndPassReceiver());

  auto socket_impl = std::make_unique<P2PSocketUdp>(
      &socket_delegate_, std::move(socket_client), std::move(socket_receiver),
      &throttler, TRAFFIC_ANNOTATION_FOR_TESTS, /*net_log=*/nullptr,
      std::move(fake_socket_factory), std::nullopt);
  net::IPEndPoint local_address =
      ParseAddress(kTestLocalIpAddress, invalid_port);

  auto* socket_impl_ptr = socket_impl.get();
  socket_delegate_.ExpectDestruction(std::move(socket_impl));
  socket_impl_ptr->Init(
      local_address, min_port, max_port,
      P2PHostAndIPEndPoint(std::string(),
                           ParseAddress(kTestIpAddress1, kTestPort1)),
      net::NetworkAnonymizationKey());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_client2.connection_error());
}

// Verify that we can receive packets from the sockets, and that the
// discontinuous packets are not batched.
TEST_F(P2PSocketUdpTest, ReceiveDiscontinuousPackets) {
  // Receive STUN request from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  base::RunLoop().RunUntilIdle();

  // Now we should be able to receive any data from |dest1_|.
  constexpr uint64_t kPacketIntervalNs =
      P2PSocketUdp::kUdpMaxBatchingRecvBuffering.InNanoseconds() / 2;

  std::vector<uint8_t> packet1;
  std::vector<uint8_t> packet2;
  std::vector<uint8_t> packet3;

  CreateRandomPacket(&packet1);
  CreateRandomPacket(&packet2);
  CreateRandomPacket(&packet3);

  InSequence s;
  // The socket returns `ERR_IO_PENDING` in between packets. It
  // indicates no more packets in the socket at that moment. The
  // packet1/packet2/packet3 are regarded as discontinuous.
  // Expect that the discontinuous packets are not batched.
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packet1), _));
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packet2), _));
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packet3), _));

  // Start to receive packets.
  socket_->ReceivePacket(dest1_, packet1);

  fake_clock_.SetTimeNanos(kPacketIntervalNs);
  socket_->ReceivePacket(dest1_, packet2);

  fake_clock_.SetTimeNanos(2 * kPacketIntervalNs);
  socket_->ReceivePacket(dest1_, packet3);

  base::RunLoop().RunUntilIdle();
}

// Verify that we can receive burst packets from the sockets, and that all the
// packets are batched together.
TEST_F(P2PSocketUdpTest, ReceiveBurstPacketsBasic) {
  // Receive STUN request from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  base::RunLoop().RunUntilIdle();

  // Now we should be able to receive any data from |dest1_|.
  constexpr size_t kNumPackets = P2PSocketUdp::kUdpMaxBatchingRecvPackets;

  std::vector<std::vector<uint8_t>> packets(kNumPackets);
  for (size_t i = 0; i < kNumPackets; i++) {
    CreateRandomPacket(&packets[i]);
    socket_->AddRecvPacket(dest1_, packets[i]);
  }

  InSequence s;
  // Expect to receive all the packets in one batching.
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  for (size_t i = 0; i < kNumPackets; i++) {
    EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packets[i]), _));
  }
  // Start to receive burst packets.
  socket_->FireRecvCallback();

  base::RunLoop().RunUntilIdle();
}

// Verify that we can receive burst packets, and that the batching size does not
// exceed limit.
TEST_F(P2PSocketUdpTest, ReceiveBurstPacketsExceedingMaxBatchingSize) {
  // Receive STUN request from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  base::RunLoop().RunUntilIdle();

  // Now we should be able to receive any data from |dest1_|.
  constexpr size_t kNumPacketsExceedingMaxBatching = 3;
  DCHECK_LE(kNumPacketsExceedingMaxBatching,
            P2PSocketUdp::kUdpMaxBatchingRecvPackets);
  constexpr size_t kNumPacketsAll = P2PSocketUdp::kUdpMaxBatchingRecvPackets +
                                    kNumPacketsExceedingMaxBatching;

  std::vector<std::vector<uint8_t>> packets(kNumPacketsAll);
  for (size_t i = 0; i < kNumPacketsAll; i++) {
    CreateRandomPacket(&packets[i]);
    socket_->AddRecvPacket(dest1_, packets[i]);
  }

  InSequence s;
  size_t i = 0;
  // Expect to receive maximum allowed number of packets in the first batching.
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  for (; i < P2PSocketUdp::kUdpMaxBatchingRecvPackets; i++) {
    EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packets[i]), _));
  }
  // Expect to receive the remainder packets in the second batching.
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  for (; i < kNumPacketsAll; i++) {
    EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packets[i]), _));
  }
  // Start to receive burst packets.
  socket_->FireRecvCallback();

  base::RunLoop().RunUntilIdle();
}

// Verify that we can receive burst packets, and that the batching cancels if
// buffering time exceeds limit.
TEST_F(P2PSocketUdpTest, ReceiveBurstPacketsExceedingMaxBatchingBuffering) {
  // Receive STUN request from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  base::RunLoop().RunUntilIdle();

  // Now we should be able to receive any data from |dest1_|.
  constexpr size_t kNumPacketsWithProcessLatency = 16;
  constexpr size_t kNumPacketsExceedingMaximumBuffering = 8;
  constexpr size_t kNumPacketsAll =
      kNumPacketsWithProcessLatency + kNumPacketsExceedingMaximumBuffering;
  DCHECK_LE(kNumPacketsAll, P2PSocketUdp::kUdpMaxBatchingRecvPackets);

  std::vector<std::vector<uint8_t>> packets(kNumPacketsAll);
  for (size_t i = 0; i < kNumPacketsAll; i++) {
    CreateRandomPacket(&packets[i]);
  }

  constexpr uint64_t kMaximumBatchingBufferingNs =
      P2PSocketUdp::kUdpMaxBatchingRecvBuffering.InNanoseconds();
  // Latency of `P2PSocketUdp` to retrieve one packet from socket.
  constexpr uint64_t kPacketProcessLatencyNs =
      kMaximumBatchingBufferingNs / kNumPacketsWithProcessLatency;

  // Add packets with process latency. The total latency does not exceed limit.
  for (size_t i = 0; i < kNumPacketsWithProcessLatency; i++) {
    socket_->AddRecvPacket(dest1_, packets[i], kPacketProcessLatencyNs * i);
  }
  // Add the packet with maximum buffering time plus 1 microsecond, which
  // immediately cancels batching more packets.
  socket_->AddRecvPacket(
      dest1_, packets[kNumPacketsWithProcessLatency],
      kMaximumBatchingBufferingNs + rtc::kNumNanosecsPerMicrosec);
  // Add the remainder packets.
  for (size_t i = kNumPacketsWithProcessLatency + 1; i < kNumPacketsAll; i++) {
    socket_->AddRecvPacket(dest1_, packets[i]);
  }

  InSequence s;
  // Expect to receive the first batching packets.
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  for (size_t i = 0; i < kNumPacketsWithProcessLatency + 1; i++) {
    EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packets[i]), _));
  }
  // Expect to receive the remainder packets in the second batching.
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  for (size_t i = kNumPacketsWithProcessLatency + 1; i < kNumPacketsAll; i++) {
    EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packets[i]), _));
  }
  // Start to receive burst packets.
  socket_->FireRecvCallback();

  base::RunLoop().RunUntilIdle();
}

class P2PSocketUdpWithInterceptorTest : public P2PSocketUdpTest {
 public:
  P2PSocketUdpWithInterceptorTest()
      : P2PSocketUdpTest(base::UnguessableToken::Create()),
        throttling_token_(
            ScopedThrottlingToken::MaybeCreate(net_log_with_source_.source().id,
                                               devtools_token_)) {}

  void SetUp() override {
    SetNetworkState({});
    P2PSocketUdpTest::SetUp();
  }

  void TearDown() override {
    RemoveThrottling();
    P2PSocketUdpTest::TearDown();
  }

  struct NetworkState {
    bool offline = false;
    base::TimeDelta latency;
    double packet_loss = 0.0;
    int packet_queue_length = 0;
  };

  void SetNetworkState(NetworkState state) {
    std::unique_ptr<NetworkConditions> conditions(new NetworkConditions(
        state.offline, state.latency.InMillisecondsF(), 0.0, 0.0,
        state.packet_loss, state.packet_queue_length, false));
    ThrottlingController::SetConditions(*devtools_token_,
                                        std::move(conditions));
  }

  void RemoveThrottling() {
    ThrottlingController::SetConditions(*devtools_token_,
                                        std::unique_ptr<NetworkConditions>());
  }

  void AdvanceClock(base::TimeDelta delta) {
    base::TimeDelta now = base::Nanoseconds(fake_clock_.TimeNanos());
    fake_clock_.SetTimeNanos((now + delta).InNanoseconds());
    task_environment_.FastForwardBy(delta);
  }

 protected:
  std::unique_ptr<network::ScopedThrottlingToken> throttling_token_;
};

TEST_F(P2PSocketUdpWithInterceptorTest, SendPacket) {
  // Receive packet from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  // Now we should be able to send any data to |dest1_|.
  EXPECT_CALL(*fake_client_.get(), SendComplete(_));

  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));

  AdvanceClock(base::Milliseconds(100));

  ASSERT_EQ(1U, sent_packets_.size());
  ASSERT_EQ(dest1_, std::get<0>(sent_packets_[0]));
}

TEST_F(P2PSocketUdpWithInterceptorTest, SendPacketOffline) {
  // Receive packet from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  // Now we should be able to send any data to |dest1_|.
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(2);

  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);

  SetNetworkState({.offline = true});
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));
  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(0U, sent_packets_.size());

  SetNetworkState({});
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 1));
  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(1U, sent_packets_.size());
}

TEST_F(P2PSocketUdpWithInterceptorTest, SendPacketDelayed) {
  // Receive packet from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  // Now we should be able to send any data to |dest1_|.
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(2);

  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);

  SetNetworkState({.latency = base::Milliseconds(1000)});
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));

  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(0U, sent_packets_.size());

  AdvanceClock(base::Milliseconds(2000));
  EXPECT_EQ(1U, sent_packets_.size());

  SetNetworkState({});
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 1));

  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(2U, sent_packets_.size());
}

TEST_F(P2PSocketUdpWithInterceptorTest, SendPacketAndRemoveThrottling) {
  // Receive packet from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  // Now we should be able to send any data to |dest1_|.
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(2);

  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);

  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));

  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(1U, sent_packets_.size());

  RemoveThrottling();

  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));

  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(2U, sent_packets_.size());
}

TEST_F(P2PSocketUdpWithInterceptorTest, SendPacketDropsLongQueue) {
  constexpr size_t kMaxQueueLength = 100;
  SetNetworkState({.packet_queue_length = kMaxQueueLength});

  // Receive packet from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  // Now we should be able to send any data to |dest1_|.
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(500);

  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);

  for (int i = 0; i < 500; ++i) {
    socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));
  }

  AdvanceClock(base::Milliseconds(1000));
  EXPECT_EQ(kMaxQueueLength, sent_packets_.size());
}

TEST_F(P2PSocketUdpWithInterceptorTest, SendPacketWithPacketDrop) {
  // Receive packet from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  EXPECT_CALL(*fake_client_.get(), SendComplete(_)).Times(2);

  socket_->ReceivePacket(dest1_, request_packet);
  AdvanceClock(base::Milliseconds(100));

  rtc::PacketOptions options;
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);

  SetNetworkState({.packet_loss = 100.0});
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 0));
  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(0U, sent_packets_.size());

  SetNetworkState({});
  socket_impl_->Send(packet, P2PPacketInfo(dest1_, options, 1));
  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(1U, sent_packets_.size());
}

TEST_F(P2PSocketUdpWithInterceptorTest, ReceivePackets) {
  // Receive STUN request from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  constexpr size_t kNumPackets = P2PSocketUdp::kUdpMaxBatchingRecvPackets;
  std::vector<std::vector<uint8_t>> packets(kNumPackets);
  for (size_t i = 0; i < kNumPackets; i++) {
    CreateRandomPacket(&packets[i]);
  }

  InSequence s;
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  for (size_t i = 0; i < kNumPackets; i++) {
    EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
    EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packets[i]), _));
  }

  socket_->ReceivePacket(dest1_, request_packet);

  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(1U, received_packets_.size());

  // Now we should be able to receive any data from |dest1_|.
  for (size_t i = 0; i < kNumPackets; i++) {
    socket_->ReceivePacket(dest1_, packets[i]);
  }

  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(kNumPackets + 1U, received_packets_.size());
}

// Verify that we can receive Explicit Congestion Notification (ECN) bits
// from the socket after enabling the socket option, while assuming that
// the sender is sending the ECN bits.
TEST_F(P2PSocketUdpWithInterceptorTest, ReceivePacketsWithEcn) {
  // Receive STUN request from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  constexpr size_t kNumPackets = P2PSocketUdp::kUdpMaxBatchingRecvPackets;
  std::vector<std::vector<uint8_t>> packets(kNumPackets);
  for (size_t i = 0; i < kNumPackets; i++) {
    CreateRandomPacket(&packets[i]);
  }

  InSequence s;
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  for (size_t i = 0; i < kNumPackets; i++) {
    EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
    EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packets[i]), _));
  }

  int desired_recv_ecn = 1;
  socket_->ReceivePacket(dest1_, request_packet);

  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(1U, received_packets_.size());
  // Before setting the ECN receiving option on the socket,
  // it will return the default ECN bits.
  EXPECT_EQ(net::ECN_DEFAULT, socket_->GetLastTos().ecn);

  // Setting the ECN bits receiving option for the socket.
  socket_impl_->SetOption(P2P_SOCKET_OPT_RECV_ECN, desired_recv_ecn);
  // Now we should be able to receive any data from |dest1_| with the ECN bits.
  for (size_t i = 0; i < kNumPackets; i++) {
    socket_->ReceivePacket(dest1_, packets[i]);
    EXPECT_EQ(net::ECN_ECT1, socket_->GetLastTos().ecn);
  }

  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(kNumPackets + 1U, received_packets_.size());
}

TEST_F(P2PSocketUdpWithInterceptorTest, ReceivePacketDelayed) {
  // Receive STUN request from |dest1_|.
  std::vector<uint8_t> request_packet;
  CreateStunRequest(&request_packet);

  InSequence s;
  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(request_packet), _));
  socket_->ReceivePacket(dest1_, request_packet);

  AdvanceClock(base::Milliseconds(100));

  SetNetworkState({.latency = base::Milliseconds(1000)});

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(0);
  std::vector<uint8_t> packet;
  CreateRandomPacket(&packet);
  socket_->ReceivePacket(dest1_, packet);

  AdvanceClock(base::Milliseconds(100));
  EXPECT_EQ(1U, received_packets_.size());

  EXPECT_CALL(*fake_client_.get(), DataReceived(_)).Times(1);
  EXPECT_CALL(*this, SinglePacketReceptionHelper(_, SpanEq(packet), _));
  AdvanceClock(base::Milliseconds(2000));
  EXPECT_EQ(2U, received_packets_.size());
}

}  // namespace network
