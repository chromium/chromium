// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/udp_socket_posix.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/socket/datagram_socket.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace net {

namespace test {

namespace {

const size_t kMaxPacketSize = 1500;
const size_t kNumMsgs = 3;
const std::string kHelloMsg = "Hello world";
const std::string kSecondMsg = "Second buffer";
const std::string kThirdMsg = "Third buffer";

int SetWouldBlock() {
  errno = EWOULDBLOCK;
  return -1;
}

#if HAVE_SENDMMSG
int SetNotImplemented() {
  errno = ENOSYS;
  return -1;
}
#endif

bool WatcherSetInvalidHandle() {
  errno = EBADF;
  return false;
}

int SetInvalidHandle() {
  errno = EBADF;
  return -1;
}

}  // namespace

class MockUDPSocketPosixSender : public UDPSocketPosixSender {
 public:
  MOCK_CONST_METHOD4(
      Send,
      ssize_t(int sockfd, const void* buf, size_t len, int flags));
  MOCK_CONST_METHOD4(Sendmmsg,
                     int(int sockfd,
                         struct mmsghdr* msgvec,
                         unsigned int vlen,
                         unsigned int flags));

 public:
  SendResult InternalSendBuffers(int fd, DatagramBuffers buffers) const {
    return UDPSocketPosixSender::InternalSendBuffers(fd, std::move(buffers));
  }
#if HAVE_SENDMMSG
  SendResult InternalSendmmsgBuffers(int fd, DatagramBuffers buffers) const {
    return UDPSocketPosixSender::InternalSendmmsgBuffers(fd,
                                                         std::move(buffers));
  }
#endif

 private:
  ~MockUDPSocketPosixSender() override {}
};

class MockUDPSocketPosix : public UDPSocketPosix {
 public:
  MockUDPSocketPosix(DatagramSocket::BindType bind_type,
                     net::NetLog* net_log,
                     const net::NetLogSource& source)
      : UDPSocketPosix(bind_type, net_log, source) {
    sender_ = new MockUDPSocketPosixSender();
  }

  MockUDPSocketPosixSender* sender() {
    return static_cast<MockUDPSocketPosixSender*>(sender_.get());
  }

  MOCK_METHOD0(InternalWatchFileDescriptor, bool());
  MOCK_METHOD0(InternalStopWatchingFileDescriptor, void());

  void FlushPending() { UDPSocketPosix::FlushPending(); }

  void DidSendBuffers(SendResult buffers) {
    UDPSocketPosix::DidSendBuffers(std::move(buffers));
  }

  void Enqueue(const std::string& msg, DatagramBuffers* buffers) {
    datagram_buffer_pool_->Enqueue(msg.data(), msg.length(), buffers);
  }

  void SetWriteCallback(CompletionOnceCallback callback) {
    UDPSocketPosix::SetWriteCallback(std::move(callback));
  }

  void IncreaseWriteAsyncOutstanding(int increment) {
    UDPSocketPosix::IncreaseWriteAsyncOutstanding(increment);
  }

  void SetPendingWrites(DatagramBuffers buffers) {
    pending_writes_ = std::move(buffers);
  }

  void OnFileCanWriteWithoutBlocking() {
    write_async_watcher_->OnFileCanWriteWithoutBlocking(1);
  }
};

class UDPSocketPosixTest : public TestWithTaskEnvironment {
 public:
  UDPSocketPosixTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        socket_(DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource()),
        callback_fired_(false) {
    write_callback_ = base::BindRepeating(&UDPSocketPosixTest::OnWriteComplete,
                                          weak_factory_.GetWeakPtr());
  }

  void SetUp() override {
    socket_.SetWriteAsyncEnabled(true);
    socket_.SetMaxPacketSize(kMaxPacketSize);
  }

  void AddBuffer(const std::string& msg) {
    socket_.IncreaseWriteAsyncOutstanding(1);
    socket_.Enqueue(msg, &buffers_);
  }

