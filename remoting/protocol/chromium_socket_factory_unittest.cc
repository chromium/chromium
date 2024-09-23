// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/protocol/chromium_socket_factory.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"
#include "third_party/webrtc/rtc_base/network/received_packet.h"
#include "third_party/webrtc/rtc_base/socket_address.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace remoting::protocol {

namespace {

// UDP packets may be lost, so we have to retry sending it more than once.
constexpr int kMaxAttempts = 3;

// The amount of time to wait for packets to be received for each attempt.
constexpr base::TimeDelta kAttemptPeriod = base::Seconds(5);

class ConstantScopedFakeClock : public rtc::ClockInterface {
 public:
  ConstantScopedFakeClock() { prev_clock_ = rtc::SetClockForTesting(this); }
  ~ConstantScopedFakeClock() override { rtc::SetClockForTesting(prev_clock_); }

  int64_t TimeNanos() const override { return 1337L * 1000L * 1000L; }

 private:
  raw_ptr<ClockInterface> prev_clock_;
};

}  // namespace

class ChromiumSocketFactoryTest : public testing::Test,
                                  public sigslot::has_slots<> {
 public:
  void SetUp() override {
    socket_factory_ = std::make_unique<ChromiumPacketSocketFactory>(nullptr);

    socket_.reset(socket_factory_->CreateUdpSocket(
        rtc::SocketAddress("127.0.0.1", 0), 0, 0));
    ASSERT_TRUE(socket_.get() != nullptr);
    EXPECT_EQ(socket_->GetState(), rtc::AsyncPacketSocket::STATE_BOUND);
    socket_->RegisterReceivedPacketCallback(
        [&](rtc::AsyncPacketSocket* socket, const rtc::ReceivedPacket& packet) {
          OnPacket(socket, packet);
        });
  }

  void OnPacket(rtc::AsyncPacketSocket* socket,
                const rtc::ReceivedPacket& packet) {
    EXPECT_EQ(socket, socket_.get());

    received_packets_.push_back(
        {packet.payload().data(),
         packet.payload().data() + packet.payload().size()});
    last_address_ = packet.source_address();
    last_packet_time_ = packet.arrival_time()->us();
    if (received_packets_.size() >= expected_packet_count_) {
      run_loop_.Quit();
    }
  }

  void OnSentPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::SentPacket& sent_packet) {
    // It is expected that send_packet was set using rtc::TimeMillis(),
    // which will use the fake clock set above, so the times will be equal
    int64_t fake_clock_ms = rtc::TimeMillis();
    EXPECT_EQ(fake_clock_ms, sent_packet.send_time_ms);
  }

  void VerifyCanSendAndReceive(rtc::AsyncPacketSocket* sender,
                               uint32_t packet_count = 1) {
    CHECK_GT(packet_count, 0U);

    base::flat_set<std::string> packets_to_send;
    packets_to_send.reserve(packet_count);
    for (uint32_t i = 0; i < packet_count; i++) {
      packets_to_send.insert("TEST PACKET " + base::NumberToString(i));
    }

    int attempts = 0;
    while (!packets_to_send.empty() && attempts++ < kMaxAttempts) {
      LOG(INFO) << "ATTEMPT " << attempts;
      // Reset members to prepare to send/receive the expected number of packets
      expected_packet_count_ = packets_to_send.size();
      received_packets_.clear();

      rtc::PacketOptions options;
      LOG(INFO) << "packets_to_send: " << packets_to_send.size();
      for (const auto& test_packet : packets_to_send) {
        int result = sender->SendTo(test_packet.data(), test_packet.size(),
                                    socket_->GetLocalAddress(), options);
        PLOG_IF(WARNING, result < 0) << "Failed to send packet";
      }

      task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
          FROM_HERE, run_loop_.QuitClosure(), kAttemptPeriod);
      run_loop_.Run();

      LOG(INFO) << "received_packets_: " << received_packets_.size();
      for (const auto& packet_data : received_packets_) {
        packets_to_send.erase(packet_data);
      }
    }

    // Verify all packets were sent and received. Use EQ check so the number of
    // packets which were not sent will be included in the error message.
    EXPECT_EQ(packets_to_send.size(), 0U);
    EXPECT_EQ(sender->GetLocalAddress(), last_address_);
  }

  void VerifyCanSendAndReceive(rtc::AsyncPacketSocket* sender,
                               const std::string& packet_data) {
    // Reset members to prepare to send/receive the expected number of packets.
    expected_packet_count_ = 1;
    received_packets_.clear();

    int attempts = 0;
    while (received_packets_.empty() && attempts++ < kMaxAttempts) {
      rtc::PacketOptions options;
      int result = sender->SendTo(packet_data.data(), packet_data.size(),
                                  socket_->GetLocalAddress(), options);
      PLOG_IF(WARNING, result < 0) << "Failed to send packet";
      task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
          FROM_HERE, run_loop_.QuitClosure(), kAttemptPeriod);
      run_loop_.Run();
    }
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::RunLoop run_loop_;

  std::unique_ptr<rtc::PacketSocketFactory> socket_factory_;
  std::unique_ptr<rtc::AsyncPacketSocket> socket_;

  uint32_t expected_packet_count_;
  std::vector<std::string> received_packets_;
  rtc::SocketAddress last_address_;
  int64_t last_packet_time_;

  ConstantScopedFakeClock fake_clock_;
};

