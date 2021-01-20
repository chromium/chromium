// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/chromium_socket_factory.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"
#include "third_party/webrtc/rtc_base/socket_address.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace remoting {
namespace protocol {

namespace {

class ConstantScopedFakeClock : public rtc::ClockInterface {
 public:
  ConstantScopedFakeClock() { prev_clock_ = rtc::SetClockForTesting(this); }
  ~ConstantScopedFakeClock() override { rtc::SetClockForTesting(prev_clock_); }

  int64_t TimeNanos() const override { return 1337L * 1000L * 1000L; }

 private:
  ClockInterface* prev_clock_;
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
    socket_->SignalReadPacket.connect(
        this, &ChromiumSocketFactoryTest::OnPacket);
  }

  void OnPacket(rtc::AsyncPacketSocket* socket,
                const char* data,
                size_t size,
                const rtc::SocketAddress& address,
                const int64_t& packet_time) {
    EXPECT_EQ(socket, socket_.get());
    last_packet_.assign(data, data + size);
    last_address_ = address;
    last_packet_time_ = packet_time;
    run_loop_.Quit();
  }

  void OnSentPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::SentPacket& sent_packet) {
    // It is expected that send_packet was set using rtc::TimeMillis(),
    // which will use the fake clock set above, so the times will be equal
    int64_t fake_clock_ms = rtc::TimeMillis();
    EXPECT_EQ(fake_clock_ms, sent_packet.send_time_ms);
  }

  void VerifyCanSendAndReceive(rtc::AsyncPacketSocket* sender) {
    // UDP packets may be lost, so we have to retry sending it more than once.
    const int kMaxAttempts = 3;
    const base::TimeDelta kAttemptPeriod = base::TimeDelta::FromSeconds(1);
    std::string test_packet("TEST PACKET");
    int attempts = 0;
    rtc::PacketOptions options;
    while (last_packet_.empty() && attempts++ < kMaxAttempts) {
      sender->SendTo(test_packet.data(), test_packet.size(),
                     socket_->GetLocalAddress(), options);
      task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
          FROM_HERE, run_loop_.QuitClosure(), kAttemptPeriod);
      run_loop_.Run();
    }
    EXPECT_EQ(test_packet, last_packet_);
    EXPECT_EQ(sender->GetLocalAddress(), last_address_);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::RunLoop run_loop_;

  std::unique_ptr<rtc::PacketSocketFactory> socket_factory_;
  std::unique_ptr<rtc::AsyncPacketSocket> socket_;

  std::string last_packet_;
  rtc::SocketAddress last_address_;
  int64_t last_packet_time_;

  ConstantScopedFakeClock fake_clock_;
};

TEST_F(ChromiumSocketFactoryTest, SendAndReceive) {
  std::unique_ptr<rtc::AsyncPacketSocket> sending_socket(
      socket_factory_->CreateUdpSocket(rtc::SocketAddress("127.0.0.1", 0), 0,
                                       0));
  ASSERT_TRUE(sending_socket.get() != nullptr);
  EXPECT_EQ(sending_socket->GetState(),
            rtc::AsyncPacketSocket::STATE_BOUND);

  VerifyCanSendAndReceive(sending_socket.get());
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
    ASSERT_EQ(assigned_ports.end(), assigned_ports.find(port));
    assigned_ports.insert(port);
  }

  // Try to create another socket, which should fail because no more port is
  // available.
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
                         rtc::SocketAddress("::1", 0),
                         rtc::PacketOptions());

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

}  // namespace protocol
}  // namespace remoting
