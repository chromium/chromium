// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/udp_transport_impl.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/udp_packet_pipe.h"
#include "media/cast/test/utility/net_utility.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {

class MockPacketReceiver final : public UdpTransportReceiver {
 public:
  MockPacketReceiver(const base::RepeatingClosure& callback)
      : packet_callback_(callback) {}

  bool ReceivedPacket(std::unique_ptr<Packet> packet) {
    packet_ = std::move(packet);
    packet_callback_.Run();
    return true;
  }

  // UdpTransportReceiver implementation.
  void OnPacketReceived(const std::vector<uint8_t>& packet) override {
    EXPECT_GT(packet.size(), 0u);
    packet_.reset(new Packet(packet));
    packet_callback_.Run();
  }

  PacketReceiverCallbackWithStatus packet_receiver() {
    return base::BindRepeating(&MockPacketReceiver::ReceivedPacket,
                               base::Unretained(this));
  }

  std::unique_ptr<Packet> TakePacket() { return std::move(packet_); }

 private:
  base::RepeatingClosure packet_callback_;
  std::unique_ptr<Packet> packet_;

  DISALLOW_COPY_AND_ASSIGN(MockPacketReceiver);
};

void SendPacket(UdpTransportImpl* transport, Packet packet) {
  base::Closure cb;
  transport->SendPacket(new base::RefCountedData<Packet>(packet), cb);
}

static void UpdateCastTransportStatus(CastTransportStatus status) {
  NOTREACHED();
}

}  // namespace

class UdpTransportImplTest : public ::testing::Test {
 public:
  UdpTransportImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    net::IPEndPoint free_local_port1 = test::GetFreeLocalPort();
    net::IPEndPoint free_local_port2 = test::GetFreeLocalPort();

    send_transport_ = std::make_unique<UdpTransportImpl>(
        task_environment_.GetMainThreadTaskRunner(), free_local_port1,
        free_local_port2, base::BindRepeating(&UpdateCastTransportStatus));
    send_transport_->SetSendBufferSize(65536);

    recv_transport_ = std::make_unique<UdpTransportImpl>(
        task_environment_.GetMainThreadTaskRunner(), free_local_port2,
        free_local_port1, base::BindRepeating(&UpdateCastTransportStatus));
    recv_transport_->SetSendBufferSize(65536);
  }

  ~UdpTransportImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<UdpTransportImpl> send_transport_;

  // A receiver side transport to receiver/send packets from/to sender.
  std::unique_ptr<UdpTransportImpl> recv_transport_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UdpTransportImplTest);
};

// Test the sending/receiving functions as a PacketSender.
TEST_F(UdpTransportImplTest, PacketSenderSendAndReceive) {
  std::string data = "Test";
  Packet packet(data.begin(), data.end());

  base::RunLoop run_loop;
  MockPacketReceiver packet_receiver_on_sender(run_loop.QuitClosure());
  MockPacketReceiver packet_receiver_on_receiver(
      base::BindRepeating(&SendPacket, recv_transport_.get(), packet));
  send_transport_->StartReceiving(packet_receiver_on_sender.packet_receiver());
  recv_transport_->StartReceiving(
      packet_receiver_on_receiver.packet_receiver());

  base::Closure cb;
  SendPacket(send_transport_.get(), packet);
  run_loop.Run();
  std::unique_ptr<Packet> received_packet =
      packet_receiver_on_sender.TakePacket();
  EXPECT_TRUE(received_packet);
  EXPECT_TRUE(
      std::equal(packet.begin(), packet.end(), received_packet->begin()));
  received_packet = packet_receiver_on_receiver.TakePacket();
  EXPECT_TRUE(received_packet);
  EXPECT_TRUE(
      std::equal(packet.begin(), packet.end(), (*received_packet).begin()));
}

// Test the sending/receiving functions as a UdpTransport.
TEST_F(UdpTransportImplTest, UdpTransportSendAndReceive) {
  std::string data = "Hello!";
  Packet packet(data.begin(), data.end());

  base::RunLoop run_loop;
  MockPacketReceiver packet_receiver_on_sender(run_loop.QuitClosure());
  MockPacketReceiver packet_receiver_on_receiver(
      base::BindRepeating(&SendPacket, recv_transport_.get(), packet));
  send_transport_->StartReceiving(&packet_receiver_on_sender);
  recv_transport_->StartReceiving(
      packet_receiver_on_receiver.packet_receiver());

  mojo::DataPipe data_pipe(5);
  send_transport_->StartSending(std::move(data_pipe.consumer_handle));
  UdpPacketPipeWriter writer(std::move(data_pipe.producer_handle));
  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run()).Times(1);
  writer.Write(new base::RefCountedData<Packet>(packet), done_callback.Get());
  run_loop.Run();
  std::unique_ptr<Packet> received_packet =
      packet_receiver_on_sender.TakePacket();
  EXPECT_TRUE(received_packet);
  EXPECT_TRUE(
      std::equal(packet.begin(), packet.end(), received_packet->begin()));
  received_packet = packet_receiver_on_receiver.TakePacket();
  EXPECT_TRUE(received_packet);
  EXPECT_TRUE(
      std::equal(packet.begin(), packet.end(), (*received_packet).begin()));
}

}  // namespace cast
}  // namespace media
