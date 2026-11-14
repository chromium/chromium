// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/stream_packet_socket.h"

#include <errno.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
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

constexpr size_t kLengthHeaderSize = sizeof(uint16_t);
constexpr int kSocketBufferSize = 4096;
// Must match `kMaxSendBufferSize` in stream_packet_socket.cc.
constexpr size_t kMaxSendBufferSize = 65536;
constexpr int kPartialWriteBytes = 2;

constexpr net::IPAddress kDefaultPeerIp(192, 0, 2, 1);
constexpr char kDefaultPeerIpString[] = "192.0.2.1";
constexpr uint16_t kDefaultPeerPort = 3478;

constexpr net::IPAddress kOtherPeerIp(192, 168, 1, 1);
constexpr uint16_t kOtherPeerPort = 1234;
constexpr char kMismatchedIpString[] = "10.0.0.1";
constexpr uint16_t kMismatchedPort = 4321;

constexpr char kTestData[] = "test";
constexpr char kFirstPacket[] = "first";
constexpr char kSecondPacket[] = "second";
constexpr char kFollowUpPacket[] = "next";
constexpr char kReplyPacket[] = "reply";
constexpr char kPacketOne[] = "one";
constexpr char kPacketTwo[] = "two";

// Incomplete length-prefixed frame (declares a 16-byte payload, supplies 1).
constexpr std::array<uint8_t, 3> kIncompleteFrame = {0x00, 0x10, 0xaa};

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

class LengthPrefixedPacketProcessor : public StreamPacketProcessor {
 public:
  LengthPrefixedPacketProcessor() = default;
  ~LengthPrefixedPacketProcessor() override = default;

  scoped_refptr<net::IOBufferWithSize> Pack(
      base::span<const uint8_t> data) const override {
    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(
        kLengthHeaderSize + data.size());
    base::SpanWriter writer(buffer->span());
    writer.WriteU16BigEndian(static_cast<uint16_t>(data.size()));
    writer.Write(data);
    return buffer;
  }

