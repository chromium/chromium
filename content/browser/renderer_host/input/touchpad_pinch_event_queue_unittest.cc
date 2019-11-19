// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touchpad_pinch_event_queue.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "content/common/input/event_with_latency_info.h"
#include "content/public/common/content_features.h"
#include "content/public/common/input_event_ack_source.h"
#include "content/public/common/input_event_ack_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/latency/latency_info.h"

namespace content {

class MockTouchpadPinchEventQueueClient : public TouchpadPinchEventQueueClient {
 public:
  MockTouchpadPinchEventQueueClient() = default;
  ~MockTouchpadPinchEventQueueClient() override = default;

  // TouchpadPinchEventQueueClient
  MOCK_METHOD1(SendMouseWheelEventForPinchImmediately,
               void(const MouseWheelEventWithLatencyInfo& event));
  MOCK_METHOD3(OnGestureEventForPinchAck,
               void(const GestureEventWithLatencyInfo& event,
                    InputEventAckSource ack_source,
                    InputEventAckState ack_result));
};

class TouchpadPinchEventQueueTest : public testing::TestWithParam<bool> {
 protected:
  TouchpadPinchEventQueueTest() : async_events_enabled_(GetParam()) {
    if (async_events_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kTouchpadAsyncPinchEvents);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kTouchpadAsyncPinchEvents);
    }
    queue_ = std::make_unique<TouchpadPinchEventQueue>(&mock_client_);
  }
  ~TouchpadPinchEventQueueTest() = default;

  void QueueEvent(const blink::WebGestureEvent& event) {
    queue_->QueueEvent(GestureEventWithLatencyInfo(event));
  }

  void QueuePinchBegin() {
    blink::WebGestureEvent event(
        blink::WebInputEvent::kGesturePinchBegin,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchpad);
    event.SetPositionInWidget(gfx::PointF(1, 1));
    event.SetPositionInScreen(gfx::PointF(1, 1));
    event.SetNeedsWheelEvent(true);
    QueueEvent(event);
  }

  void QueuePinchEnd() {
    blink::WebGestureEvent event(
        blink::WebInputEvent::kGesturePinchEnd,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchpad);
    event.SetPositionInWidget(gfx::PointF(1, 1));
    event.SetPositionInScreen(gfx::PointF(1, 1));
    event.SetNeedsWheelEvent(true);
    QueueEvent(event);
  }

  void QueuePinchUpdate(float scale, bool zoom_disabled) {
    blink::WebGestureEvent event(
        blink::WebInputEvent::kGesturePinchUpdate,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchpad);
    event.SetPositionInWidget(gfx::PointF(1, 1));
    event.SetPositionInScreen(gfx::PointF(1, 1));
    event.SetNeedsWheelEvent(true);
    event.data.pinch_update.zoom_disabled = zoom_disabled;
    event.data.pinch_update.scale = scale;
    QueueEvent(event);
  }

  void QueueDoubleTap() {
    blink::WebGestureEvent event(
        blink::WebInputEvent::kGestureDoubleTap,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchpad);
    event.SetPositionInWidget(gfx::PointF(1, 1));
    event.SetPositionInScreen(gfx::PointF(1, 1));
    event.data.tap.tap_count = 1;
    event.SetNeedsWheelEvent(true);
    QueueEvent(event);
  }

  void SendWheelEventAck(InputEventAckSource ack_source,
                         InputEventAckState ack_result) {
    const MouseWheelEventWithLatencyInfo mouse_event_with_latency_info(
        queue_->get_wheel_event_awaiting_ack_for_testing(), ui::LatencyInfo());
    queue_->ProcessMouseWheelAck(ack_source, ack_result,
                                 mouse_event_with_latency_info);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  testing::StrictMock<MockTouchpadPinchEventQueueClient> mock_client_;
  std::unique_ptr<TouchpadPinchEventQueue> queue_;
  const bool async_events_enabled_;
};

INSTANTIATE_TEST_SUITE_P(, TouchpadPinchEventQueueTest, ::testing::Bool());

MATCHER_P(EventHasType,
          type,
          std::string(negation ? "does not have" : "has") + " type " +
              ::testing::PrintToString(type)) {
  return arg.event.GetType() == type;
}

MATCHER_P(EventHasPhase,
          phase,
          std::string(negation ? "does not have" : "has") + " phase " +
              ::testing::PrintToString(phase)) {
  return arg.event.phase == phase;
}

