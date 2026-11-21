// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/host_event_dispatcher.h"

#include <cstddef>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/test/mock_log.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/fake_message_pipe.h"
#include "remoting/protocol/fake_message_pipe_wrapper.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {
namespace {

using test::EqualsKeyEvent;
using test::EqualsTextEvent;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::HasSubstr;

constexpr int kKeyA = 0x070004;

class FakeChannelDispatcherEventHandler
    : public ChannelDispatcherBase::EventHandler {
 public:
  void OnChannelInitialized(
      ChannelDispatcherBase* channel_dispatcher) override {}
  void OnChannelClosed(ChannelDispatcherBase* channel_dispatcher) override {}
};

EventMessage MakeKeyEventMessage() {
  EventMessage message;
  message.set_timestamp(1);
  message.mutable_key_event()->set_usb_keycode(kKeyA);
  message.mutable_key_event()->set_pressed(true);
  return message;
}

// Returns a well-formed text event message that serializes to `size_bytes`.
// Callers check the serialized size.
EventMessage MakeTextEventMessageOfSize(size_t size_bytes) {
  EventMessage message;
  message.set_timestamp(1);
  message.mutable_text_event()->set_text(std::string(size_bytes, 'a'));
  // Shorten the text by the number of bytes used for everything else.
  size_t overhead = message.ByteSizeLong() - size_bytes;
  message.mutable_text_event()->set_text(
      std::string(size_bytes - overhead, 'a'));
  return message;
}

class HostEventDispatcherTest : public testing::Test {
 protected:
  void SetUp() override {
    dispatcher_ = std::make_unique<HostEventDispatcher>();
    dispatcher_->set_input_stub(&input_stub_);
    dispatcher_->Init(pipe_.Wrap(), &event_handler_);
    pipe_.OpenPipe();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeMessagePipe pipe_{/*asynchronous=*/false};
  FakeChannelDispatcherEventHandler event_handler_;
  testing::StrictMock<MockInputStub> input_stub_;
  std::unique_ptr<HostEventDispatcher> dispatcher_;
};

TEST_F(HostEventDispatcherTest, DispatchesKeyEvent) {
  EXPECT_CALL(input_stub_, InjectKeyEvent(EqualsKeyEvent(kKeyA, true)));
  pipe_.ReceiveProtobufMessage(MakeKeyEventMessage());
}

TEST_F(HostEventDispatcherTest, DispatchesTextEvent) {
  EventMessage message;
  message.set_timestamp(1);
  message.mutable_text_event()->set_text("text");

  EXPECT_CALL(input_stub_, InjectTextEvent(EqualsTextEvent("text")));
  pipe_.ReceiveProtobufMessage(message);
}

TEST_F(HostEventDispatcherTest, DispatchesEventMessageAtSizeLimit) {
  EventMessage message = MakeTextEventMessageOfSize(kMaxEventMessageSizeBytes);
  ASSERT_EQ(message.ByteSizeLong(), kMaxEventMessageSizeBytes);

  EXPECT_CALL(input_stub_, InjectTextEvent(_));
  pipe_.ReceiveProtobufMessage(message);
}

TEST_F(HostEventDispatcherTest, DropsEventMessageOverSizeLimit) {
  // This well-formed message would be dispatched if it were not for the size
  // limit.
  EventMessage message =
      MakeTextEventMessageOfSize(kMaxEventMessageSizeBytes + 1);
  ASSERT_EQ(message.ByteSizeLong(), kMaxEventMessageSizeBytes + 1);

  EXPECT_CALL(input_stub_, InjectTextEvent(_)).Times(0);
  pipe_.ReceiveProtobufMessage(message);
  EXPECT_TRUE(dispatcher_->event_timestamps_source()
                  ->TakeLastEventTimestamps()
                  .is_null());

  // The channel stays open and later messages are dispatched.
  EXPECT_TRUE(dispatcher_->is_connected());
  EXPECT_CALL(input_stub_, InjectKeyEvent(EqualsKeyEvent(kKeyA, true)));
  pipe_.ReceiveProtobufMessage(MakeKeyEventMessage());
}

TEST_F(HostEventDispatcherTest, DropsEventMessageWhenNoInputStubIsSet) {
  dispatcher_->set_input_stub(nullptr);
  pipe_.ReceiveProtobufMessage(MakeKeyEventMessage());

  dispatcher_->set_input_stub(&input_stub_);
  EXPECT_CALL(input_stub_, InjectKeyEvent(EqualsKeyEvent(kKeyA, true)));
  pipe_.ReceiveProtobufMessage(MakeKeyEventMessage());
}

TEST_F(HostEventDispatcherTest, LimitsOversizedMessageWarningRate) {
  EventMessage oversized =
      MakeTextEventMessageOfSize(kMaxEventMessageSizeBytes + 1);
  EventMessage larger =
      MakeTextEventMessageOfSize(kMaxEventMessageSizeBytes + 100);
  ASSERT_EQ(oversized.ByteSizeLong(), kMaxEventMessageSizeBytes + 1);
  ASSERT_EQ(larger.ByteSizeLong(), kMaxEventMessageSizeBytes + 100);

  base::test::MockLog log;
  EXPECT_CALL(log, Log(_, _, _, _, _)).Times(AnyNumber());
  {
    testing::InSequence sequence;
    EXPECT_CALL(log, Log(logging::LOGGING_WARNING, _, _, _,
                         HasSubstr("Dropped an event message of 4097 bytes")));
    EXPECT_CALL(log, Log(logging::LOGGING_WARNING, _, _, _,
                         HasSubstr("Dropped an event message of 4196 bytes")));
  }
  log.StartCapturingLogs();

  // Only the first of these messages is logged.
  pipe_.ReceiveProtobufMessage(oversized);
  pipe_.ReceiveProtobufMessage(oversized);
  task_environment_.FastForwardBy(base::Seconds(9));
  pipe_.ReceiveProtobufMessage(oversized);

  // The logging interval has passed, so this message is logged.
  task_environment_.FastForwardBy(base::Seconds(1));
  pipe_.ReceiveProtobufMessage(larger);
}

}  // namespace
}  // namespace remoting::protocol