  scoped_refptr<net::IOBufferWithSize> Unpack(
      base::span<const uint8_t> data,
      size_t* bytes_consumed) const override {
    *bytes_consumed = 0;
    if (data.size() < kLengthHeaderSize) {
      return nullptr;
    }
    uint16_t payload_len =
        (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
    if (data.size() < kLengthHeaderSize + payload_len) {
      return nullptr;
    }
    *bytes_consumed = kLengthHeaderSize + payload_len;
    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(payload_len);
    buffer->span().copy_from(data.subspan(kLengthHeaderSize, payload_len));
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

// `webrtc::AsyncPacketSocket::SubscribeReadyToSend()` is defined inline in
// `async_packet_socket.h` and instantiates
// `CallbackListReceivers::AddReceiver<NontrivialUntypedFunctionArgs>`, which is
// not exported across the `webrtc_component` shared library boundary in
// component builds. In component builds `ExpectCount()` is a no-op, so
// `ReadyToSend` counts are only verified in non-component builds.
// Must not outlive `socket`.
// TODO(crbug.com/584004657): Remove once
// AsyncPacketSocket::SubscribeReadyToSend() is defined out-of-line upstream
// (like SubscribeSentPacket()).
class ReadyToSendCounter {
 public:
  explicit ReadyToSendCounter(
      [[maybe_unused]] webrtc::AsyncPacketSocket* socket) {
#if !defined(COMPONENT_BUILD)
    socket_ = socket;
    socket_->SubscribeReadyToSend(
        this, [this](webrtc::AsyncPacketSocket*) { ++count_; });
#endif
  }

  ~ReadyToSendCounter() {
#if !defined(COMPONENT_BUILD)
    socket_->UnsubscribeReadyToSend(this);
#endif
  }

  ReadyToSendCounter(const ReadyToSendCounter&) = delete;
  ReadyToSendCounter& operator=(const ReadyToSendCounter&) = delete;

  void ExpectCount([[maybe_unused]] int expected) const {
#if !defined(COMPONENT_BUILD)
    EXPECT_EQ(count_, expected);
#endif
  }

 private:
#if !defined(COMPONENT_BUILD)
  raw_ptr<webrtc::AsyncPacketSocket> socket_ = nullptr;
  int count_ = 0;
#endif
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

  // Drains tasks already posted to the current sequence without calling
  // RunUntilIdle(), which is flagged by Chromium presubmit checks.
  void FlushCurrentTaskQueue() {
    base::RunLoop loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void InitSocket(StreamPacketProcessor* processor = nullptr) {
    if (!processor) {
      processor = &fake_packet_processor_;
    }
    EXPECT_CALL(*mock_stream_socket_, Connect(_)).WillOnce(Return(net::OK));
    EXPECT_CALL(*mock_stream_socket_, Read(_, _, _))
        .WillOnce(Return(net::ERR_IO_PENDING));
    EXPECT_TRUE(socket_->Init(std::move(mock_socket_ptr_), processor));
  }

  void InitSocketWithPendingRead(StreamPacketProcessor* processor,
                                 scoped_refptr<net::IOBuffer>* read_buf,
                                 net::CompletionOnceCallback* read_cb) {
    EXPECT_CALL(*mock_stream_socket_, Connect(_)).WillOnce(Return(net::OK));
    EXPECT_CALL(*mock_stream_socket_, Read(_, _, _))
        .WillOnce([read_buf, read_cb](net::IOBuffer* buf, int,
                                      net::CompletionOnceCallback callback) {
          *read_buf = buf;
          *read_cb = std::move(callback);
          return net::ERR_IO_PENDING;
        });
    ASSERT_TRUE(socket_->Init(std::move(mock_socket_ptr_), processor));
    ASSERT_TRUE(*read_buf);
    ASSERT_FALSE(read_cb->is_null());
  }

  void ExpectPeerAddress(const net::IPEndPoint& peer_endpoint =
                             net::IPEndPoint(kDefaultPeerIp,
                                             kDefaultPeerPort)) {
    EXPECT_CALL(*mock_stream_socket_, GetPeerAddress(_))
        .WillRepeatedly([peer_endpoint](net::IPEndPoint* address) {
          *address = peer_endpoint;
          return net::OK;
        });
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeStreamPacketProcessor fake_packet_processor_;
  LengthPrefixedPacketProcessor length_prefixed_processor_;
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
  EXPECT_CALL(*mock_stream_socket_, SetReceiveBufferSize(kSocketBufferSize))
      .WillOnce(Return(net::OK));
  EXPECT_EQ(socket_->SetOption(webrtc::Socket::OPT_RCVBUF, kSocketBufferSize),
            0);

  EXPECT_CALL(*mock_stream_socket_, SetSendBufferSize(kSocketBufferSize))
      .WillOnce(Return(net::OK));
  EXPECT_EQ(socket_->SetOption(webrtc::Socket::OPT_SNDBUF, kSocketBufferSize),
            0);

  EXPECT_EQ(socket_->SetOption(webrtc::Socket::OPT_NODELAY, 1), -1);
}

TEST_F(StreamPacketSocketTest, SendFailsWhenNotConnected) {
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  EXPECT_EQ(socket_->Send(kTestData, sizeof(kTestData),
                          webrtc::AsyncSocketPacketOptions()),
            -1);
  EXPECT_EQ(socket_->GetError(), ENOTCONN);
}

TEST_F(StreamPacketSocketTest, SendToFailsWithAddressMismatch) {
  InitSocket();
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CONNECTED);

  ExpectPeerAddress(net::IPEndPoint(kOtherPeerIp, kOtherPeerPort));

  webrtc::SocketAddress mismatched_address(kMismatchedIpString,
                                           kMismatchedPort);
  EXPECT_EQ(socket_->SendTo(kTestData, sizeof(kTestData), mismatched_address,
                            webrtc::AsyncSocketPacketOptions()),
            -1);
  EXPECT_EQ(socket_->GetError(), ENOTCONN);
}

TEST_F(StreamPacketSocketTest, SendLargePacketFails) {
  InitSocket();
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CONNECTED);

  std::vector<char> large_data(kMaxSendBufferSize + 1, 'a');
  EXPECT_EQ(socket_->Send(large_data.data(), large_data.size(),
                          webrtc::AsyncSocketPacketOptions()),
            -1);
  EXPECT_EQ(socket_->GetError(), EMSGSIZE);
}

TEST_F(StreamPacketSocketTest, SendEmptyPacketFails) {
  InitSocket();
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CONNECTED);

  constexpr char kEmpty = '\0';
  EXPECT_EQ(socket_->Send(&kEmpty, 0, webrtc::AsyncSocketPacketOptions()), -1);
  EXPECT_EQ(socket_->GetError(), EINVAL);
}

TEST_F(StreamPacketSocketTest, SendSuccess) {
  InitSocket();
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CONNECTED);

  // We expect Write to be called once.
  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .WillOnce([](net::IOBuffer* buf, int buf_len,
                   net::CompletionOnceCallback callback,
                   const net::NetworkTrafficAnnotationTag&) {
        return buf_len;  // Return number of bytes written
      });

  EXPECT_EQ(socket_->Send(kTestData, sizeof(kTestData),
                          webrtc::AsyncSocketPacketOptions()),
            static_cast<int>(sizeof(kTestData)));
}

TEST_F(StreamPacketSocketTest, Close) {
  InitSocket();
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CONNECTED);

