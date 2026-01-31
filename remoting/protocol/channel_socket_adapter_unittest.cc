// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_socket_adapter.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "remoting/protocol/p2p_datagram_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/array_view.h"
#include "third_party/webrtc/p2p/base/mock_ice_transport.h"
#include "third_party/webrtc/rtc_base/network/received_packet.h"

using net::IOBufferWithSize;

using testing::_;
using testing::Return;

namespace remoting::protocol {

namespace {
constexpr base::ByteSize kBufferSize = base::KiBU(4);
constexpr uint8_t kTestData[] = {'d', 'a', 't', 'a'};
constexpr base::ByteSize kTestDataSize = base::ByteSize(std::size(kTestData));
const net::Error kTestError = static_cast<net::Error>(-32123);
}  // namespace

class IceTransportForTest : public webrtc::MockIceTransport {
 public:
  // Exposed for testing.
  using webrtc::PacketTransportInternal::NotifyPacketReceived;
};

class TransportChannelSocketAdapterTest : public testing::Test {
 public:
  TransportChannelSocketAdapterTest()
      : callback_(
            base::BindRepeating(&TransportChannelSocketAdapterTest::Callback,
                                base::Unretained(this))) {}

 protected:
  void SetUp() override {
    target_ = std::make_unique<TransportChannelSocketAdapter>(&channel_);
  }

  void Callback(base::expected<base::ByteSize, net::Error> result) {
    callback_result_ = result;
  }

  IceTransportForTest channel_;
  std::unique_ptr<TransportChannelSocketAdapter> target_;
  P2PDatagramSocket::Callback callback_;
  base::expected<base::ByteSize, net::Error> callback_result_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
};

// Verify that Read() returns net::ERR_IO_PENDING.
TEST_F(TransportChannelSocketAdapterTest, Read) {
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize.InBytes());
  base::expected<base::ByteSize, net::Error> result =
      target_->Recv(buffer.get(), kBufferSize, callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result.error());

  channel_.NotifyPacketReceived(webrtc::ReceivedIpPacket(
      webrtc::MakeArrayView(kTestData, kTestDataSize.InBytes()),
      webrtc::SocketAddress()));
  EXPECT_EQ(kTestDataSize, callback_result_);
}

// Verify that Read() after Close() returns error.
TEST_F(TransportChannelSocketAdapterTest, ReadClose) {
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize.InBytes());
  base::expected<base::ByteSize, net::Error> result =
      target_->Recv(buffer.get(), kBufferSize, callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result.error());

  target_->Close(kTestError);
  EXPECT_EQ(kTestError, callback_result_.error());

  // All Recv() calls after Close() should return the error.
  EXPECT_EQ(base::unexpected(kTestError),
            target_->Recv(buffer.get(), kBufferSize, callback_));
}

// Verify that Send sends the packet and returns correct result.
TEST_F(TransportChannelSocketAdapterTest, Send) {
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(kTestDataSize.InBytes());

  EXPECT_CALL(channel_, writable()).WillOnce(Return(true));
  EXPECT_CALL(channel_,
              SendPacket(buffer->data(), kTestDataSize.InBytes(), _, 0))
      .WillOnce(Return(kTestDataSize.InBytes()));

  base::expected<base::ByteSize, net::Error> result =
      target_->Send(buffer.get(), kTestDataSize, callback_);
  EXPECT_EQ(kTestDataSize, result);
}

// Verify that the message is still sent if Send() is called while
// socket is not open yet. The result is the packet is lost.
TEST_F(TransportChannelSocketAdapterTest, SendPending) {
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(kTestDataSize.InBytes());

  EXPECT_CALL(channel_, writable()).WillOnce(Return(true));
  EXPECT_CALL(channel_,
              SendPacket(buffer->data(), kTestDataSize.InBytes(), _, 0))
      .Times(1)
      .WillOnce(Return(SOCKET_ERROR));

  EXPECT_CALL(channel_, GetError()).WillOnce(Return(EWOULDBLOCK));

  base::expected<base::ByteSize, net::Error> result =
      target_->Send(buffer.get(), kTestDataSize, callback_);
  ASSERT_EQ(base::ByteSize(0), result);
}

}  // namespace remoting::protocol