MATCHER_P(EventHasScale,
          expected_scale,
          std::string(negation ? "does not have" : "has") + " scale " +
              ::testing::PrintToString(expected_scale)) {
  const float actual_scale = exp(arg.event.delta_y / 100.0f);
  return ::testing::Matches(::testing::FloatEq(expected_scale))(actual_scale);
}

MATCHER(EventHasCtrlModifier,
        std::string(negation ? "does not have" : "has") + " control modifier") {
  return (arg.event.GetModifiers() & blink::WebInputEvent::kControlKey) != 0;
}

MATCHER(EventIsBlocking,
        std::string(negation ? "is not" : "is") + " blocking") {
  return arg.event.dispatch_type == blink::WebInputEvent::kBlocking;
}

// Ensure that when the queue receives a touchpad pinch sequence, it sends a
// synthetic mouse wheel event and acks the pinch events back to the client.
TEST_P(TouchpadPinchEventQueueTest, Basic) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchBegin),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  ::testing::AllOf(EventHasCtrlModifier(), EventIsBlocking())));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
                  InputEventAckSource::COMPOSITOR_THREAD,
                  INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchEnd),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  QueuePinchEnd();
  SendWheelEventAck(InputEventAckSource::BROWSER,
                    INPUT_EVENT_ACK_STATE_IGNORED);
}

// Ensure the queue sends the wheel events with phase information.
TEST_P(TouchpadPinchEventQueueTest, MouseWheelPhase) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchBegin),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  EventHasPhase(blink::WebMouseWheelEvent::kPhaseBegan)));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
                  InputEventAckSource::COMPOSITOR_THREAD,
                  INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  EventHasPhase(blink::WebMouseWheelEvent::kPhaseChanged)));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
                  testing::_, testing::_));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  EventHasPhase(blink::WebMouseWheelEvent::kPhaseEnded)));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchEnd),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  QueuePinchUpdate(1.23, false);
  if (async_events_enabled_) {
    SendWheelEventAck(InputEventAckSource::BROWSER,
                      INPUT_EVENT_ACK_STATE_IGNORED);
  } else {
    SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  }
  QueuePinchEnd();
  SendWheelEventAck(InputEventAckSource::BROWSER,
                    INPUT_EVENT_ACK_STATE_IGNORED);
}

// Ensure that if the renderer consumes the synthetic wheel event, the ack of
// the GesturePinchUpdate reflects this.
TEST_P(TouchpadPinchEventQueueTest, Consumed) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchBegin),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  ::testing::AllOf(EventHasCtrlModifier(), EventIsBlocking())));
  EXPECT_CALL(
      mock_client_,
      OnGestureEventForPinchAck(
          EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
          InputEventAckSource::MAIN_THREAD, INPUT_EVENT_ACK_STATE_CONSUMED));

  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))));

  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchEnd),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(InputEventAckSource::MAIN_THREAD,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  QueuePinchEnd();
  SendWheelEventAck(InputEventAckSource::BROWSER,
                    INPUT_EVENT_ACK_STATE_IGNORED);
}

// Ensure that the queue sends wheel events for updates with |zoom_disabled| as
// well.
TEST_P(TouchpadPinchEventQueueTest, ZoomDisabled) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchBegin),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  ::testing::AllOf(EventHasCtrlModifier(), EventIsBlocking())));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
                  InputEventAckSource::COMPOSITOR_THREAD,
                  INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS));

  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))));

  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchEnd),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, true);
  SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  QueuePinchEnd();
  SendWheelEventAck(InputEventAckSource::BROWSER,
                    INPUT_EVENT_ACK_STATE_IGNORED);
}

TEST_P(TouchpadPinchEventQueueTest, MultipleSequences) {
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchBegin),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED))
      .Times(2);
  EXPECT_CALL(mock_client_, SendMouseWheelEventForPinchImmediately(testing::_))
      .Times(4);
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
                  InputEventAckSource::COMPOSITOR_THREAD,
                  INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS))
      .Times(2);
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchEnd),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED))
      .Times(2);

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  QueuePinchEnd();
  SendWheelEventAck(InputEventAckSource::BROWSER,
                    INPUT_EVENT_ACK_STATE_IGNORED);

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  QueuePinchEnd();
  SendWheelEventAck(InputEventAckSource::BROWSER,
                    INPUT_EVENT_ACK_STATE_IGNORED);
}