  // Set a non-zero error first and verify Close() preserves it.
  socket_->SetError(EMSGSIZE);
  mock_stream_socket_ = nullptr;
  socket_->Close();
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  EXPECT_EQ(socket_->GetError(), EMSGSIZE);
  // SetOption should return -1 on closed socket.
  EXPECT_EQ(socket_->SetOption(webrtc::Socket::OPT_RCVBUF, kSocketBufferSize),
            -1);
}

TEST_F(StreamPacketSocketTest, CloseFromConnectCallbackSkipsRead) {
#if defined(COMPONENT_BUILD)
  GTEST_SKIP() << "SubscribeConnect() is not linkable in component builds.";
#else
  net::CompletionOnceCallback connect_cb;
  EXPECT_CALL(*mock_stream_socket_, Connect(_))
      .WillOnce([&](net::CompletionOnceCallback callback) {
        connect_cb = std::move(callback);
        return net::ERR_IO_PENDING;
      });
  ASSERT_TRUE(
      socket_->Init(std::move(mock_socket_ptr_), &fake_packet_processor_));
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CONNECTING);

  bool close_notified = false;
  socket_->SubscribeCloseEvent(
      this, [&](webrtc::AsyncPacketSocket*, int) { close_notified = true; });
  socket_->SubscribeConnect(this, [&](webrtc::AsyncPacketSocket*) {
    mock_stream_socket_ = nullptr;
    socket_->Close();
  });

  std::move(connect_cb).Run(net::OK);
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  FlushCurrentTaskQueue();
  EXPECT_FALSE(close_notified);
#endif  // defined(COMPONENT_BUILD)
}

TEST_F(StreamPacketSocketTest, AsyncConnectFailureNotifiesCloseAsync) {
  net::CompletionOnceCallback connect_cb;
  EXPECT_CALL(*mock_stream_socket_, Connect(_))
      .WillOnce([&](net::CompletionOnceCallback callback) {
        connect_cb = std::move(callback);
        return net::ERR_IO_PENDING;
      });
  ASSERT_TRUE(
      socket_->Init(std::move(mock_socket_ptr_), &fake_packet_processor_));
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CONNECTING);

  base::RunLoop close_loop;
  bool close_notified = false;
  int close_error = 0;
  socket_->SubscribeCloseEvent(this,
                               [&](webrtc::AsyncPacketSocket* s, int err) {
                                 EXPECT_EQ(s, socket_.get());
                                 close_notified = true;
                                 close_error = err;
                                 close_loop.Quit();
                               });

  mock_stream_socket_ = nullptr;
  std::move(connect_cb).Run(net::ERR_CONNECTION_RESET);
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  EXPECT_EQ(socket_->GetError(), ECONNRESET);
  EXPECT_FALSE(close_notified);

  close_loop.Run();
  EXPECT_TRUE(close_notified);
  EXPECT_EQ(close_error, ECONNRESET);
}

TEST_F(StreamPacketSocketTest,
       SendWriteErrorReturnsErrorAndNotifiesCloseAsync) {
  InitSocket();
  ASSERT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CONNECTED);

  ReadyToSendCounter ready_to_send(socket_.get());

  base::RunLoop close_loop;
  bool close_notified = false;
  int close_error = 0;
  socket_->SubscribeCloseEvent(this,
                               [&](webrtc::AsyncPacketSocket* s, int err) {
                                 EXPECT_EQ(s, socket_.get());
                                 close_notified = true;
                                 close_error = err;
                                 close_loop.Quit();
                               });

  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .WillOnce([this](net::IOBuffer*, int, net::CompletionOnceCallback,
                       const net::NetworkTrafficAnnotationTag&) {
        mock_stream_socket_ = nullptr;
        return net::ERR_CONNECTION_RESET;
      });

  EXPECT_EQ(socket_->Send(kTestData, sizeof(kTestData),
                          webrtc::AsyncSocketPacketOptions()),
            -1);
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  EXPECT_EQ(socket_->GetError(), ECONNRESET);
  ready_to_send.ExpectCount(0);
  EXPECT_FALSE(close_notified);

  close_loop.Run();
  EXPECT_TRUE(close_notified);
  ready_to_send.ExpectCount(0);
  EXPECT_EQ(close_error, ECONNRESET);
}