  void AddBuffers() {
    for (size_t i = 0; i < kNumMsgs; i++) {
      AddBuffer(msgs_[i]);
    }
  }

  void SaveBufferPtrs() {
    int i = 0;
    for (auto it = buffers_.cbegin(); it != buffers_.cend(); it++) {
      buffer_ptrs_[i] = it->get();
      i++;
    }
  }

  void VerifyBufferPtrs() {
    int i = 0;
    for (auto it = buffers_.cbegin(); it != buffers_.cend(); it++) {
      EXPECT_EQ(buffer_ptrs_[i], it->get());
      i++;
    }
  }

  void VerifyBuffersDequeued() {
    AddBuffers();
    VerifyBufferPtrs();
    buffers_.clear();
  }

  void ResetWriteCallback() {
    callback_fired_ = false;
    rv_ = 0;
  }

  void OnWriteComplete(int rv) {
    callback_fired_ = true;
    rv_ = rv;
  }

  int WriteAsync(int i) {
    return socket_.WriteAsync(msgs_[i].data(), lengths_[i], write_callback_,
                              TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  void ExpectSend(int i) {
    EXPECT_CALL(*socket_.sender(), Send(_, _, lengths_[i], _))
        .WillOnce(Return(lengths_[i]));
  }

  void ExpectSendWillBlock(int i) {
    EXPECT_CALL(*socket_.sender(), Send(_, _, lengths_[i], _))
        .WillOnce(InvokeWithoutArgs(SetWouldBlock));
    EXPECT_CALL(socket_, InternalWatchFileDescriptor()).WillOnce(Return(true));
  }

  void ExpectSendWillError(int i) {
    EXPECT_CALL(*socket_.sender(), Send(_, _, lengths_[i], _))
        .WillOnce(InvokeWithoutArgs(SetInvalidHandle));
  }

  void ExpectSends() {
    InSequence dummy;
    for (size_t i = 0; i < kNumMsgs; i++) {
      ExpectSend(static_cast<int>(i));
    }
  }

  void ExpectSendmmsg() {
    EXPECT_CALL(*socket_.sender(), Sendmmsg(_, _, kNumMsgs, _))
        .WillOnce(Return(kNumMsgs));
  }

  RecordingNetLogObserver net_log_observer_;
  MockUDPSocketPosix socket_;
  DatagramBuffers buffers_;
  bool callback_fired_;
  int rv_;
  std::string msgs_[kNumMsgs] = {kHelloMsg, kSecondMsg, kThirdMsg};
  int lengths_[kNumMsgs] = {static_cast<int>(kHelloMsg.length()),
                            static_cast<int>(kSecondMsg.length()),
                            static_cast<int>(kThirdMsg.length())};
  int total_lengths_ =
      kHelloMsg.length() + kSecondMsg.length() + kThirdMsg.length();
  DatagramBuffer* buffer_ptrs_[kNumMsgs];
  CompletionRepeatingCallback write_callback_;
#if HAVE_SENDMMSG
  struct iovec msg_iov_[kNumMsgs];
  struct mmsghdr msgvec_[kNumMsgs];
#endif
  base::WeakPtrFactory<UDPSocketPosixTest> weak_factory_{this};
};

TEST_F(UDPSocketPosixTest, InternalSendBuffers) {
  AddBuffers();
  ExpectSends();
  SendResult result = socket_.sender()->SendBuffers(1, std::move(buffers_));
  DatagramBuffers& buffers = result.buffers;
  EXPECT_EQ(0, result.rv);
  EXPECT_EQ(3, result.write_count);
  EXPECT_EQ(kNumMsgs, buffers.size());
}

TEST_F(UDPSocketPosixTest, InternalSendBuffersWriteError) {
  AddBuffers();
  {
    InSequence dummy;
    EXPECT_CALL(*socket_.sender(), Send(_, _, lengths_[0], _))
        .WillOnce(Return(lengths_[0]));
    EXPECT_CALL(*socket_.sender(), Send(_, _, lengths_[1], _))
        .WillOnce(InvokeWithoutArgs(SetWouldBlock));
  }
  SendResult result = socket_.sender()->SendBuffers(1, std::move(buffers_));
  DatagramBuffers& buffers = result.buffers;
  EXPECT_EQ(ERR_IO_PENDING, result.rv);
  EXPECT_EQ(1, result.write_count);
  EXPECT_EQ(kNumMsgs, buffers.size());
}

#if HAVE_SENDMMSG

TEST_F(UDPSocketPosixTest, InternalSendmmsgBuffers) {
  AddBuffers();
  ExpectSendmmsg();
  SendResult result =
      socket_.sender()->InternalSendmmsgBuffers(1, std::move(buffers_));
  DatagramBuffers& buffers = result.buffers;
  EXPECT_EQ(0, result.rv);
  EXPECT_EQ(3, result.write_count);
  EXPECT_EQ(kNumMsgs, buffers.size());
}

TEST_F(UDPSocketPosixTest, InternalSendmmsgBuffersWriteShort) {
  AddBuffers();
  EXPECT_CALL(*socket_.sender(), Sendmmsg(_, _, kNumMsgs, _))
      .WillOnce(Return(1));
  SendResult result =
      socket_.sender()->InternalSendmmsgBuffers(1, std::move(buffers_));
  DatagramBuffers& buffers = result.buffers;
  EXPECT_EQ(0, result.rv);
  EXPECT_EQ(1, result.write_count);
  EXPECT_EQ(kNumMsgs, buffers.size());
}

TEST_F(UDPSocketPosixTest, InternalSendmmsgBuffersWriteError) {
  AddBuffers();
  EXPECT_CALL(*socket_.sender(), Sendmmsg(_, _, kNumMsgs, _))
      .WillOnce(InvokeWithoutArgs(SetWouldBlock));
  SendResult result =
      socket_.sender()->InternalSendmmsgBuffers(1, std::move(buffers_));
  DatagramBuffers& buffers = result.buffers;
  EXPECT_EQ(ERR_IO_PENDING, result.rv);
  EXPECT_EQ(0, result.write_count);
  EXPECT_EQ(kNumMsgs, buffers.size());
}

TEST_F(UDPSocketPosixTest, SendInternalSend) {
  AddBuffers();
  ExpectSends();
  SendResult result = socket_.sender()->SendBuffers(1, std::move(buffers_));
  EXPECT_EQ(0, result.rv);
  EXPECT_EQ(3, result.write_count);
  EXPECT_EQ(kNumMsgs, result.buffers.size());
}

TEST_F(UDPSocketPosixTest, SendInternalSendmmsg) {
  socket_.sender()->SetSendmmsgEnabled(true);
  AddBuffers();
  ExpectSendmmsg();
  SendResult result = socket_.sender()->SendBuffers(1, std::move(buffers_));
  EXPECT_EQ(0, result.rv);
  EXPECT_EQ(3, result.write_count);
  EXPECT_EQ(kNumMsgs, result.buffers.size());
}

TEST_F(UDPSocketPosixTest, SendInternalSendmmsgFallback) {
  socket_.sender()->SetSendmmsgEnabled(true);
  AddBuffers();
  {
    InSequence dummy;
    EXPECT_CALL(*socket_.sender(), Sendmmsg(_, _, kNumMsgs, _))
        .WillOnce(InvokeWithoutArgs(SetNotImplemented));
    ExpectSends();
  }
  SendResult result = socket_.sender()->SendBuffers(1, std::move(buffers_));
  EXPECT_EQ(0, result.rv);
  EXPECT_EQ(3, result.write_count);
  EXPECT_EQ(kNumMsgs, result.buffers.size());
}

#endif  // HAVE_SENDMMSG

TEST_F(UDPSocketPosixTest, DidSendBuffers) {
  AddBuffers();
  SaveBufferPtrs();
  SendResult send_result(0, kNumMsgs, std::move(buffers_));
  socket_.DidSendBuffers(std::move(send_result));
  EXPECT_EQ(0u, socket_.GetUnwrittenBuffers().size());
  VerifyBuffersDequeued();
  auto client_entries = net_log_observer_.GetEntries();
  EXPECT_EQ(4u, client_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 1,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 2,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 3,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_FALSE(callback_fired_);
}

TEST_F(UDPSocketPosixTest, DidSendBuffersAsync) {
  AddBuffers();
  SendResult send_result(0, kNumMsgs, std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  socket_.DidSendBuffers(std::move(send_result));
  EXPECT_EQ(0u, socket_.GetUnwrittenBuffers().size());
  auto client_entries = net_log_observer_.GetEntries();
  EXPECT_EQ(4u, client_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 1,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 2,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 3,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, total_lengths_);
}

TEST_F(UDPSocketPosixTest, DidSendBuffersError) {
  AddBuffers();
  SendResult send_result(ERR_INVALID_HANDLE, 1, std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  socket_.DidSendBuffers(std::move(send_result));
  EXPECT_EQ(2u, socket_.GetUnwrittenBuffers().size());
  auto client_entries = net_log_observer_.GetEntries();
  EXPECT_EQ(2u, client_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 1,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, ERR_INVALID_HANDLE);
}

TEST_F(UDPSocketPosixTest, DidSendBuffersShort) {
  AddBuffers();
  SendResult send_result(0, 1, std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  socket_.DidSendBuffers(std::move(send_result));
  EXPECT_EQ(2u, socket_.GetUnwrittenBuffers().size());
  auto client_entries = net_log_observer_.GetEntries();
  EXPECT_EQ(2u, client_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 1,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, lengths_[0]);
}

TEST_F(UDPSocketPosixTest, DidSendBuffersPending) {
  AddBuffers();
  SendResult send_result(ERR_IO_PENDING, 1, std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  EXPECT_CALL(socket_, InternalWatchFileDescriptor()).WillOnce(Return(true));
  socket_.DidSendBuffers(std::move(send_result));
  EXPECT_EQ(2u, socket_.GetUnwrittenBuffers().size());
  auto client_entries = net_log_observer_.GetEntries();
  EXPECT_EQ(2u, client_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 1,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, lengths_[0]);
}

TEST_F(UDPSocketPosixTest, DidSendBuffersWatchError) {
  AddBuffers();
  SendResult send_result(ERR_IO_PENDING, 1, std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  EXPECT_CALL(socket_, InternalWatchFileDescriptor())
      .WillOnce(InvokeWithoutArgs(WatcherSetInvalidHandle));
  socket_.DidSendBuffers(std::move(send_result));
  EXPECT_EQ(2u, socket_.GetUnwrittenBuffers().size());
  auto client_entries = net_log_observer_.GetEntries();
  EXPECT_EQ(3u, client_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 1,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 2,
                               NetLogEventType::UDP_SEND_ERROR,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, ERR_INVALID_HANDLE);
}

TEST_F(UDPSocketPosixTest, DidSendBuffersStopWatch) {
  AddBuffers();
  SendResult send_result(ERR_IO_PENDING, 1, std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  EXPECT_CALL(socket_, InternalWatchFileDescriptor()).WillOnce(Return(true));
  socket_.DidSendBuffers(std::move(send_result));
  buffers_ = socket_.GetUnwrittenBuffers();
  EXPECT_EQ(2u, buffers_.size());
  auto client_entries = net_log_observer_.GetEntries();
  EXPECT_EQ(2u, client_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 1,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, lengths_[0]);

  SendResult send_result2(0, 2, std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  EXPECT_CALL(socket_, InternalStopWatchingFileDescriptor());

  socket_.DidSendBuffers(std::move(send_result2));

  EXPECT_EQ(0u, socket_.GetUnwrittenBuffers().size());
  client_entries = net_log_observer_.GetEntries();
  EXPECT_EQ(4u, client_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 1,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 2,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 3,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, lengths_[1] + lengths_[2]);
}

TEST_F(UDPSocketPosixTest, DidSendBuffersErrorStopWatch) {
  AddBuffers();
  SendResult send_result(ERR_IO_PENDING, 1, std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  EXPECT_CALL(socket_, InternalWatchFileDescriptor()).WillOnce(Return(true));
  socket_.DidSendBuffers(std::move(send_result));
  buffers_ = socket_.GetUnwrittenBuffers();
  EXPECT_EQ(2u, buffers_.size());
  auto client_entries = net_log_observer_.GetEntries();
  EXPECT_EQ(2u, client_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 1,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, lengths_[0]);

  SendResult send_result2(ERR_INVALID_HANDLE, 0, std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  EXPECT_CALL(socket_, InternalStopWatchingFileDescriptor());

  socket_.DidSendBuffers(std::move(send_result2));

  EXPECT_EQ(2u, socket_.GetUnwrittenBuffers().size());
  client_entries = net_log_observer_.GetEntries();
  EXPECT_EQ(2u, client_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 1,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, ERR_INVALID_HANDLE);
}

TEST_F(UDPSocketPosixTest, DidSendBuffersDelayCallbackWhileTooManyBuffers) {
  for (int i = 0; i < kWriteAsyncCallbackBuffersThreshold + 2; i++) {
    AddBuffer(msgs_[0]);
  }
  SendResult send_result(0, 2, std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  socket_.DidSendBuffers(std::move(send_result));
  auto client_entries = net_log_observer_.GetEntries();
  EXPECT_EQ(3u, client_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 1,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 2,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  // bytes written but no callback because socket_.pending_writes_ is full.
  EXPECT_FALSE(callback_fired_);

  // now the rest
  buffers_ = socket_.GetUnwrittenBuffers();
  EXPECT_EQ(kWriteAsyncCallbackBuffersThreshold,
            static_cast<int>(buffers_.size()));
  SendResult send_result2(0, buffers_.size(), std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  socket_.DidSendBuffers(std::move(send_result2));
  EXPECT_TRUE(callback_fired_);
  // rv includes bytes from previous invocation.
  EXPECT_EQ(rv_, (kWriteAsyncCallbackBuffersThreshold + 2) * lengths_[0]);
}

TEST_F(UDPSocketPosixTest, FlushPendingLocal) {
  socket_.SetWriteMultiCoreEnabled(false);
  AddBuffers();
  ExpectSends();
  socket_.SetPendingWrites(std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  socket_.FlushPending();
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, total_lengths_);
}

TEST_F(UDPSocketPosixTest, FlushPendingMultiCore) {
  socket_.SetWriteMultiCoreEnabled(true);
  AddBuffers();
  ExpectSends();
  socket_.SetPendingWrites(std::move(buffers_));
  ResetWriteCallback();
  socket_.SetWriteCallback(write_callback_);
  socket_.FlushPending();
  EXPECT_FALSE(callback_fired_);
  RunUntilIdle();
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, total_lengths_);
}

TEST_F(UDPSocketPosixTest, WriteAsyncNoBatching) {
  socket_.SetWriteBatchingActive(false);
  socket_.SetWriteMultiCoreEnabled(true);
  DatagramBuffers buffers;
  ExpectSend(0);
  int rv = WriteAsync(0);
  EXPECT_EQ(lengths_[0], rv);
  ExpectSend(1);
  rv = WriteAsync(1);
  EXPECT_EQ(lengths_[1], rv);
  ExpectSend(2);
  rv = WriteAsync(2);
  EXPECT_EQ(lengths_[2], rv);
}

TEST_F(UDPSocketPosixTest, WriteAsyncNoBatchingErrIOPending) {
  socket_.SetWriteBatchingActive(false);
  socket_.SetWriteMultiCoreEnabled(true);
  DatagramBuffers buffers;
  ExpectSend(0);
  int rv = WriteAsync(0);
  EXPECT_EQ(lengths_[0], rv);
  ExpectSendWillBlock(1);
  rv = WriteAsync(1);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_CALL(socket_, InternalStopWatchingFileDescriptor());
  ExpectSend(1);
  socket_.OnFileCanWriteWithoutBlocking();
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, lengths_[1]);
}

TEST_F(UDPSocketPosixTest, WriteAsyncNoBatchingError) {
  socket_.SetWriteBatchingActive(false);
  socket_.SetWriteMultiCoreEnabled(true);
  DatagramBuffers buffers;
  ExpectSend(0);
  int rv = WriteAsync(0);
  EXPECT_EQ(lengths_[0], rv);
  ExpectSendWillError(1);
  rv = WriteAsync(1);
  EXPECT_EQ(ERR_INVALID_HANDLE, rv);
}

TEST_F(UDPSocketPosixTest, WriteAsyncBasicDelay) {
  socket_.SetWriteBatchingActive(true);
  socket_.SetWriteMultiCoreEnabled(true);
  DatagramBuffers buffers;
  ASSERT_LT(kWriteAsyncMinBuffersThreshold, 3);
  ASSERT_GT(kWriteAsyncPostBuffersThreshold, 3);
  int rv = WriteAsync(0);
  EXPECT_EQ(0, rv);
  rv = WriteAsync(1);
  EXPECT_EQ(0, rv);
  rv = WriteAsync(2);
  EXPECT_EQ(0, rv);
  // Cause the write async timer to fire and above writes to flush.
  ExpectSends();
  FastForwardBy(kWriteAsyncMsThreshold);
  RunUntilIdle();
  rv = WriteAsync(0);
  EXPECT_EQ(total_lengths_, rv);
}

TEST_F(UDPSocketPosixTest, WriteAsyncPostBuffersThresholdLocal) {
  socket_.SetWriteBatchingActive(true);
  socket_.SetWriteMultiCoreEnabled(false);
  DatagramBuffers buffers;
  int rv = 0;
  for (int i = 0; i < kWriteAsyncPostBuffersThreshold - 1; i++) {
    WriteAsync(0);
    EXPECT_EQ(0, rv);
  }
  EXPECT_CALL(*socket_.sender(), Send(_, _, lengths_[0], _))
      .Times(kWriteAsyncPostBuffersThreshold)
      .WillRepeatedly(Return(lengths_[0]));
  rv = WriteAsync(0);
  EXPECT_EQ(kWriteAsyncPostBuffersThreshold * lengths_[0], rv);
}

TEST_F(UDPSocketPosixTest, WriteAsyncPostBuffersThresholdRemote) {
  socket_.SetWriteBatchingActive(true);
  socket_.SetWriteMultiCoreEnabled(true);
  EXPECT_CALL(*socket_.sender(), Send(_, _, lengths_[0], _))
      .Times(kWriteAsyncPostBuffersThreshold)
      .WillRepeatedly(Return(lengths_[0]));
  DatagramBuffers buffers;
  int rv = 0;
  for (int i = 0; i < kWriteAsyncPostBuffersThreshold; i++) {
    WriteAsync(0);
    EXPECT_EQ(0, rv);
  }
  RunUntilIdle();
  rv = WriteAsync(0);
  EXPECT_EQ(kWriteAsyncPostBuffersThreshold * lengths_[0], rv);
}

TEST_F(UDPSocketPosixTest, WriteAsyncPostBlocks) {
  socket_.SetWriteBatchingActive(true);
  socket_.SetWriteMultiCoreEnabled(true);
  DatagramBuffers buffers;
  for (int i = 0; i < kWriteAsyncMaxBuffersThreshold; i++) {
    socket_.Enqueue(msgs_[0], &buffers_);
  }
  EXPECT_CALL(*socket_.sender(), Send(_, _, lengths_[0], _))
      .Times(kWriteAsyncMaxBuffersThreshold)
      .WillRepeatedly(Return(lengths_[0]));
  int rv = socket_.WriteAsync(std::move(buffers_), write_callback_,
                              TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(callback_fired_);
  RunUntilIdle();
  EXPECT_TRUE(callback_fired_);
  EXPECT_EQ(rv_, kWriteAsyncMaxBuffersThreshold * lengths_[0]);
}

}  // namespace test

}  // namespace net