// Ensure we can queue additional pinch event sequences while the queue is
// waiting for a wheel event ack.
TEST_P(TouchpadPinchEventQueueTest, MultipleQueuedSequences) {
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchBegin),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  EXPECT_CALL(mock_client_, SendMouseWheelEventForPinchImmediately(testing::_));
  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);

  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  QueuePinchEnd();

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  QueuePinchEnd();

  // No calls since we're still waiting on the ack for the first wheel event.
  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
                  InputEventAckSource::COMPOSITOR_THREAD,
                  INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchEnd),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchBegin),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  EXPECT_CALL(mock_client_, SendMouseWheelEventForPinchImmediately(testing::_))
      .Times(2);
  SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  // ACK for end event.
  SendWheelEventAck(InputEventAckSource::BROWSER,
                    INPUT_EVENT_ACK_STATE_IGNORED);

  // After acking the first wheel event, the queue continues.
  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
                  InputEventAckSource::COMPOSITOR_THREAD,
                  INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchEnd),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  EXPECT_CALL(mock_client_, SendMouseWheelEventForPinchImmediately(testing::_));
  SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  // ACK for end event.
  SendWheelEventAck(InputEventAckSource::BROWSER,
                    INPUT_EVENT_ACK_STATE_IGNORED);
}

// Ensure the queue handles pinch event sequences with multiple updates.
TEST_P(TouchpadPinchEventQueueTest, MultipleUpdatesInSequence) {
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchBegin),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  if (async_events_enabled_) {
    // Only first wheel event is cancelable.
    // Here the second and the third wheel events are not blocking because we
    // ack the first wheel event as unconsumed.
    EXPECT_CALL(mock_client_,
                SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                    EventHasCtrlModifier(), EventIsBlocking())));
    EXPECT_CALL(mock_client_,
                SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                    EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))))
        .Times(3);
  } else {
    EXPECT_CALL(mock_client_,
                SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                    EventHasCtrlModifier(), EventIsBlocking())))
        .Times(3);
    EXPECT_CALL(
        mock_client_,
        SendMouseWheelEventForPinchImmediately(::testing::AllOf(
            EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))));
  }
  if (async_events_enabled_) {
    EXPECT_CALL(mock_client_,
                OnGestureEventForPinchAck(
                    EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
                    InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS));
    EXPECT_CALL(
        mock_client_,
        OnGestureEventForPinchAck(
            EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
            InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED))
        .Times(2);
  } else {
    EXPECT_CALL(mock_client_,
                OnGestureEventForPinchAck(
                    EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
                    InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS))
        .Times(3);
  }
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchEnd),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  QueuePinchUpdate(1.23, false);
  if (async_events_enabled_) {
    SendWheelEventAck(InputEventAckSource::BROWSER,
                      INPUT_EVENT_ACK_STATE_IGNORED);
  } else {
    SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  }
  QueuePinchUpdate(1.23, false);
  if (async_events_enabled_) {
    SendWheelEventAck(InputEventAckSource::BROWSER,
                      INPUT_EVENT_ACK_STATE_IGNORED);
  } else {
    SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  }
  QueuePinchEnd();
  SendWheelEventAck(InputEventAckSource::BROWSER,
                    INPUT_EVENT_ACK_STATE_IGNORED);
}