TEST_F(StreamPacketSocketTest,
       SendToWriteErrorReturnsErrorAndNotifiesCloseAsync) {
  InitSocket();
  ExpectPeerAddress();

  base::RunLoop close_loop;
  bool close_notified = false;
  int close_error = 0;
  socket_->SubscribeCloseEvent(this, [&](webrtc::AsyncPacketSocket*, int err) {
    close_notified = true;
    close_error = err;
    close_loop.Quit();
  });

  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .WillOnce([this](net::IOBuffer*, int, net::CompletionOnceCallback,
                       const net::NetworkTrafficAnnotationTag&) {
        mock_stream_socket_ = nullptr;
        return net::ERR_CONNECTION_RESET;
      });

  webrtc::SocketAddress remote_address(kDefaultPeerIpString, kDefaultPeerPort);
  EXPECT_EQ(socket_->SendTo(kTestData, sizeof(kTestData), remote_address,
                            webrtc::AsyncSocketPacketOptions()),
            -1);
  EXPECT_EQ(socket_->GetError(), ECONNRESET);
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  EXPECT_FALSE(close_notified);

  close_loop.Run();
  EXPECT_TRUE(close_notified);
  EXPECT_EQ(close_error, ECONNRESET);
}

TEST_F(StreamPacketSocketTest, SendPartialWriteThenErrorReturnsError) {
  InitSocket();

  base::RunLoop close_loop;
  bool sent_notified = false;
  ReadyToSendCounter ready_to_send(socket_.get());
  bool close_notified = false;
  socket_->SubscribeSentPacket(
      this, [&](webrtc::AsyncPacketSocket*, const webrtc::SentPacketInfo&) {
        sent_notified = true;
      });
  socket_->SubscribeCloseEvent(this, [&](webrtc::AsyncPacketSocket*, int) {
    close_notified = true;
    close_loop.Quit();
  });

  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .WillOnce(Return(kPartialWriteBytes))
      .WillOnce([this](net::IOBuffer*, int, net::CompletionOnceCallback,
                       const net::NetworkTrafficAnnotationTag&) {
        mock_stream_socket_ = nullptr;
        return net::ERR_CONNECTION_RESET;
      });

  EXPECT_EQ(socket_->Send(kTestData, sizeof(kTestData),
                          webrtc::AsyncSocketPacketOptions()),
            -1);
  EXPECT_FALSE(sent_notified);
  ready_to_send.ExpectCount(0);
  EXPECT_FALSE(close_notified);
  EXPECT_EQ(socket_->GetError(), ECONNRESET);

  close_loop.Run();
  EXPECT_TRUE(close_notified);
  ready_to_send.ExpectCount(0);
}

TEST_F(StreamPacketSocketTest, SendFromSentPacketCallbackWhilePending) {
  InitSocket();

  net::CompletionOnceCallback second_write_cb;
  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .WillOnce([](net::IOBuffer*, int buf_len, net::CompletionOnceCallback,
                   const net::NetworkTrafficAnnotationTag&) { return buf_len; })
      .WillOnce([&](net::IOBuffer*, int, net::CompletionOnceCallback cb,
                    const net::NetworkTrafficAnnotationTag&) {
        second_write_cb = std::move(cb);
        return net::ERR_IO_PENDING;
      });

  ReadyToSendCounter ready_to_send(socket_.get());

  int sent_count = 0;
  socket_->SubscribeSentPacket(
      this, [&](webrtc::AsyncPacketSocket*, const webrtc::SentPacketInfo&) {
        ++sent_count;
        if (sent_count == 1) {
          EXPECT_EQ(socket_->Send(kFollowUpPacket, sizeof(kFollowUpPacket),
                                  webrtc::AsyncSocketPacketOptions()),
                    static_cast<int>(sizeof(kFollowUpPacket)));
        }
      });

  EXPECT_EQ(socket_->Send(kFirstPacket, sizeof(kFirstPacket),
                          webrtc::AsyncSocketPacketOptions()),
            static_cast<int>(sizeof(kFirstPacket)));
  EXPECT_EQ(sent_count, 1);
  ready_to_send.ExpectCount(0);
  ASSERT_FALSE(second_write_cb.is_null());

  std::move(second_write_cb).Run(sizeof(kFollowUpPacket));
  EXPECT_EQ(sent_count, 2);
  ready_to_send.ExpectCount(1);
}

TEST_F(StreamPacketSocketTest,
       SendFromSentPacketCallbackSyncFiresReadyToSendOnce) {
#if defined(COMPONENT_BUILD)
  GTEST_SKIP() << "SubscribeReadyToSend() is not linkable in component builds.";
#else
  InitSocket();

  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .Times(2)
      .WillRepeatedly(
          [](net::IOBuffer*, int buf_len, net::CompletionOnceCallback,
             const net::NetworkTrafficAnnotationTag&) { return buf_len; });

  ReadyToSendCounter ready_to_send(socket_.get());

  int sent_count = 0;
  socket_->SubscribeSentPacket(
      this, [&](webrtc::AsyncPacketSocket*, const webrtc::SentPacketInfo&) {
        ++sent_count;
        if (sent_count == 1) {
          EXPECT_EQ(socket_->Send(kFollowUpPacket, sizeof(kFollowUpPacket),
                                  webrtc::AsyncSocketPacketOptions()),
                    static_cast<int>(sizeof(kFollowUpPacket)));
        }
      });

  EXPECT_EQ(socket_->Send(kFirstPacket, sizeof(kFirstPacket),
                          webrtc::AsyncSocketPacketOptions()),
            static_cast<int>(sizeof(kFirstPacket)));
  EXPECT_EQ(sent_count, 2);
  ready_to_send.ExpectCount(1);
#endif  // defined(COMPONENT_BUILD)
}