TEST_F(ChromiumSocketFactoryTest, SendAndReceiveOnePacket) {
  std::unique_ptr<rtc::AsyncPacketSocket> sending_socket(
      socket_factory_->CreateUdpSocket(rtc::SocketAddress("127.0.0.1", 0), 0,
                                       0));
  ASSERT_TRUE(sending_socket.get() != nullptr);
  EXPECT_EQ(sending_socket->GetState(), rtc::AsyncPacketSocket::STATE_BOUND);

  VerifyCanSendAndReceive(sending_socket.get());
}

TEST_F(ChromiumSocketFactoryTest, SendAndReceiveOneLargePacket) {
  std::unique_ptr<rtc::AsyncPacketSocket> sending_socket(
      socket_factory_->CreateUdpSocket(rtc::SocketAddress("127.0.0.1", 0), 0,
                                       0));
  ASSERT_TRUE(sending_socket.get() != nullptr);
  EXPECT_EQ(sending_socket->GetState(), rtc::AsyncPacketSocket::STATE_BOUND);

  std::string packet_data(1000, 'a');
  VerifyCanSendAndReceive(sending_socket.get(), packet_data);
}

TEST_F(ChromiumSocketFactoryTest, SendAndReceiveManyPackets) {
  std::unique_ptr<rtc::AsyncPacketSocket> sending_socket(
      socket_factory_->CreateUdpSocket(rtc::SocketAddress("127.0.0.1", 0), 0,
                                       0));
  ASSERT_TRUE(sending_socket.get() != nullptr);
  EXPECT_EQ(sending_socket->GetState(), rtc::AsyncPacketSocket::STATE_BOUND);

  VerifyCanSendAndReceive(sending_socket.get(), 100);
}

TEST_F(ChromiumSocketFactoryTest, SetOptions) {
  EXPECT_EQ(0, socket_->SetOption(rtc::Socket::OPT_SNDBUF, 4096));
  EXPECT_EQ(0, socket_->SetOption(rtc::Socket::OPT_RCVBUF, 4096));
}

TEST_F(ChromiumSocketFactoryTest, PortRange) {
  constexpr uint16_t kMinPort = 12400;
  constexpr uint16_t kMaxPort = 12410;
  socket_.reset(socket_factory_->CreateUdpSocket(
      rtc::SocketAddress("127.0.0.1", 0), kMinPort, kMaxPort));
  ASSERT_TRUE(socket_.get() != nullptr);
  EXPECT_EQ(socket_->GetState(), rtc::AsyncPacketSocket::STATE_BOUND);
  EXPECT_GE(socket_->GetLocalAddress().port(), kMinPort);
  EXPECT_LE(socket_->GetLocalAddress().port(), kMaxPort);
}

TEST_F(ChromiumSocketFactoryTest, CreateMultiplePortsFromPortRange) {
  constexpr uint16_t kPortCount = 5;
  constexpr uint16_t kMinPort = 12400;
  constexpr uint16_t kMaxPort = kMinPort + kPortCount - 1;
  std::vector<std::unique_ptr<rtc::AsyncPacketSocket>> sockets;
  for (int i = 0; i < kPortCount; i++) {
    sockets.push_back(std::unique_ptr<rtc::AsyncPacketSocket>(
        socket_factory_->CreateUdpSocket(rtc::SocketAddress("127.0.0.1", 0),
                                         kMinPort, kMaxPort)));
  }
  base::flat_set<uint16_t> assigned_ports;
  for (auto& socket : sockets) {
    ASSERT_TRUE(socket.get() != nullptr);
    EXPECT_EQ(socket->GetState(), rtc::AsyncPacketSocket::STATE_BOUND);
    uint16_t port = socket->GetLocalAddress().port();
    EXPECT_GE(port, kMinPort);
    EXPECT_LE(port, kMaxPort);
    ASSERT_FALSE(base::Contains(assigned_ports, port));
    assigned_ports.insert(port);
  }

  // Create another socket should fail because no ports are available.
  auto* extra_socket = socket_factory_->CreateUdpSocket(
      rtc::SocketAddress("127.0.0.1", 0), kMinPort, kMaxPort);
  ASSERT_EQ(nullptr, extra_socket);
}

TEST_F(ChromiumSocketFactoryTest, TransientError) {
  std::unique_ptr<rtc::AsyncPacketSocket> sending_socket(
      socket_factory_->CreateUdpSocket(rtc::SocketAddress("127.0.0.1", 0), 0,
                                       0));
  std::string test_packet("TEST");

  // Try sending a packet to an IPv6 address from a socket that's bound to an
  // IPv4 address. This send is expected to fail, but the socket should still be
  // functional.
  sending_socket->SendTo(test_packet.data(), test_packet.size(),
                         rtc::SocketAddress("::1", 0), rtc::PacketOptions());

  // Verify that socket is still usable.
  VerifyCanSendAndReceive(sending_socket.get());
}

TEST_F(ChromiumSocketFactoryTest, CheckSendTime) {
  std::unique_ptr<rtc::AsyncPacketSocket> sending_socket(
      socket_factory_->CreateUdpSocket(rtc::SocketAddress("127.0.0.1", 0), 0,
                                       0));
  sending_socket->SignalSentPacket.connect(
      static_cast<ChromiumSocketFactoryTest*>(this),
      &ChromiumSocketFactoryTest::OnSentPacket);
  VerifyCanSendAndReceive(sending_socket.get());

  // Check receive time is from rtc clock as well
  ASSERT_EQ(last_packet_time_, rtc::TimeMicros());
}

}  // namespace remoting::protocol