// Ensure the queue coalesces pinch update events.
TEST_P(TouchpadPinchEventQueueTest, MultipleUpdatesCoalesced) {
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchBegin),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  if (async_events_enabled_) {
    // Only the first wheel event is cancelable.
    // Here the second wheel is not blocking because we ack the first wheel
    // event as unconsumed.
    EXPECT_CALL(mock_client_,
                SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                    EventHasCtrlModifier(), EventIsBlocking())));
    EXPECT_CALL(mock_client_,
                SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                    EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))))
        .Times(2);
  } else {
    EXPECT_CALL(mock_client_,
                SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                    EventHasCtrlModifier(), EventIsBlocking())))
        .Times(2);
    EXPECT_CALL(
        mock_client_,
        SendMouseWheelEventForPinchImmediately(::testing::AllOf(
            EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))));
  }
  if (async_events_enabled_) {
    EXPECT_CALL(mock_client_,
                OnGestureEventForPinchAck(
                    EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
                    InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS));
    EXPECT_CALL(
        mock_client_,
        OnGestureEventForPinchAck(
            EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
            InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  } else {
    EXPECT_CALL(mock_client_,
                OnGestureEventForPinchAck(
                    EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
                    InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS))
        .Times(2);
  }
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchEnd),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));

  QueuePinchBegin();
  // The queue will send the first wheel event for this first update.
  QueuePinchUpdate(1.23, false);
  // Before the first wheel event is acked, queue another update.
  QueuePinchUpdate(1.23, false);
  // Queue a third update. This will be coalesced with the second update which
  // is currently in the queue.
  QueuePinchUpdate(1.23, false);
  QueuePinchEnd();

  // Ack for the wheel event corresponding to the first update.
  SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  // Ack for the wheel event corresponding to the second and third updates.
  if (async_events_enabled_) {
    SendWheelEventAck(InputEventAckSource::BROWSER,
                      INPUT_EVENT_ACK_STATE_IGNORED);
  } else {
    SendWheelEventAck(InputEventAckSource::COMPOSITOR_THREAD,
                      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  }
  // ACK for end event.
  SendWheelEventAck(InputEventAckSource::BROWSER,
                    INPUT_EVENT_ACK_STATE_IGNORED);
  EXPECT_FALSE(queue_->has_pending());
}

// Ensure the queue handles pinch event sequences with multiple canceled
// updates.
TEST_P(TouchpadPinchEventQueueTest, MultipleCanceledUpdatesInSequence) {
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchBegin),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  ::testing::AllOf(EventHasCtrlModifier(), EventIsBlocking())))
      .Times(3);
  EXPECT_CALL(
      mock_client_,
      OnGestureEventForPinchAck(
          EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
          InputEventAckSource::MAIN_THREAD, INPUT_EVENT_ACK_STATE_CONSUMED))
      .Times(3);
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchEnd),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(InputEventAckSource::MAIN_THREAD,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(InputEventAckSource::MAIN_THREAD,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(InputEventAckSource::MAIN_THREAD,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  QueuePinchEnd();
  SendWheelEventAck(InputEventAckSource::BROWSER,
                    INPUT_EVENT_ACK_STATE_IGNORED);
}

// Ensure that when the queue receives a touchpad double tap, it sends a
// synthetic mouse wheel event and acks the double tap back to the client.
TEST_P(TouchpadPinchEventQueueTest, DoubleTap) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), EventIsBlocking(),
                  EventHasPhase(blink::WebMouseWheelEvent::kPhaseNone),
                  EventHasScale(1.0f))));
  EXPECT_CALL(
      mock_client_,
      OnGestureEventForPinchAck(
          EventHasType(blink::WebInputEvent::kGestureDoubleTap),
          InputEventAckSource::MAIN_THREAD, INPUT_EVENT_ACK_STATE_CONSUMED));

  QueueDoubleTap();
  SendWheelEventAck(InputEventAckSource::MAIN_THREAD,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
}

// Ensure that ACKs are only processed when they match the event that is
// currently awaiting an ACK.
TEST_P(TouchpadPinchEventQueueTest, IgnoreNonMatchingEvents) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchBegin),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  ::testing::AllOf(EventHasCtrlModifier(), EventIsBlocking())));
  EXPECT_CALL(
      mock_client_,
      OnGestureEventForPinchAck(
          EventHasType(blink::WebInputEvent::kGesturePinchUpdate),
          InputEventAckSource::MAIN_THREAD, INPUT_EVENT_ACK_STATE_CONSUMED));

  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))));

  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::kGesturePinchEnd),
                  InputEventAckSource::BROWSER, INPUT_EVENT_ACK_STATE_IGNORED));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  QueuePinchEnd();

  // Create a fake end event to give to ProcessMouseWheelAck to confirm that
  // it correctly filters this event out and doesn't start processing the ack.
  blink::WebMouseWheelEvent fake_end_event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kControlKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  fake_end_event.dispatch_type = blink::WebMouseWheelEvent::kBlocking;
  fake_end_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  const MouseWheelEventWithLatencyInfo fake_end_event_with_latency_info(
      fake_end_event, ui::LatencyInfo());
  queue_->ProcessMouseWheelAck(InputEventAckSource::MAIN_THREAD,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
                               fake_end_event_with_latency_info);

  SendWheelEventAck(InputEventAckSource::MAIN_THREAD,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  SendWheelEventAck(InputEventAckSource::BROWSER,
                    INPUT_EVENT_ACK_STATE_IGNORED);
}

}  // namespace content