TEST_F(StreamPacketSocketTest, SendFromReadyToSendCallbackDrainsCleanly) {
#if defined(COMPONENT_BUILD)
  GTEST_SKIP() << "SubscribeReadyToSend() is not linkable in component builds.";
#else
  InitSocket();

  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .Times(2)
      .WillRepeatedly(
          [](net::IOBuffer*, int buf_len, net::CompletionOnceCallback,
             const net::NetworkTrafficAnnotationTag&) { return buf_len; });

  int sent_count = 0;
  socket_->SubscribeSentPacket(
      this, [&](webrtc::AsyncPacketSocket*, const webrtc::SentPacketInfo&) {
        ++sent_count;
      });

  int ready_to_send_count = 0;
  socket_->SubscribeReadyToSend(this, [&](webrtc::AsyncPacketSocket*) {
    ++ready_to_send_count;
    if (ready_to_send_count == 1) {
      EXPECT_EQ(socket_->Send(kFollowUpPacket, sizeof(kFollowUpPacket),
                              webrtc::AsyncSocketPacketOptions()),
                static_cast<int>(sizeof(kFollowUpPacket)));
    }
  });

  EXPECT_EQ(socket_->Send(kFirstPacket, sizeof(kFirstPacket),
                          webrtc::AsyncSocketPacketOptions()),
            static_cast<int>(sizeof(kFirstPacket)));
  EXPECT_EQ(sent_count, 2);
  EXPECT_EQ(ready_to_send_count, 2);
#endif  // defined(COMPONENT_BUILD)
}

TEST_F(StreamPacketSocketTest, CloseFromSentPacketCallbackStopsWriting) {
  InitSocket();

  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .WillOnce(
          [](net::IOBuffer*, int buf_len, net::CompletionOnceCallback,
             const net::NetworkTrafficAnnotationTag&) { return buf_len; });

  ReadyToSendCounter ready_to_send(socket_.get());
  bool close_notified = false;
  socket_->SubscribeCloseEvent(
      this, [&](webrtc::AsyncPacketSocket*, int) { close_notified = true; });

  int sent_count = 0;
  socket_->SubscribeSentPacket(
      this, [&](webrtc::AsyncPacketSocket*, const webrtc::SentPacketInfo&) {
        ++sent_count;
        EXPECT_EQ(socket_->Send(kFollowUpPacket, sizeof(kFollowUpPacket),
                                webrtc::AsyncSocketPacketOptions()),
                  static_cast<int>(sizeof(kFollowUpPacket)));
        mock_stream_socket_ = nullptr;
        socket_->Close();
      });

  EXPECT_EQ(socket_->Send(kFirstPacket, sizeof(kFirstPacket),
                          webrtc::AsyncSocketPacketOptions()),
            static_cast<int>(sizeof(kFirstPacket)));
  EXPECT_EQ(sent_count, 1);
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  ready_to_send.ExpectCount(0);

  FlushCurrentTaskQueue();
  EXPECT_FALSE(close_notified);
}

TEST_F(StreamPacketSocketTest, CloseFromAsyncSentPacketCallbackStopsWriting) {
  InitSocket();

  net::CompletionOnceCallback write_cb;
  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .WillOnce([&](net::IOBuffer*, int, net::CompletionOnceCallback cb,
                    const net::NetworkTrafficAnnotationTag&) {
        write_cb = std::move(cb);
        return net::ERR_IO_PENDING;
      });

  ReadyToSendCounter ready_to_send(socket_.get());
  bool close_notified = false;
  socket_->SubscribeCloseEvent(
      this, [&](webrtc::AsyncPacketSocket*, int) { close_notified = true; });

  int sent_count = 0;
  socket_->SubscribeSentPacket(
      this, [&](webrtc::AsyncPacketSocket*, const webrtc::SentPacketInfo&) {
        ++sent_count;
        EXPECT_EQ(socket_->Send(kFollowUpPacket, sizeof(kFollowUpPacket),
                                webrtc::AsyncSocketPacketOptions()),
                  static_cast<int>(sizeof(kFollowUpPacket)));
        mock_stream_socket_ = nullptr;
        socket_->Close();
      });

  EXPECT_EQ(socket_->Send(kFirstPacket, sizeof(kFirstPacket),
                          webrtc::AsyncSocketPacketOptions()),
            static_cast<int>(sizeof(kFirstPacket)));
  ASSERT_FALSE(write_cb.is_null());

  std::move(write_cb).Run(sizeof(kFirstPacket));
  EXPECT_EQ(sent_count, 1);
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  ready_to_send.ExpectCount(0);

  FlushCurrentTaskQueue();
  EXPECT_FALSE(close_notified);
}

