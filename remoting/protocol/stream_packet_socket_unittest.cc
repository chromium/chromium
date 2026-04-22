// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/stream_packet_socket.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/protocol/stream_packet_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"

using testing::_;
using testing::Return;

namespace remoting::protocol {

namespace {

class FakeStreamPacketProcessor : public StreamPacketProcessor {
 public:
  FakeStreamPacketProcessor() = default;
  ~FakeStreamPacketProcessor() override = default;

  scoped_refptr<net::IOBufferWithSize> Pack(
      base::span<const uint8_t> data) const override {
    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(data.size());
    buffer->span().copy_from(data);
    return buffer;
  }

  scoped_refptr<net::IOBufferWithSize> Unpack(
      base::span<const uint8_t> data,
      size_t* bytes_consumed) const override {
    *bytes_consumed = data.size();
    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(data.size());
    buffer->span().copy_from(data);
    return buffer;
  }
};

class MockStreamSocket : public net::StreamSocket {
 public:
  MockStreamSocket() = default;
  ~MockStreamSocket() override = default;

  MOCK_METHOD(int,
              Read,
              (net::IOBuffer*, int, net::CompletionOnceCallback),
              (override));
  MOCK_METHOD(int,
              Write,
              (net::IOBuffer*,
               int,
               net::CompletionOnceCallback,
               const net::NetworkTrafficAnnotationTag&),
              (override));
  MOCK_METHOD(int, SetReceiveBufferSize, (int32_t), (override));
  MOCK_METHOD(int, SetSendBufferSize, (int32_t), (override));
  MOCK_METHOD(int, Connect, (net::CompletionOnceCallback), (override));
  MOCK_METHOD(void, Disconnect, (), (override));
  MOCK_METHOD(bool, IsConnected, (), (const, override));
  MOCK_METHOD(bool, IsConnectedAndIdle, (), (const, override));
  MOCK_METHOD(int, GetPeerAddress, (net::IPEndPoint*), (const, override));
  MOCK_METHOD(int, GetLocalAddress, (net::IPEndPoint*), (const, override));
  MOCK_METHOD(const net::NetLogWithSource&, NetLog, (), (const, override));
  MOCK_METHOD(bool, WasEverUsed, (), (const, override));
  MOCK_METHOD(net::NextProto, GetNegotiatedProtocol, (), (const, override));
  MOCK_METHOD(bool, GetSSLInfo, (net::SSLInfo*), (override));
  MOCK_METHOD(int64_t, GetTotalReceivedBytes, (), (const, override));
  MOCK_METHOD(void, ApplySocketTag, (const net::SocketTag&), (override));
};

}  // namespace

class StreamPacketSocketTest : public testing::Test {
 public:
  StreamPacketSocketTest() = default;
  ~StreamPacketSocketTest() override = default;

 protected:
  void SetUp() override {
    socket_ = std::make_unique<StreamPacketSocket>();
    auto mock_socket =
        std::make_unique<testing::StrictMock<MockStreamSocket>>();
    mock_stream_socket_ = mock_socket.get();
    mock_socket_ptr_ = std::move(mock_socket);
  }

  void TearDown() override {
    mock_stream_socket_ = nullptr;
    socket_.reset();
  }

  void InitSocket() {
    EXPECT_CALL(*mock_stream_socket_, Connect(_)).WillOnce(Return(net::OK));
    EXPECT_CALL(*mock_stream_socket_, Read(_, _, _))
        .WillOnce(Return(net::ERR_IO_PENDING));
    EXPECT_TRUE(
        socket_->Init(std::move(mock_socket_ptr_), &fake_packet_processor_));
  }

  FakeStreamPacketProcessor fake_packet_processor_;
  std::unique_ptr<StreamPacketSocket> socket_;
  std::unique_ptr<testing::StrictMock<MockStreamSocket>> mock_socket_ptr_;
  // Owned by socket_ after InitSocket().
  raw_ptr<testing::StrictMock<MockStreamSocket>> mock_stream_socket_ = nullptr;
};

TEST_F(StreamPacketSocketTest, SetOptionUninitialized) {
  // Should return -1 instead of crashing.
  EXPECT_EQ(socket_->SetOption(webrtc::Socket::OPT_NODELAY, 1), -1);
  EXPECT_EQ(socket_->SetOption(webrtc::Socket::OPT_DSCP, 1), -1);
}

TEST_F(StreamPacketSocketTest, SetOptionInitialized) {
  InitSocket();
  EXPECT_CALL(*mock_stream_socket_, SetReceiveBufferSize(4096))
      .WillOnce(Return(net::OK));
  EXPECT_EQ(socket_->SetOption(webrtc::Socket::OPT_RCVBUF, 4096), 0);

  EXPECT_CALL(*mock_stream_socket_, SetSendBufferSize(4096))
      .WillOnce(Return(net::OK));
  EXPECT_EQ(socket_->SetOption(webrtc::Socket::OPT_SNDBUF, 4096), 0);

  EXPECT_EQ(socket_->SetOption(webrtc::Socket::OPT_NODELAY, 1), -1);
}

TEST_F(StreamPacketSocketTest, SendFailsWhenNotConnected) {
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  const char kData[] = "test";
  EXPECT_EQ(
      socket_->Send(kData, sizeof(kData), webrtc::AsyncSocketPacketOptions()),
      -1);
  EXPECT_EQ(socket_->GetError(), ENOTCONN);
}

TEST_F(StreamPacketSocketTest, SendToFailsWithAddressMismatch) {
  InitSocket();
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CONNECTED);

  net::IPEndPoint peer_endpoint(net::IPAddress(192, 168, 1, 1), 1234);
  EXPECT_CALL(*mock_stream_socket_, GetPeerAddress(_))
      .WillRepeatedly([&](net::IPEndPoint* address) {
        *address = peer_endpoint;
        return net::OK;
      });

  webrtc::SocketAddress mismatched_address("10.0.0.1", 4321);
  const char kData[] = "test";
  EXPECT_EQ(socket_->SendTo(kData, sizeof(kData), mismatched_address,
                            webrtc::AsyncSocketPacketOptions()),
            -1);
  EXPECT_EQ(socket_->GetError(), ENOTCONN);
}

TEST_F(StreamPacketSocketTest, SendLargePacketFails) {
  InitSocket();
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CONNECTED);

  // Packet size exceeds kMaxSendBufferSize (65536)
  std::vector<char> large_data(65537, 'a');
  EXPECT_EQ(socket_->Send(large_data.data(), large_data.size(),
                          webrtc::AsyncSocketPacketOptions()),
            -1);
  EXPECT_EQ(socket_->GetError(), EMSGSIZE);
}

TEST_F(StreamPacketSocketTest, SendSuccess) {
  InitSocket();
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CONNECTED);

  const char kData[] = "test";
  size_t data_size = sizeof(kData);

  // We expect Write to be called once.
  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .WillOnce([](net::IOBuffer* buf, int buf_len,
                   net::CompletionOnceCallback callback,
                   const net::NetworkTrafficAnnotationTag&) {
        return buf_len;  // Return number of bytes written
      });

  EXPECT_EQ(socket_->Send(kData, data_size, webrtc::AsyncSocketPacketOptions()),
            static_cast<int>(data_size));
}

TEST_F(StreamPacketSocketTest, Close) {
  InitSocket();
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CONNECTED);

  mock_stream_socket_ = nullptr;
  socket_->Close();
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  // SetOption should return -1 on closed socket.
  EXPECT_EQ(socket_->SetOption(webrtc::Socket::OPT_RCVBUF, 4096), -1);
}

}  // namespace remoting::protocol
