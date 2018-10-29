// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_rtc_data_channel_handler.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

class MockHandler final : public WebRTCDataChannelHandler {
 public:
  MockHandler()
      : client_(nullptr),
        state_(WebRTCDataChannelHandlerClient::kReadyStateConnecting),
        buffered_amount_(0) {}
  void SetClient(WebRTCDataChannelHandlerClient* client) override {
    client_ = client;
  }
  WebString Label() override { return WebString(""); }
  bool Ordered() const override { return true; }
  unsigned short MaxRetransmitTime() const override { return 0; }
  unsigned short MaxRetransmits() const override { return 0; }
  WebString Protocol() const override { return WebString(""); }
  bool Negotiated() const override { return false; }
  unsigned short Id() const override { return 0; }

  WebRTCDataChannelHandlerClient::ReadyState GetState() const override {
    return state_;
  }
  unsigned long BufferedAmount() override { return buffered_amount_; }
  bool SendStringData(const WebString& s) override {
    buffered_amount_ += s.length();
    return true;
  }
  bool SendRawData(const char* data, size_t length) override {
    buffered_amount_ += length;
    return true;
  }
  void Close() override {}

  // Methods for testing.
  void ChangeState(WebRTCDataChannelHandlerClient::ReadyState state) {
    state_ = state;
    if (client_) {
      client_->DidChangeReadyState(state_);
    }
  }
  void DrainBuffer(unsigned long bytes) {
    unsigned long old_buffered_amount = buffered_amount_;
    buffered_amount_ -= bytes;
    if (client_) {
      client_->DidDecreaseBufferedAmount(
          static_cast<unsigned>(old_buffered_amount));
    }
  }

 private:
  WebRTCDataChannelHandlerClient* client_;
  WebRTCDataChannelHandlerClient::ReadyState state_;
  unsigned long buffered_amount_;
};

}  // namespace

TEST(RTCDataChannelTest, BufferedAmount) {
  MockHandler* handler = new MockHandler();
  RTCDataChannel* channel = RTCDataChannel::Create(new NullExecutionContext,
                                                   base::WrapUnique(handler));

  handler->ChangeState(WebRTCDataChannelHandlerClient::kReadyStateOpen);
  String message(std::string(100, 'A').c_str());
  channel->send(message, IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ(100U, channel->bufferedAmount());
}

TEST(RTCDataChannelTest, BufferedAmountLow) {
  MockHandler* handler = new MockHandler();
  RTCDataChannel* channel = RTCDataChannel::Create(new NullExecutionContext,
                                                   base::WrapUnique(handler));

  // Add and drain 100 bytes
  handler->ChangeState(WebRTCDataChannelHandlerClient::kReadyStateOpen);
  String message(std::string(100, 'A').c_str());
  channel->send(message, IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ(100U, channel->bufferedAmount());
  EXPECT_EQ(1U, channel->scheduled_events_.size());

  handler->DrainBuffer(100);
  EXPECT_EQ(0U, channel->bufferedAmount());
  EXPECT_EQ(2U, channel->scheduled_events_.size());
  EXPECT_EQ(
      "bufferedamountlow",
      std::string(channel->scheduled_events_.back()->type().Utf8().data()));

  // Add and drain 1 byte
  channel->send("A", IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ(1U, channel->bufferedAmount());
  EXPECT_EQ(2U, channel->scheduled_events_.size());

  handler->DrainBuffer(1);
  EXPECT_EQ(0U, channel->bufferedAmount());
  EXPECT_EQ(3U, channel->scheduled_events_.size());
  EXPECT_EQ(
      "bufferedamountlow",
      std::string(channel->scheduled_events_.back()->type().Utf8().data()));

  // Set the threshold to 99 bytes, add 101, and drain 1 byte at a time.
  channel->setBufferedAmountLowThreshold(99U);
  channel->send(message, IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ(100U, channel->bufferedAmount());

  channel->send("A", IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ(101U, channel->bufferedAmount());

  handler->DrainBuffer(1);
  EXPECT_EQ(100U, channel->bufferedAmount());
  EXPECT_EQ(3U, channel->scheduled_events_.size());  // No new events.

  handler->DrainBuffer(1);
  EXPECT_EQ(99U, channel->bufferedAmount());
  EXPECT_EQ(4U, channel->scheduled_events_.size());  // One new event.
  EXPECT_EQ(
      "bufferedamountlow",
      std::string(channel->scheduled_events_.back()->type().Utf8().data()));

  handler->DrainBuffer(1);
  EXPECT_EQ(98U, channel->bufferedAmount());

  channel->setBufferedAmountLowThreshold(97U);
  EXPECT_EQ(4U, channel->scheduled_events_.size());  // No new events.

  handler->DrainBuffer(1);
  EXPECT_EQ(97U, channel->bufferedAmount());
  EXPECT_EQ(5U, channel->scheduled_events_.size());  // New event.
  EXPECT_EQ(
      "bufferedamountlow",
      std::string(channel->scheduled_events_.back()->type().Utf8().data()));
}

TEST(RTCDataChannelTest, SendAfterContextDestroyed) {
  MockHandler* handler = new MockHandler();
  RTCDataChannel* channel = RTCDataChannel::Create(new NullExecutionContext,
                                                   base::WrapUnique(handler));
  handler->ChangeState(WebRTCDataChannelHandlerClient::kReadyStateOpen);
  channel->ContextDestroyed(nullptr);

  String message(std::string(100, 'A').c_str());
  DummyExceptionStateForTesting exception_state;
  channel->send(message, exception_state);

  EXPECT_TRUE(exception_state.HadException());
}

TEST(RTCDataChannelTest, CloseAfterContextDestroyed) {
  MockHandler* handler = new MockHandler();
  RTCDataChannel* channel = RTCDataChannel::Create(new NullExecutionContext,
                                                   base::WrapUnique(handler));
  handler->ChangeState(WebRTCDataChannelHandlerClient::kReadyStateOpen);
  channel->ContextDestroyed(nullptr);
  channel->close();
  EXPECT_EQ(String::FromUTF8("closed"), channel->readyState());
}

}  // namespace blink