TEST_F(StreamPacketSocketTest, CloseCancelsPendingCloseNotification) {
  InitSocket();

  bool close_notified = false;
  socket_->SubscribeCloseEvent(
      this, [&](webrtc::AsyncPacketSocket*, int) { close_notified = true; });

  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .WillOnce([this](net::IOBuffer*, int, net::CompletionOnceCallback,
                       const net::NetworkTrafficAnnotationTag&) {
        mock_stream_socket_ = nullptr;
        return net::ERR_CONNECTION_RESET;
      });

  EXPECT_EQ(socket_->Send(kTestData, sizeof(kTestData),
                          webrtc::AsyncSocketPacketOptions()),
            -1);
  EXPECT_FALSE(close_notified);

  socket_->Close();
  FlushCurrentTaskQueue();
  EXPECT_FALSE(close_notified);
}

TEST_F(StreamPacketSocketTest, DestroyCancelsPendingCloseNotification) {
  InitSocket();

  bool close_notified = false;
  socket_->SubscribeCloseEvent(
      this, [&](webrtc::AsyncPacketSocket*, int) { close_notified = true; });

  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .WillOnce([this](net::IOBuffer*, int, net::CompletionOnceCallback,
                       const net::NetworkTrafficAnnotationTag&) {
        mock_stream_socket_ = nullptr;
        return net::ERR_CONNECTION_RESET;
      });

  EXPECT_EQ(socket_->Send(kTestData, sizeof(kTestData),
                          webrtc::AsyncSocketPacketOptions()),
            -1);
  EXPECT_FALSE(close_notified);

  socket_.reset();
  FlushCurrentTaskQueue();
  EXPECT_FALSE(close_notified);
}

TEST_F(StreamPacketSocketTest, ReadNetErrorNotifiesCloseAsync) {
  scoped_refptr<net::IOBuffer> read_buf;
  net::CompletionOnceCallback read_cb;
  ASSERT_NO_FATAL_FAILURE(
      InitSocketWithPendingRead(&fake_packet_processor_, &read_buf, &read_cb));

  base::RunLoop close_loop;
  bool close_notified = false;
  int close_error = 0;
  socket_->SubscribeCloseEvent(this, [&](webrtc::AsyncPacketSocket*, int err) {
    close_notified = true;
    close_error = err;
    close_loop.Quit();
  });

  read_buf.reset();
  mock_stream_socket_ = nullptr;
  std::move(read_cb).Run(net::ERR_SOCKET_NOT_CONNECTED);
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  EXPECT_EQ(socket_->GetError(), ENOTCONN);
  EXPECT_FALSE(close_notified);

  close_loop.Run();
  EXPECT_TRUE(close_notified);
  EXPECT_EQ(close_error, ENOTCONN);
}

TEST_F(StreamPacketSocketTest, ReadEofMapsToConnResetAndNotifiesCloseAsync) {
  scoped_refptr<net::IOBuffer> read_buf;
  net::CompletionOnceCallback read_cb;
  ASSERT_NO_FATAL_FAILURE(
      InitSocketWithPendingRead(&fake_packet_processor_, &read_buf, &read_cb));

  base::RunLoop close_loop;
  bool close_notified = false;
  int close_error = 0;
  socket_->SubscribeCloseEvent(this, [&](webrtc::AsyncPacketSocket*, int err) {
    close_notified = true;
    close_error = err;
    close_loop.Quit();
  });

  read_buf.reset();
  mock_stream_socket_ = nullptr;
  std::move(read_cb).Run(0);
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  EXPECT_EQ(socket_->GetError(), ECONNRESET);
  EXPECT_FALSE(close_notified);

  close_loop.Run();
  EXPECT_TRUE(close_notified);
  EXPECT_EQ(close_error, ECONNRESET);
}

