// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_socket_adapter.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/p2p/base/mock_ice_transport.h"

using net::IOBuffer;

using testing::_;
using testing::Return;

namespace remoting {
namespace protocol {

namespace {
const int kBufferSize = 4096;
const char kTestData[] = "data";
const int kTestDataSize = 4;
const int kTestError = -32123;
}  // namespace

class TransportChannelSocketAdapterTest : public testing::Test {
 public:
  TransportChannelSocketAdapterTest()
      : callback_(base::Bind(&TransportChannelSocketAdapterTest::Callback,
                             base::Unretained(this))),
        callback_result_(0) {
  }

 protected:
  void SetUp() override {
    target_.reset(new TransportChannelSocketAdapter(&channel_));
  }

  void Callback(int result) {
    callback_result_ = result;
  }

  cricket::MockIceTransport channel_;
  std::unique_ptr<TransportChannelSocketAdapter> target_;
  net::CompletionRepeatingCallback callback_;
  int callback_result_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
};

// Verify that Read() returns net::ERR_IO_PENDING.
TEST_F(TransportChannelSocketAdapterTest, Read) {
  scoped_refptr<IOBuffer> buffer = base::MakeRefCounted<IOBuffer>(kBufferSize);

  int result = target_->Recv(buffer.get(), kBufferSize, callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  channel_.SignalReadPacket(&channel_, kTestData, kTestDataSize,
                            rtc::TimeMicros(), 0);
  EXPECT_EQ(kTestDataSize, callback_result_);
}

// Verify that Read() after Close() returns error.
TEST_F(TransportChannelSocketAdapterTest, ReadClose) {
  scoped_refptr<IOBuffer> buffer = base::MakeRefCounted<IOBuffer>(kBufferSize);

  int result = target_->Recv(buffer.get(), kBufferSize, callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  target_->Close(kTestError);
  EXPECT_EQ(kTestError, callback_result_);

  // All Recv() calls after Close() should return the error.
  EXPECT_EQ(kTestError, target_->Recv(buffer.get(), kBufferSize, callback_));
}

// Verify that Send sends the packet and returns correct result.
TEST_F(TransportChannelSocketAdapterTest, Send) {
  scoped_refptr<IOBuffer> buffer =
      base::MakeRefCounted<IOBuffer>(kTestDataSize);

  EXPECT_CALL(channel_, SendPacket(buffer->data(), kTestDataSize, _, 0))
      .WillOnce(Return(kTestDataSize));

  int result = target_->Send(buffer.get(), kTestDataSize, callback_);
  EXPECT_EQ(kTestDataSize, result);
}

// Verify that the message is still sent if Send() is called while
// socket is not open yet. The result is the packet is lost.
TEST_F(TransportChannelSocketAdapterTest, SendPending) {
  scoped_refptr<IOBuffer> buffer =
      base::MakeRefCounted<IOBuffer>(kTestDataSize);

  EXPECT_CALL(channel_, SendPacket(buffer->data(), kTestDataSize, _, 0))
      .Times(1)
      .WillOnce(Return(SOCKET_ERROR));

  EXPECT_CALL(channel_, GetError())
      .WillOnce(Return(EWOULDBLOCK));

  int result = target_->Send(buffer.get(), kTestDataSize, callback_);
  ASSERT_EQ(net::OK, result);
}

}  // namespace protocol
}  // namespace remoting