TEST_F(StreamPacketSocketTest,
       AsyncReadWriteErrorStopsProcessingRemainingData) {
  scoped_refptr<net::IOBuffer> read_buf;
  net::CompletionOnceCallback read_cb;
  ASSERT_NO_FATAL_FAILURE(InitSocketWithPendingRead(&length_prefixed_processor_,
                                                    &read_buf, &read_cb));
  ExpectPeerAddress();

  int packets_received = 0;
  int reply_send_result = 0;
  socket_->RegisterReceivedPacketCallback(
      [&](webrtc::AsyncPacketSocket*, const webrtc::ReceivedIpPacket&) {
        ++packets_received;
        reply_send_result = socket_->Send(kReplyPacket, sizeof(kReplyPacket),
                                          webrtc::AsyncSocketPacketOptions());
      });

  base::RunLoop close_loop;
  bool close_notified = false;
  socket_->SubscribeCloseEvent(this, [&](webrtc::AsyncPacketSocket*, int) {
    close_notified = true;
    close_loop.Quit();
  });

  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .WillOnce([this](net::IOBuffer*, int, net::CompletionOnceCallback,
                       const net::NetworkTrafficAnnotationTag&) {
        mock_stream_socket_ = nullptr;
        return net::ERR_CONNECTION_RESET;
      });

  auto pkt1 =
      length_prefixed_processor_.Pack(base::byte_span_from_cstring(kPacketOne));
  auto pkt2 =
      length_prefixed_processor_.Pack(base::byte_span_from_cstring(kPacketTwo));
  std::vector<uint8_t> wire;
  wire.insert(wire.end(), pkt1->span().begin(), pkt1->span().end());
  wire.insert(wire.end(), pkt2->span().begin(), pkt2->span().end());
  wire.insert(wire.end(), kIncompleteFrame.begin(), kIncompleteFrame.end());
  read_buf->span().copy_prefix_from(wire);
  read_buf.reset();

  std::move(read_cb).Run(static_cast<int>(wire.size()));

  EXPECT_EQ(packets_received, 1);
  EXPECT_EQ(reply_send_result, -1);
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  EXPECT_FALSE(close_notified);

  close_loop.Run();
  EXPECT_TRUE(close_notified);
}

TEST_F(StreamPacketSocketTest, SyncReadWriteErrorStopsProcessingRemainingData) {
  auto pkt1 =
      length_prefixed_processor_.Pack(base::byte_span_from_cstring(kPacketOne));
  auto pkt2 =
      length_prefixed_processor_.Pack(base::byte_span_from_cstring(kPacketTwo));
  std::vector<uint8_t> wire;
  wire.insert(wire.end(), pkt1->span().begin(), pkt1->span().end());
  wire.insert(wire.end(), pkt2->span().begin(), pkt2->span().end());
  wire.insert(wire.end(), kIncompleteFrame.begin(), kIncompleteFrame.end());

  net::CompletionOnceCallback connect_cb;
  EXPECT_CALL(*mock_stream_socket_, Connect(_))
      .WillOnce([&](net::CompletionOnceCallback callback) {
        connect_cb = std::move(callback);
        return net::ERR_IO_PENDING;
      });
  ASSERT_TRUE(
      socket_->Init(std::move(mock_socket_ptr_), &length_prefixed_processor_));
  ExpectPeerAddress();

  base::RunLoop close_loop;
  bool close_notified = false;
  socket_->SubscribeCloseEvent(this, [&](webrtc::AsyncPacketSocket*, int) {
    close_notified = true;
    close_loop.Quit();
  });

  int packets_received = 0;
  socket_->RegisterReceivedPacketCallback(
      [&](webrtc::AsyncPacketSocket*, const webrtc::ReceivedIpPacket&) {
        ++packets_received;
        EXPECT_EQ(socket_->Send(kReplyPacket, sizeof(kReplyPacket),
                                webrtc::AsyncSocketPacketOptions()),
                  -1);
      });

  EXPECT_CALL(*mock_stream_socket_, Read(_, _, _))
      .WillOnce([&](net::IOBuffer* buf, int, net::CompletionOnceCallback) {
        buf->span().copy_prefix_from(wire);
        return static_cast<int>(wire.size());
      });
  EXPECT_CALL(*mock_stream_socket_, Write(_, _, _, _))
      .WillOnce([this](net::IOBuffer*, int, net::CompletionOnceCallback,
                       const net::NetworkTrafficAnnotationTag&) {
        mock_stream_socket_ = nullptr;
        return net::ERR_CONNECTION_RESET;
      });

  std::move(connect_cb).Run(net::OK);
  EXPECT_EQ(packets_received, 1);
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);
  EXPECT_FALSE(close_notified);
  close_loop.Run();
  EXPECT_TRUE(close_notified);
}

TEST_F(StreamPacketSocketTest,
       CloseFromReadCallbackStopsProcessingRemainingData) {
  scoped_refptr<net::IOBuffer> read_buf;
  net::CompletionOnceCallback read_cb;
  ASSERT_NO_FATAL_FAILURE(InitSocketWithPendingRead(&length_prefixed_processor_,
                                                    &read_buf, &read_cb));
  ExpectPeerAddress();

  bool close_notified = false;
  socket_->SubscribeCloseEvent(
      this, [&](webrtc::AsyncPacketSocket*, int) { close_notified = true; });

  int packets_received = 0;
  socket_->RegisterReceivedPacketCallback(
      [&](webrtc::AsyncPacketSocket*, const webrtc::ReceivedIpPacket&) {
        ++packets_received;
        mock_stream_socket_ = nullptr;
        socket_->Close();
      });

  auto pkt1 =
      length_prefixed_processor_.Pack(base::byte_span_from_cstring(kPacketOne));
  auto pkt2 =
      length_prefixed_processor_.Pack(base::byte_span_from_cstring(kPacketTwo));
  std::vector<uint8_t> wire;
  wire.insert(wire.end(), pkt1->span().begin(), pkt1->span().end());
  wire.insert(wire.end(), pkt2->span().begin(), pkt2->span().end());
  wire.insert(wire.end(), kIncompleteFrame.begin(), kIncompleteFrame.end());
  read_buf->span().copy_prefix_from(wire);
  read_buf.reset();

  std::move(read_cb).Run(static_cast<int>(wire.size()));
  EXPECT_EQ(packets_received, 1);
  EXPECT_EQ(socket_->GetState(), webrtc::AsyncPacketSocket::STATE_CLOSED);

  FlushCurrentTaskQueue();
  EXPECT_FALSE(close_notified);
}

TEST_F(StreamPacketSocketTest, ReadCallbackDestroyingSocketStopsProcessing) {
  scoped_refptr<net::IOBuffer> read_buf;
  net::CompletionOnceCallback read_cb;
  ASSERT_NO_FATAL_FAILURE(InitSocketWithPendingRead(&length_prefixed_processor_,
                                                    &read_buf, &read_cb));
  ExpectPeerAddress();

  socket_->RegisterReceivedPacketCallback(
      [&](webrtc::AsyncPacketSocket*, const webrtc::ReceivedIpPacket&) {
        mock_stream_socket_ = nullptr;
        socket_.reset();
      });

  auto pkt1 =
      length_prefixed_processor_.Pack(base::byte_span_from_cstring(kPacketOne));
  std::vector<uint8_t> wire(pkt1->span().begin(), pkt1->span().end());
  wire.insert(wire.end(), kIncompleteFrame.begin(), kIncompleteFrame.end());
  read_buf->span().copy_prefix_from(wire);
  read_buf.reset();

  std::move(read_cb).Run(static_cast<int>(wire.size()));
  EXPECT_EQ(socket_, nullptr);
}

TEST_F(StreamPacketSocketTest, SequentialReadsConsumePacketsOnce) {
  scoped_refptr<net::IOBuffer> read_buf1;
  net::CompletionOnceCallback read_cb1;
  scoped_refptr<net::IOBuffer> read_buf2;
  net::CompletionOnceCallback read_cb2;

  EXPECT_CALL(*mock_stream_socket_, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*mock_stream_socket_, Read(_, _, _))
      .WillOnce(
          [&](net::IOBuffer* buf, int, net::CompletionOnceCallback callback) {
            read_buf1 = buf;
            read_cb1 = std::move(callback);
            return net::ERR_IO_PENDING;
          })
      .WillOnce(
          [&](net::IOBuffer* buf, int, net::CompletionOnceCallback callback) {
            read_buf2 = buf;
            read_cb2 = std::move(callback);
            return net::ERR_IO_PENDING;
          })
      .WillOnce(Return(net::ERR_IO_PENDING));
  ASSERT_TRUE(
      socket_->Init(std::move(mock_socket_ptr_), &length_prefixed_processor_));
  ExpectPeerAddress();

  std::vector<std::string> received;
  socket_->RegisterReceivedPacketCallback(
      [&](webrtc::AsyncPacketSocket*, const webrtc::ReceivedIpPacket& packet) {
        received.emplace_back(
            reinterpret_cast<const char*>(packet.payload().data()),
            packet.payload().size());
      });

  auto pkt1 = length_prefixed_processor_.Pack(
      base::byte_span_from_cstring(kFirstPacket));
  read_buf1->span().copy_prefix_from(pkt1->span());
  std::move(read_cb1).Run(pkt1->size());

  auto pkt2 = length_prefixed_processor_.Pack(
      base::byte_span_from_cstring(kSecondPacket));
  read_buf2->span().copy_prefix_from(pkt2->span());
  std::move(read_cb2).Run(pkt2->size());

  ASSERT_EQ(received.size(), 2u);
  EXPECT_EQ(received[0], kFirstPacket);
  EXPECT_EQ(received[1], kSecondPacket);
}

}  // namespace remoting::protocol
