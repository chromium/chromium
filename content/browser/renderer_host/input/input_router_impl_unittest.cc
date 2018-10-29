// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_impl.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/input/touch_action.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/mock_input_disposition_handler.h"
#include "content/browser/renderer_host/input/mock_input_router_client.h"
#include "content/common/content_constants_internal.h"
#include "content/common/edit_command.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_widget_input_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/events/event.h"
#endif

using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebKeyboardEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::DidOverscrollParams;
using ui::WebInputEventTraits;

namespace content {

namespace {

bool ShouldBlockEventStream(const blink::WebInputEvent& event) {
  return ui::WebInputEventTraits::ShouldBlockEventStream(event);
}

WebInputEvent& GetEventWithType(WebInputEvent::Type type) {
  WebInputEvent* event = nullptr;
  if (WebInputEvent::IsMouseEventType(type)) {
    static WebMouseEvent mouse;
    event = &mouse;
  } else if (WebInputEvent::IsTouchEventType(type)) {
    static WebTouchEvent touch;
    event = &touch;
  } else if (WebInputEvent::IsKeyboardEventType(type)) {
    static WebKeyboardEvent key;
    event = &key;
  } else if (WebInputEvent::IsGestureEventType(type)) {
    static WebGestureEvent gesture;
    event = &gesture;
  } else if (type == WebInputEvent::kMouseWheel) {
    static WebMouseWheelEvent wheel;
    event = &wheel;
  }
  CHECK(event);
  event->SetType(type);
  return *event;
}

}  // namespace

// TODO(dtapuska): Remove this class when we don't have multiple implementations
// of InputRouters.
class MockInputRouterImplClient : public InputRouterImplClient {
 public:
  mojom::WidgetInputHandler* GetWidgetInputHandler() override {
    return &widget_input_handler_;
  }

  void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override {}

  void OnImeCancelComposition() override {}

  void SetMouseCapture(bool capture) override {}

  MockWidgetInputHandler::MessageVector GetAndResetDispatchedMessages() {
    return widget_input_handler_.GetAndResetDispatchedMessages();
  }

  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) override {
    return input_router_client_.FilterInputEvent(input_event, latency_info);
  }

  void IncrementInFlightEventCount() override {
    input_router_client_.IncrementInFlightEventCount();
  }

  void DecrementInFlightEventCount(InputEventAckSource ack_source) override {
    input_router_client_.DecrementInFlightEventCount(ack_source);
  }

  void DidOverscroll(const ui::DidOverscrollParams& params) override {
    input_router_client_.DidOverscroll(params);
  }

  void DidStartScrollingViewport() override {
    input_router_client_.DidStartScrollingViewport();
  }

  void ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& latency_info) override {
    input_router_client_.ForwardWheelEventWithLatencyInfo(wheel_event,
                                                          latency_info);
  }

  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency_info) override {
    input_router_client_.ForwardGestureEventWithLatencyInfo(gesture_event,
                                                            latency_info);
  }

  bool IsWheelScrollInProgress() override {
    return input_router_client_.IsWheelScrollInProgress();
  }

  void OnSetWhiteListedTouchAction(cc::TouchAction touch_action) override {
    input_router_client_.OnSetWhiteListedTouchAction(touch_action);
  }

  bool GetAndResetFilterEventCalled() {
    return input_router_client_.GetAndResetFilterEventCalled();
  }

  ui::DidOverscrollParams GetAndResetOverscroll() {
    return input_router_client_.GetAndResetOverscroll();
  }

  cc::TouchAction GetAndResetWhiteListedTouchAction() {
    return input_router_client_.GetAndResetWhiteListedTouchAction();
  }

  void set_input_router(InputRouter* input_router) {
    input_router_client_.set_input_router(input_router);
  }

  void set_filter_state(InputEventAckState filter_state) {
    input_router_client_.set_filter_state(filter_state);
  }
  int in_flight_event_count() const {
    return input_router_client_.in_flight_event_count();
  }
  int last_in_flight_event_type() const {
    return input_router_client_.last_in_flight_event_type();
  }
  void set_allow_send_event(bool allow) {
    input_router_client_.set_allow_send_event(allow);
  }
  const blink::WebInputEvent* last_filter_event() const {
    return input_router_client_.last_filter_event();
  }

  MockInputRouterClient input_router_client_;
  MockWidgetInputHandler widget_input_handler_;
};

class InputRouterImplTest : public testing::Test {
 public:
  InputRouterImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {
    vsync_feature_list_.InitAndEnableFeature(
        features::kVsyncAlignedInputEvents);
  }

  ~InputRouterImplTest() override {}

 protected:
  using DispatchedMessages = MockWidgetInputHandler::MessageVector;
  // testing::Test
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kValidateInputEventStream);
    client_.reset(new MockInputRouterImplClient());
    disposition_handler_.reset(new MockInputDispositionHandler());
    input_router_.reset(
        new InputRouterImpl(client_.get(), disposition_handler_.get(),
                            &client_->input_router_client_, config_));

    client_->set_input_router(input_router());
    disposition_handler_->set_input_router(input_router());
  }

  void TearDown() override {
    // Process all pending tasks to avoid leaks.
    base::RunLoop().RunUntilIdle();

    input_router_.reset();
    client_.reset();
  }

  void SetUpForTouchAckTimeoutTest(int desktop_timeout_ms,
                                   int mobile_timeout_ms) {
    config_.touch_config.desktop_touch_ack_timeout_delay =
        base::TimeDelta::FromMilliseconds(desktop_timeout_ms);
    config_.touch_config.mobile_touch_ack_timeout_delay =
        base::TimeDelta::FromMilliseconds(mobile_timeout_ms);
    config_.touch_config.touch_ack_timeout_supported = true;
    TearDown();
    SetUp();
    input_router()->NotifySiteIsMobileOptimized(false);
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type) {
    NativeWebKeyboardEventWithLatencyInfo key_event(
        type, WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
        ui::LatencyInfo());
    input_router_->SendKeyboardEvent(
        key_event, disposition_handler_->CreateKeyboardEventCallback());
  }

  void SimulateWheelEvent(float x,
                          float y,
                          float dX,
                          float dY,
                          int modifiers,
                          bool precise,
                          WebMouseWheelEvent::Phase phase) {
    WebMouseWheelEvent wheel_event = SyntheticWebMouseWheelEventBuilder::Build(
        x, y, dX, dY, modifiers, precise);
    wheel_event.phase = phase;
    input_router_->SendWheelEvent(MouseWheelEventWithLatencyInfo(wheel_event));
  }

  void SimulateWheelEvent(WebMouseWheelEvent::Phase phase) {
    input_router_->SendWheelEvent(MouseWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(phase)));
  }

  void SimulateMouseEvent(WebInputEvent::Type type, int x, int y) {
    input_router_->SendMouseEvent(
        MouseEventWithLatencyInfo(
            SyntheticWebMouseEventBuilder::Build(type, x, y, 0)),
        disposition_handler_->CreateMouseEventCallback());
  }

  void SimulateGestureEvent(WebGestureEvent gesture) {
    if (gesture.GetType() == WebInputEvent::kGestureScrollBegin &&
        gesture.SourceDevice() == blink::kWebGestureDeviceTouchscreen &&
        !gesture.data.scroll_begin.delta_x_hint &&
        !gesture.data.scroll_begin.delta_y_hint) {
      // Ensure non-zero scroll-begin offset-hint to make the event sane,
      // prevents unexpected filtering at TouchActionFilter.
      gesture.data.scroll_begin.delta_y_hint = 2.f;
    } else if (gesture.GetType() == WebInputEvent::kGestureFlingStart &&
               gesture.SourceDevice() == blink::kWebGestureDeviceTouchscreen &&
               !gesture.data.fling_start.velocity_x &&
               !gesture.data.fling_start.velocity_y) {
      // Ensure non-zero touchscreen fling velocities, as the router will
      // validate against such. The velocity should be large enough to make
      // sure that the fling is still active while sending the GFC.
      gesture.data.fling_start.velocity_x = 500.f;
    } else if (gesture.GetType() == WebInputEvent::kGestureFlingCancel) {
      // Set prevent boosting to make sure that the GFC cancels the active
      // fling.
      gesture.data.fling_cancel.prevent_boosting = true;
    }

    input_router_->SendGestureEvent(GestureEventWithLatencyInfo(gesture));
  }

  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureDevice source_device) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, source_device));
  }

  void SimulateGestureScrollUpdateEvent(float dX,
                                        float dY,
                                        int modifiers,
                                        WebGestureDevice source_device) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollUpdate(
        dX, dY, modifiers, source_device));
  }

  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchor_x,
                                       float anchor_y,
                                       int modifiers,
                                       WebGestureDevice source_device) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildPinchUpdate(
        scale, anchor_x, anchor_y, modifiers, source_device));
  }

  void SimulateTouchpadGesturePinchEventWithoutWheel(WebInputEvent::Type type,
                                                     float scale,
                                                     float anchor_x,
                                                     float anchor_y,
                                                     int modifiers) {
    DCHECK(blink::WebInputEvent::IsPinchGestureEventType(type));
    WebGestureEvent event =
        (type == blink::WebInputEvent::kGesturePinchUpdate
             ? SyntheticWebGestureEventBuilder::BuildPinchUpdate(
                   scale, anchor_x, anchor_y, modifiers,
                   blink::kWebGestureDeviceTouchpad)
             : SyntheticWebGestureEventBuilder::Build(
                   type, blink::kWebGestureDeviceTouchpad));
    // For touchpad pinch, we first send wheel events to the renderer. Only
    // after these have been acknowledged do we send the actual gesture pinch
    // events to the renderer. We indicate here that the wheel sending phase is
    // done for the purpose of testing the sending of the gesture events
    // themselves.
    event.SetNeedsWheelEvent(false);
    SimulateGestureEvent(event);
  }

  void SimulateGestureFlingStartEvent(float velocity_x,
                                      float velocity_y,
                                      WebGestureDevice source_device) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildFling(
        velocity_x, velocity_y, source_device));
  }

  void SetTouchTimestamp(base::TimeTicks timestamp) {
    touch_event_.SetTimestamp(timestamp);
  }

  uint32_t SendTouchEvent() {
    uint32_t touch_event_id = touch_event_.unique_touch_event_id;
    input_router_->SendTouchEvent(TouchEventWithLatencyInfo(touch_event_));
    touch_event_.ResetPoints();
    return touch_event_id;
  }

  int PressTouchPoint(int x, int y) { return touch_event_.PressPoint(x, y); }

  void MoveTouchPoint(int index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
  }

  void ReleaseTouchPoint(int index) { touch_event_.ReleasePoint(index); }

  void CancelTouchPoint(int index) { touch_event_.CancelPoint(index); }

  InputRouterImpl* input_router() const { return input_router_.get(); }

  bool TouchEventQueueEmpty() const {
    return input_router()->touch_event_queue_.Empty();
  }

  bool TouchEventTimeoutEnabled() const {
    return input_router()->touch_event_queue_.IsAckTimeoutEnabled();
  }

  bool HasPendingEvents() const { return input_router_->HasPendingEvents(); }

  void OnHasTouchEventHandlers(bool has_handlers) {
    input_router_->OnHasTouchEventHandlers(has_handlers);
  }

  void CancelTouchTimeout() { input_router_->CancelTouchTimeout(); }

  void OnSetWhiteListedTouchAction(cc::TouchAction white_listed_touch_action,
                                   uint32_t unique_touch_event_id,
                                   InputEventAckState ack_result) {
    input_router_->SetWhiteListedTouchAction(white_listed_touch_action,
                                             unique_touch_event_id, ack_result);
  }

  void ResetTouchAction() {
    input_router_->touch_action_filter_.ResetTouchAction();
  }

  DispatchedMessages GetAndResetDispatchedMessages() {
    return client_->GetAndResetDispatchedMessages();
  }

  static void RunTasksAndWait(base::TimeDelta delay) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delay);
    run_loop.Run();
  }

  void PressAndSetTouchActionAuto() {
    PressTouchPoint(1, 1);
    SendTouchEvent();
    input_router_->OnSetTouchAction(cc::kTouchActionAuto);
    GetAndResetDispatchedMessages();
    disposition_handler_->GetAndResetAckCount();
  }

  void ActiveTouchSequenceCountTest(
      const base::Optional<cc::TouchAction>& touch_action,
      InputEventAckState state) {
    PressTouchPoint(1, 1);
    base::Optional<ui::DidOverscrollParams> overscroll;
    input_router_->SendTouchEvent(TouchEventWithLatencyInfo(touch_event_));
    input_router_->TouchEventHandled(
        TouchEventWithLatencyInfo(touch_event_), InputEventAckSource::BROWSER,
        ui::LatencyInfo(), state, overscroll, touch_action);
    EXPECT_EQ(input_router_->touch_action_filter_.num_of_active_touches_, 1);
    ReleaseTouchPoint(0);
    input_router_->OnTouchEventAck(TouchEventWithLatencyInfo(touch_event_),
                                   InputEventAckSource::BROWSER, state);
    EXPECT_EQ(input_router_->touch_action_filter_.num_of_active_touches_, 0);
  }

  void OnTouchEventAckWithAckState(InputEventAckState ack_state) {
    input_router_->OnHasTouchEventHandlers(true);
    EXPECT_FALSE(input_router_->AllowedTouchAction().has_value());
    PressTouchPoint(1, 1);
    input_router_->SendTouchEvent(TouchEventWithLatencyInfo(touch_event_));
    input_router_->OnTouchEventAck(TouchEventWithLatencyInfo(touch_event_),
                                   InputEventAckSource::BROWSER, ack_state);
    EXPECT_EQ(input_router_->AllowedTouchAction().value(),
              cc::kTouchActionAuto);
  }

  InputRouter::Config config_;
  std::unique_ptr<MockInputRouterImplClient> client_;
  std::unique_ptr<InputRouterImpl> input_router_;
  std::unique_ptr<MockInputDispositionHandler> disposition_handler_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  SyntheticWebTouchEvent touch_event_;

  base::test::ScopedFeatureList vsync_feature_list_;
};

TEST_F(InputRouterImplTest, HandledInputEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure no input event is sent to the renderer.
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_messages.size());

  // OnKeyboardEventAck should be triggered without actual ack.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}

TEST_F(InputRouterImplTest, ClientCanceledKeyboardEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // Simulate a keyboard event that has no consumer.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure no input event is sent to the renderer.
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_messages.size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // Simulate a keyboard event that should be dropped.
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_UNKNOWN);
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure no input event is sent to the renderer, and no ack is sent.
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_messages.size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
}

// Tests ported from RenderWidgetHostTest --------------------------------------

TEST_F(InputRouterImplTest, HandleKeyEventsWeSent) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure we sent the input event to the renderer.
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            disposition_handler_->acked_keyboard_event().GetType());
}

TEST_F(InputRouterImplTest, CoalescesWheelEvents) {
  // Simulate wheel events.
  SimulateWheelEvent(0, 0, 0, -5, 0, false,
                     WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEvent(0, 0, 0, -10, 0, false,
                     WebMouseWheelEvent::kPhaseChanged);  // enqueued
  SimulateWheelEvent(
      0, 0, 8, -6, 0, false,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEvent(
      0, 0, 9, -7, 1, false,
      WebMouseWheelEvent::kPhaseChanged);  // enqueued, different modifiers
  SimulateWheelEvent(
      0, 0, 0, -10, 0, false,
      WebMouseWheelEvent::kPhaseChanged);  // enqueued, different modifiers
  // Explicitly verify that PhaseEnd isn't coalesced to avoid bugs like
  // https://crbug.com/154740.
  SimulateWheelEvent(WebMouseWheelEvent::kPhaseEnded);  // enqueued

  // Check that only the first event was sent.
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_messages[0]->ToEvent()->Event()->web_event->GetType());
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          dispatched_messages[0]->ToEvent()->Event()->web_event.get());
  EXPECT_EQ(0, wheel_event->delta_x);
  EXPECT_EQ(-5, wheel_event->delta_y);

  // Check that the ACK sends the second message immediately.
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  // The coalesced events can queue up a delayed ack
  // so that additional input events can be processed before
  // we turn off coalescing.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_messages[0]->ToEvent()->Event()->web_event->GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      dispatched_messages[0]->ToEvent()->Event()->web_event.get());
  EXPECT_EQ(8, wheel_event->delta_x);
  EXPECT_EQ(-10 + -6, wheel_event->delta_y);  // coalesced

  // Ack the second event (which had the third coalesced into it).
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_messages[0]->ToEvent()->Event()->web_event->GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      dispatched_messages[0]->ToEvent()->Event()->web_event.get());
  EXPECT_EQ(9, wheel_event->delta_x);
  EXPECT_EQ(-7, wheel_event->delta_y);

  // Ack the fourth event.
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_messages[0]->ToEvent()->Event()->web_event->GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      dispatched_messages[0]->ToEvent()->Event()->web_event.get());
  EXPECT_EQ(0, wheel_event->delta_x);
  EXPECT_EQ(-10, wheel_event->delta_y);

  // Ack the fifth event.
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_messages[0]->ToEvent()->Event()->web_event->GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      dispatched_messages[0]->ToEvent()->Event()->web_event.get());
  EXPECT_EQ(0, wheel_event->delta_x);
  EXPECT_EQ(0, wheel_event->delta_y);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);

  // After the final ack, the queue should be empty.
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_messages.size());
}

// Test that the active touch sequence count increment when the touch start is
// not ACKed from the main thread.
TEST_F(InputRouterImplTest, ActiveTouchSequenceCountWithoutTouchAction) {
  base::Optional<cc::TouchAction> touch_action;
  ActiveTouchSequenceCountTest(touch_action,
                               INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
}

TEST_F(InputRouterImplTest,
       ActiveTouchSequenceCountWithoutTouchActionNoConsumer) {
  base::Optional<cc::TouchAction> touch_action;
  ActiveTouchSequenceCountTest(touch_action,
                               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
}

// Test that the active touch sequence count increment when the touch start is
// ACKed from the main thread.
TEST_F(InputRouterImplTest, ActiveTouchSequenceCountWithTouchAction) {
  base::Optional<cc::TouchAction> touch_action(cc::kTouchActionPanY);
  ActiveTouchSequenceCountTest(touch_action,
                               INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
}

TEST_F(InputRouterImplTest, ActiveTouchSequenceCountWithTouchActionNoConsumer) {
  base::Optional<cc::TouchAction> touch_action(cc::kTouchActionPanY);
  ActiveTouchSequenceCountTest(touch_action,
                               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateConsumed) {
  OnTouchEventAckWithAckState(INPUT_EVENT_ACK_STATE_CONSUMED);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateNotConsumed) {
  OnTouchEventAckWithAckState(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateConsumedShouldBubble) {
  OnTouchEventAckWithAckState(INPUT_EVENT_ACK_STATE_CONSUMED_SHOULD_BUBBLE);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateNoConsumerExists) {
  OnTouchEventAckWithAckState(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateIgnored) {
  OnTouchEventAckWithAckState(INPUT_EVENT_ACK_STATE_IGNORED);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateNonBlocking) {
  OnTouchEventAckWithAckState(INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateNonBlockingDueToFling) {
  OnTouchEventAckWithAckState(
      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING);
}

// Tests that touch-events are sent properly.
TEST_F(InputRouterImplTest, TouchEventQueue) {
  OnHasTouchEventHandlers(true);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  DispatchedMessages touch_start_event = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, touch_start_event.size());
  ASSERT_TRUE(touch_start_event[0]->ToEvent());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // The second touch should be sent right away.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  DispatchedMessages touch_move_event = GetAndResetDispatchedMessages();
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  ASSERT_EQ(1U, touch_move_event.size());
  ASSERT_TRUE(touch_move_event[0]->ToEvent());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // Receive an ACK for the first touch-event.
  touch_start_event[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kTouchStart,
            disposition_handler_->acked_touch_event().event.GetType());
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  touch_move_event[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kTouchMove,
            disposition_handler_->acked_touch_event().event.GetType());
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
}

// Tests that the touch-queue is emptied after a page stops listening for touch
// events and the outstanding ack is received.
TEST_F(InputRouterImplTest, TouchEventQueueFlush) {
  OnHasTouchEventHandlers(true);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  EXPECT_TRUE(TouchEventQueueEmpty());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  MoveTouchPoint(0, 2, 2);
  MoveTouchPoint(0, 3, 3);
  EXPECT_FALSE(TouchEventQueueEmpty());
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());

  // The page stops listening for touch-events. Note that flushing is deferred
  // until the outstanding ack is received.
  OnHasTouchEventHandlers(false);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // After the ack, the touch-event queue should be empty, and none of the
  // flushed touch-events should have been sent to the renderer.
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  EXPECT_TRUE(TouchEventQueueEmpty());
}

TEST_F(InputRouterImplTest, UnhandledWheelEvent) {
  // Simulate wheel events.
  SimulateWheelEvent(0, 0, 0, -5, 0, false, WebMouseWheelEvent::kPhaseBegan);
  SimulateWheelEvent(0, 0, 0, -10, 0, false, WebMouseWheelEvent::kPhaseChanged);

  // Check that only the first event was sent.
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());

  // Indicate that the wheel event was unhandled.
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // There should be a ScrollBegin, ScrollUpdate, second MouseWheel, and second
  // ScrollUpdate sent.
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(4U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_TRUE(dispatched_messages[1]->ToEvent());
  ASSERT_TRUE(dispatched_messages[2]->ToEvent());
  ASSERT_TRUE(dispatched_messages[3]->ToEvent());
  ASSERT_EQ(WebInputEvent::kGestureScrollBegin,
            dispatched_messages[0]->ToEvent()->Event()->web_event->GetType());
  ASSERT_EQ(WebInputEvent::kGestureScrollUpdate,
            dispatched_messages[1]->ToEvent()->Event()->web_event->GetType());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_messages[2]->ToEvent()->Event()->web_event->GetType());
  ASSERT_EQ(WebInputEvent::kGestureScrollUpdate,
            dispatched_messages[3]->ToEvent()->Event()->web_event->GetType());

  // Indicate that the GestureScrollBegin event was consumed.
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the ack for the first MouseWheel, ScrollBegin, and the second
  // MouseWheel were processed.
  EXPECT_EQ(3U, disposition_handler_->GetAndResetAckCount());

  // The last acked wheel event should be the second one since the input router
  // has already sent the immediate ack for the second wheel event.
  EXPECT_EQ(disposition_handler_->acked_wheel_event().delta_y, -10);
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_IGNORED,
            disposition_handler_->acked_wheel_event_state());

  // Ack the first gesture scroll update.
  dispatched_messages[1]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the ack for the first ScrollUpdate were processed.
  EXPECT_EQ(
      -5,
      disposition_handler_->acked_gesture_event().data.scroll_update.delta_y);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // Ack the second gesture scroll update.
  dispatched_messages[3]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the ack for the second ScrollUpdate were processed.
  EXPECT_EQ(
      -10,
      disposition_handler_->acked_gesture_event().data.scroll_update.delta_y);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}

TEST_F(InputRouterImplTest, TouchTypesIgnoringAck) {
  OnHasTouchEventHandlers(true);
  // Only acks for TouchCancel should always be ignored.
  ASSERT_TRUE(
      ShouldBlockEventStream(GetEventWithType(WebInputEvent::kTouchStart)));
  ASSERT_TRUE(
      ShouldBlockEventStream(GetEventWithType(WebInputEvent::kTouchMove)));
  ASSERT_TRUE(
      ShouldBlockEventStream(GetEventWithType(WebInputEvent::kTouchEnd)));

  // Precede the TouchCancel with an appropriate TouchStart;
  PressTouchPoint(1, 1);
  SendTouchEvent();
  input_router_->OnSetTouchAction(cc::kTouchActionAuto);
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  ASSERT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  ASSERT_EQ(0, client_->in_flight_event_count());

  // The TouchCancel has no callback.
  CancelTouchPoint(0);
  SendTouchEvent();
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
  EXPECT_FALSE(HasPendingEvents());
  EXPECT_FALSE(dispatched_messages[0]->ToEvent()->HasCallback());
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_FALSE(HasPendingEvents());
}

// TODO(https://crbug.com/866946): Test is flaky on Fuchsia.
#if defined(OS_FUCHSIA)
#define MAYBE_GestureTypesIgnoringAck DISABLED_GestureTypesIgnoringAck
#else
#define MAYBE_GestureTypesIgnoringAck GestureTypesIgnoringAck
#endif
TEST_F(InputRouterImplTest, MAYBE_GestureTypesIgnoringAck) {
  // We test every gesture type, ensuring that the stream of gestures is valid.
  const WebInputEvent::Type eventTypes[] = {
      WebInputEvent::kGestureTapDown,     WebInputEvent::kGestureShowPress,
      WebInputEvent::kGestureTapCancel,   WebInputEvent::kGestureScrollBegin,
      WebInputEvent::kGestureFlingStart,  WebInputEvent::kGestureFlingCancel,
      WebInputEvent::kGestureTapDown,     WebInputEvent::kGestureTap,
      WebInputEvent::kGestureTapDown,     WebInputEvent::kGestureLongPress,
      WebInputEvent::kGestureTapCancel,   WebInputEvent::kGestureLongTap,
      WebInputEvent::kGestureTapDown,     WebInputEvent::kGestureTapUnconfirmed,
      WebInputEvent::kGestureTapCancel,   WebInputEvent::kGestureTapDown,
      WebInputEvent::kGestureDoubleTap,   WebInputEvent::kGestureTapDown,
      WebInputEvent::kGestureTapCancel,   WebInputEvent::kGestureTwoFingerTap,
      WebInputEvent::kGestureTapDown,     WebInputEvent::kGestureTapCancel,
      WebInputEvent::kGestureScrollBegin, WebInputEvent::kGestureScrollUpdate,
      WebInputEvent::kGesturePinchBegin,  WebInputEvent::kGesturePinchUpdate,
      WebInputEvent::kGesturePinchEnd,    WebInputEvent::kGestureScrollEnd};
  for (size_t i = 0; i < arraysize(eventTypes); ++i) {
    WebInputEvent::Type type = eventTypes[i];
    if (ShouldBlockEventStream(GetEventWithType(type))) {
      PressAndSetTouchActionAuto();
      SimulateGestureEvent(type, blink::kWebGestureDeviceTouchscreen);
      DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();

      if (type != WebInputEvent::kGestureFlingStart &&
          type != WebInputEvent::kGestureFlingCancel) {
        if (type == WebInputEvent::kGestureScrollUpdate)
          EXPECT_EQ(2U, dispatched_messages.size());
        else
          EXPECT_EQ(1U, dispatched_messages.size());
        EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
        EXPECT_EQ(1, client_->in_flight_event_count());
        EXPECT_TRUE(HasPendingEvents());
        ASSERT_TRUE(
            dispatched_messages[dispatched_messages.size() - 1]->ToEvent());

        dispatched_messages[dispatched_messages.size() - 1]
            ->ToEvent()
            ->CallCallback(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      }

      EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
      if (type == WebInputEvent::kGestureFlingCancel) {
        // fling controller generates and sends a GSE while handling the GFC.
        EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
      } else {
        EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
      }
      EXPECT_EQ(0, client_->in_flight_event_count());
      EXPECT_FALSE(HasPendingEvents());
      continue;

      ReleaseTouchPoint(0);
      SendTouchEvent();
      GetAndResetDispatchedMessages();
    }

    SimulateGestureEvent(type, blink::kWebGestureDeviceTouchscreen);
    EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
    EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
    EXPECT_EQ(0, client_->in_flight_event_count());
    EXPECT_FALSE(HasPendingEvents());
  }
}

TEST_F(InputRouterImplTest, MouseTypesIgnoringAck) {
  int start_type = static_cast<int>(WebInputEvent::kMouseDown);
  int end_type = static_cast<int>(WebInputEvent::kContextMenu);
  ASSERT_LT(start_type, end_type);
  for (int i = start_type; i <= end_type; ++i) {
    WebInputEvent::Type type = static_cast<WebInputEvent::Type>(i);

    SimulateMouseEvent(type, 0, 0);
    DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
    ASSERT_EQ(1U, dispatched_messages.size());
    ASSERT_TRUE(dispatched_messages[0]->ToEvent());

    if (ShouldBlockEventStream(GetEventWithType(type))) {
      EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(1, client_->in_flight_event_count());

      dispatched_messages[0]->ToEvent()->CallCallback(
          INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
      EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(0, client_->in_flight_event_count());
    } else {
      // Note: events which don't block the event stream immediately receive
      // synthetic ACKs.
      EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(0, client_->in_flight_event_count());
    }
  }
}

// Guard against breaking changes to the list of ignored event ack types in
// |WebInputEventTraits::ShouldBlockEventStream|.
TEST_F(InputRouterImplTest, RequiredEventAckTypes) {
  const WebInputEvent::Type kRequiredEventAckTypes[] = {
      WebInputEvent::kMouseMove,
      WebInputEvent::kMouseWheel,
      WebInputEvent::kRawKeyDown,
      WebInputEvent::kKeyDown,
      WebInputEvent::kKeyUp,
      WebInputEvent::kChar,
      WebInputEvent::kGestureScrollUpdate,
      WebInputEvent::kGestureFlingStart,
      WebInputEvent::kGestureFlingCancel,
      WebInputEvent::kGesturePinchUpdate,
      WebInputEvent::kTouchStart,
      WebInputEvent::kTouchMove};
  for (size_t i = 0; i < arraysize(kRequiredEventAckTypes); ++i) {
    const WebInputEvent::Type required_ack_type = kRequiredEventAckTypes[i];
    ASSERT_TRUE(ShouldBlockEventStream(GetEventWithType(required_ack_type)));
  }
}

TEST_F(InputRouterImplTest, GestureTypesIgnoringAckInterleaved) {
  // Interleave a few events that do and do not ignore acks. All gesture events
  // should be dispatched immediately, but the acks will be blocked on blocking
  // events.
  PressAndSetTouchActionAuto();
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  // Should have sent |kTouchScrollStarted| and |kGestureScrollUpdate|.
  EXPECT_EQ(2U, dispatched_messages.size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            client_->last_in_flight_event_type());

  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::kGestureTapDown,
            client_->last_in_flight_event_type());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  DispatchedMessages temp_dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, temp_dispatched_messages.size());
  dispatched_messages.emplace_back(std::move(temp_dispatched_messages.at(0)));
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::kGestureShowPress,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::kGestureShowPress,
            client_->last_in_flight_event_type());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  temp_dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, temp_dispatched_messages.size());
  dispatched_messages.emplace_back(std::move(temp_dispatched_messages.at(0)));
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(3, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            client_->last_in_flight_event_type());

  SimulateGestureEvent(WebInputEvent::kGestureTapCancel,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(3, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::kGestureTapCancel,
            client_->last_in_flight_event_type());

  // Now ack each ack-respecting event. Should see in-flight event count
  // decreasing and additional acks coming back.
  // Ack the first GestureScrollUpdate, note that |at(0)| is TouchScrollStarted.
  ASSERT_EQ(4U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_TRUE(dispatched_messages[1]->ToEvent());
  ASSERT_TRUE(dispatched_messages[2]->ToEvent());
  ASSERT_TRUE(dispatched_messages[3]->ToEvent());
  dispatched_messages[1]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2, client_->in_flight_event_count());

  // Ack the second GestureScrollUpdate
  dispatched_messages[2]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Ack the last GestureScrollUpdate
  dispatched_messages[3]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());

  ReleaseTouchPoint(0);
  SendTouchEvent();
}

// Test that GestureShowPress events don't get out of order due to
// ignoring their acks.
TEST_F(InputRouterImplTest, GestureShowPressIsInOrder) {
  PressAndSetTouchActionAuto();
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // GesturePinchBegin ignores its ack.
  SimulateGestureEvent(WebInputEvent::kGesturePinchBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // GesturePinchUpdate waits for an ack.
  // This also verifies that GesturePinchUpdates for touchscreen are sent
  // to the renderer (in contrast to the TrackpadPinchUpdate test).
  SimulateGestureEvent(WebInputEvent::kGesturePinchUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            client_->last_in_flight_event_type());

  // GestureShowPress will be sent immediately since GestureEventQueue allows
  // multiple in-flight events. However the acks will be blocked on outstanding
  // in-flight events.
  SimulateGestureEvent(WebInputEvent::kGestureShowPress,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::kGestureShowPress,
            client_->last_in_flight_event_type());

  SimulateGestureEvent(WebInputEvent::kGestureShowPress,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::kGestureShowPress,
            client_->last_in_flight_event_type());

  // Ack the GesturePinchUpdate to release two GestureShowPress ack.
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(3U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());

  ReleaseTouchPoint(0);
  SendTouchEvent();
}

// Test that touch ack timeout behavior is properly configured for
// mobile-optimized sites and allowed touch actions.
TEST_F(InputRouterImplTest, TouchAckTimeoutConfigured) {
  const int kDesktopTimeoutMs = 1;
  const int kMobileTimeoutMs = 0;
  SetUpForTouchAckTimeoutTest(kDesktopTimeoutMs, kMobileTimeoutMs);
  ASSERT_TRUE(TouchEventTimeoutEnabled());

  // Verify that the touch ack timeout fires upon the delayed ack.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(kDesktopTimeoutMs + 1));

  // The timed-out event should have been ack'ed.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  // Ack'ing the timed-out event should fire a TouchCancel.
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());

  // The remainder of the touch sequence should be dropped.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  PressAndSetTouchActionAuto();
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  ASSERT_TRUE(TouchEventTimeoutEnabled());

  // A mobile-optimized site should use the mobile timeout. For this test that
  // timeout value is 0, which disables the timeout.
  input_router()->NotifySiteIsMobileOptimized(true);
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  input_router()->NotifySiteIsMobileOptimized(false);
  EXPECT_TRUE(TouchEventTimeoutEnabled());

  // kTouchActionNone (and no other touch-action) should disable the timeout.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedMessages touch_press_event2 = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, touch_press_event2.size());
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  ReleaseTouchPoint(0);
  SendTouchEvent();
  DispatchedMessages touch_release_event2 = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, touch_release_event2.size());
  ASSERT_TRUE(touch_release_event2[0]->ToEvent());
  touch_press_event2[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  touch_release_event2[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  input_router_->OnSetTouchAction(cc::kTouchActionNone);
  DispatchedMessages touch_press_event3 = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, touch_press_event3.size());
  ASSERT_TRUE(touch_press_event3[0]->ToEvent());
  CancelTouchTimeout();
  EXPECT_FALSE(TouchEventTimeoutEnabled());
  ReleaseTouchPoint(0);
  SendTouchEvent();
  DispatchedMessages touch_release_event3 = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, touch_release_event3.size());
  ASSERT_TRUE(touch_release_event3[0]->ToEvent());
  touch_press_event3[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  touch_release_event3[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);

  // As the touch-action is reset by a new touch sequence, the timeout behavior
  // should be restored.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  ResetTouchAction();
  input_router_->OnSetTouchAction(cc::kTouchActionAuto);
  EXPECT_TRUE(TouchEventTimeoutEnabled());
}

// Test that a touch sequenced preceded by kTouchActionNone is not affected by
// the touch timeout.
TEST_F(InputRouterImplTest,
       TouchAckTimeoutDisabledForTouchSequenceAfterTouchActionNone) {
  const int kDesktopTimeoutMs = 1;
  const int kMobileTimeoutMs = 2;
  SetUpForTouchAckTimeoutTest(kDesktopTimeoutMs, kMobileTimeoutMs);
  ASSERT_TRUE(TouchEventTimeoutEnabled());
  OnHasTouchEventHandlers(true);

  // Start a touch sequence.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());

  // kTouchActionNone should disable the timeout.
  CancelTouchTimeout();
  dispatched_messages[0]->ToEvent()->CallCallback(
      InputEventAckSource::COMPOSITOR_THREAD, ui::LatencyInfo(),
      INPUT_EVENT_ACK_STATE_CONSUMED, base::nullopt, cc::kTouchActionNone);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  MoveTouchPoint(0, 1, 2);
  SendTouchEvent();
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_FALSE(TouchEventTimeoutEnabled());
  EXPECT_EQ(1U, dispatched_messages.size());

  // Delay the move ack. The timeout should not fire.
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(kDesktopTimeoutMs + 1));
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // End the touch sequence.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  ResetTouchAction();
  input_router_->OnSetTouchAction(cc::kTouchActionAuto);
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  disposition_handler_->GetAndResetAckCount();
  GetAndResetDispatchedMessages();

  // Start another touch sequence.  This should restore the touch timeout.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  // Wait for the touch ack timeout to fire.
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(kDesktopTimeoutMs + 1));
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}

// Test that TouchActionFilter::ResetTouchAction is called before the
// first touch event for a touch sequence reaches the renderer.
TEST_F(InputRouterImplTest, TouchActionResetBeforeEventReachesRenderer) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedMessages touch_press_event1 = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, touch_press_event1.size());
  ASSERT_TRUE(touch_press_event1[0]->ToEvent());
  CancelTouchTimeout();
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  DispatchedMessages touch_move_event1 = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, touch_move_event1.size());
  ASSERT_TRUE(touch_move_event1[0]->ToEvent());
  ReleaseTouchPoint(0);
  SendTouchEvent();
  DispatchedMessages touch_release_event1 = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, touch_release_event1.size());
  ASSERT_TRUE(touch_release_event1[0]->ToEvent());

  // Sequence 2.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedMessages touch_press_event2 = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, touch_press_event2.size());
  ASSERT_TRUE(touch_press_event2[0]->ToEvent());
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  DispatchedMessages touch_move_event2 = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, touch_move_event2.size());
  ReleaseTouchPoint(0);
  SendTouchEvent();
  DispatchedMessages touch_release_event2 = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, touch_release_event2.size());
  ASSERT_TRUE(touch_release_event2[0]->ToEvent());

  touch_press_event1[0]->ToEvent()->CallCallback(
      InputEventAckSource::COMPOSITOR_THREAD, ui::LatencyInfo(),
      INPUT_EVENT_ACK_STATE_CONSUMED, base::nullopt, cc::kTouchActionNone);
  touch_move_event1[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Ensure touch action is still none, as the next touch start hasn't been
  // acked yet. ScrollBegin and ScrollEnd don't require acks.
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  // This allows the next touch sequence to start.
  touch_release_event1[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);

  // Ensure touch action has been set to auto, as a new touch sequence has
  // started.
  touch_press_event2[0]->ToEvent()->CallCallback(
      InputEventAckSource::COMPOSITOR_THREAD, ui::LatencyInfo(),
      INPUT_EVENT_ACK_STATE_CONSUMED, base::nullopt, cc::kTouchActionAuto);
  touch_press_event2[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  touch_move_event2[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  DispatchedMessages gesture_scroll_begin = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, gesture_scroll_begin.size());
  gesture_scroll_begin[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  touch_release_event2[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
}

// Test that TouchActionFilter::ResetTouchAction is called when a new touch
// sequence has no consumer.
TEST_F(InputRouterImplTest, TouchActionResetWhenTouchHasNoConsumer) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedMessages touch_press_event1 = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, touch_press_event1.size());
  ASSERT_TRUE(touch_press_event1[0]->ToEvent());
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  DispatchedMessages touch_move_event1 = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, touch_move_event1.size());
  ASSERT_TRUE(touch_move_event1[0]->ToEvent());
  CancelTouchTimeout();
  touch_press_event1[0]->ToEvent()->CallCallback(
      InputEventAckSource::COMPOSITOR_THREAD, ui::LatencyInfo(),
      INPUT_EVENT_ACK_STATE_CONSUMED, base::nullopt, cc::kTouchActionNone);
  touch_move_event1[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);

  ReleaseTouchPoint(0);
  SendTouchEvent();
  DispatchedMessages touch_release_event1 = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, touch_release_event1.size());
  ASSERT_TRUE(touch_release_event1[0]->ToEvent());

  // Sequence 2
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedMessages touch_press_event2 = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, touch_press_event2.size());
  ASSERT_TRUE(touch_press_event2[0]->ToEvent());
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(2U, GetAndResetDispatchedMessages().size());

  // Ensure we have touch-action:none. ScrollBegin and ScrollEnd don't require
  // acks.
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  touch_release_event1[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  touch_press_event2[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  PressAndSetTouchActionAuto();
  // Ensure touch action has been set to auto, as the touch had no consumer.
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
}

// Test that TouchActionFilter::ResetTouchAction is called when the touch
// handler is removed.
TEST_F(InputRouterImplTest, TouchActionResetWhenTouchHandlerRemoved) {
  // Touch sequence with touch handler.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  CancelTouchTimeout();
  ReleaseTouchPoint(0);
  SendTouchEvent();
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(3U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_TRUE(dispatched_messages[1]->ToEvent());
  ASSERT_TRUE(dispatched_messages[2]->ToEvent());

  // Ensure we have touch-action:none, suppressing scroll events.
  dispatched_messages[0]->ToEvent()->CallCallback(
      InputEventAckSource::COMPOSITOR_THREAD, ui::LatencyInfo(),
      INPUT_EVENT_ACK_STATE_CONSUMED, base::nullopt, cc::kTouchActionNone);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  dispatched_messages[1]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  dispatched_messages[2]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  // Sequence without a touch handler. Note that in this case, the view may not
  // necessarily forward touches to the router (as no touch handler exists).
  OnHasTouchEventHandlers(false);

  // Ensure touch action has been set to auto, as the touch handler has been
  // removed.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
}

// Tests that async touch-moves are ack'd from the browser side.
TEST_F(InputRouterImplTest, AsyncTouchMoveAckedImmediately) {
  OnHasTouchEventHandlers(true);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  input_router_->OnSetTouchAction(cc::kTouchActionAuto);
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // Receive an ACK for the first touch-event.
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2U, GetAndResetDispatchedMessages().size());

  // Now send an async move.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
}

// Test that the double tap gesture depends on the touch action of the first
// tap.
TEST_F(InputRouterImplTest, DoubleTapGestureDependsOnFirstTap) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  CancelTouchTimeout();
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      InputEventAckSource::COMPOSITOR_THREAD, ui::LatencyInfo(),
      INPUT_EVENT_ACK_STATE_CONSUMED, base::nullopt, cc::kTouchActionNone);
  ReleaseTouchPoint(0);
  SendTouchEvent();

  // Sequence 2
  PressTouchPoint(1, 1);
  SendTouchEvent();

  // First tap.
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);

  // The GestureTapUnconfirmed is converted into a tap, as the touch action is
  // none.
  SimulateGestureEvent(WebInputEvent::kGestureTapUnconfirmed,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(4U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_TRUE(dispatched_messages[1]->ToEvent());
  ASSERT_TRUE(dispatched_messages[2]->ToEvent());
  ASSERT_TRUE(dispatched_messages[3]->ToEvent());
  // This test will become invalid if GestureTap stops requiring an ack.
  ASSERT_TRUE(
      ShouldBlockEventStream(GetEventWithType(WebInputEvent::kGestureTap)));
  EXPECT_EQ(3, client_->in_flight_event_count());

  dispatched_messages[3]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2, client_->in_flight_event_count());

  // This tap gesture is dropped, since the GestureTapUnconfirmed was turned
  // into a tap.
  SimulateGestureEvent(WebInputEvent::kGestureTap,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  dispatched_messages[1]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // Second Tap.
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());

  // Although the touch-action is now auto, the double tap still won't be
  // dispatched, because the first tap occured when the touch-action was none.
  SimulateGestureEvent(WebInputEvent::kGestureDoubleTap,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());
  // This test will become invalid if GestureDoubleTap stops requiring an ack.
  ASSERT_TRUE(ShouldBlockEventStream(
      GetEventWithType(WebInputEvent::kGestureDoubleTap)));
  ASSERT_EQ(1, client_->in_flight_event_count());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0, client_->in_flight_event_count());
}

class TouchpadPinchInputRouterImplTest
    : public InputRouterImplTest,
      public testing::WithParamInterface<bool> {
 public:
  TouchpadPinchInputRouterImplTest() : async_events_enabled_(GetParam()) {
    if (async_events_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kTouchpadAsyncPinchEvents);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kTouchpadAsyncPinchEvents);
    }
  }
  ~TouchpadPinchInputRouterImplTest() = default;

  const bool async_events_enabled_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(TouchpadPinchInputRouterImplTest);
};

INSTANTIATE_TEST_CASE_P(, TouchpadPinchInputRouterImplTest, ::testing::Bool());

// Test that GesturePinchUpdate is handled specially for trackpad
TEST_P(TouchpadPinchInputRouterImplTest, TouchpadPinchUpdate) {
  // GesturePinchUpdate for trackpad sends synthetic wheel events.
  // Note that the Touchscreen case is verified as NOT doing this as
  // part of the ShowPressIsInOrder test.

  SimulateGestureEvent(WebInputEvent::kGesturePinchBegin,
                       blink::kWebGestureDeviceTouchpad);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  ASSERT_EQ(WebInputEvent::kGesturePinchBegin,
            disposition_handler_->ack_event_type());

  SimulateGesturePinchUpdateEvent(1.5f, 20, 25, 0,
                                  blink::kWebGestureDeviceTouchpad);

  // Verify we actually sent a special wheel event to the renderer.
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  const WebInputEvent* input_event =
      dispatched_messages[0]->ToEvent()->Event()->web_event.get();
  ASSERT_EQ(WebInputEvent::kMouseWheel, input_event->GetType());
  const WebMouseWheelEvent* synthetic_wheel =
      static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(20, synthetic_wheel->PositionInWidget().x);
  EXPECT_EQ(25, synthetic_wheel->PositionInWidget().y);
  EXPECT_EQ(20, synthetic_wheel->PositionInScreen().x);
  EXPECT_EQ(25, synthetic_wheel->PositionInScreen().y);
  EXPECT_TRUE(synthetic_wheel->GetModifiers() &
              blink::WebInputEvent::kControlKey);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseBegan, synthetic_wheel->phase);
  EXPECT_EQ(blink::WebInputEvent::kBlocking, synthetic_wheel->dispatch_type);

  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Check that the correct unhandled pinch event was received.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  ASSERT_EQ(WebInputEvent::kGesturePinchUpdate,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
            disposition_handler_->ack_state());
  EXPECT_EQ(
      1.5f,
      disposition_handler_->acked_gesture_event().data.pinch_update.scale);
  EXPECT_EQ(0, client_->in_flight_event_count());

  // Second a second pinch event.
  SimulateGesturePinchUpdateEvent(0.3f, 20, 25, 0,
                                  blink::kWebGestureDeviceTouchpad);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  input_event = dispatched_messages[0]->ToEvent()->Event()->web_event.get();
  ASSERT_EQ(WebInputEvent::kMouseWheel, input_event->GetType());
  synthetic_wheel = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseChanged, synthetic_wheel->phase);
  if (async_events_enabled_) {
    EXPECT_EQ(blink::WebInputEvent::kEventNonBlocking,
              synthetic_wheel->dispatch_type);
  } else {
    EXPECT_EQ(blink::WebInputEvent::kBlocking, synthetic_wheel->dispatch_type);
  }

  if (async_events_enabled_) {
    dispatched_messages[0]->ToEvent()->CallCallback(
        INPUT_EVENT_ACK_STATE_IGNORED);
  } else {
    dispatched_messages[0]->ToEvent()->CallCallback(
        INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  }

  // Check that the correct HANDLED pinch event was received.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            disposition_handler_->ack_event_type());
  if (async_events_enabled_) {
    EXPECT_EQ(INPUT_EVENT_ACK_STATE_IGNORED, disposition_handler_->ack_state());
  } else {
    EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
              disposition_handler_->ack_state());
  }
  EXPECT_FLOAT_EQ(
      0.3f,
      disposition_handler_->acked_gesture_event().data.pinch_update.scale);

  SimulateGestureEvent(WebInputEvent::kGesturePinchEnd,
                       blink::kWebGestureDeviceTouchpad);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  input_event = dispatched_messages[0]->ToEvent()->Event()->web_event.get();
  ASSERT_EQ(WebInputEvent::kMouseWheel, input_event->GetType());
  synthetic_wheel = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseEnded, synthetic_wheel->phase);
  EXPECT_EQ(blink::WebInputEvent::kEventNonBlocking,
            synthetic_wheel->dispatch_type);
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_IGNORED);

  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kGesturePinchEnd,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_IGNORED, disposition_handler_->ack_state());

  // The first event is blocked. We should send following wheel events as
  // blocking events.
  SimulateGestureEvent(WebInputEvent::kGesturePinchBegin,
                       blink::kWebGestureDeviceTouchpad);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  ASSERT_EQ(WebInputEvent::kGesturePinchBegin,
            disposition_handler_->ack_event_type());

  SimulateGesturePinchUpdateEvent(1.5f, 20, 25, 0,
                                  blink::kWebGestureDeviceTouchpad);

  // Verify we actually sent a special wheel event to the renderer.
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  input_event = dispatched_messages[0]->ToEvent()->Event()->web_event.get();
  ASSERT_EQ(WebInputEvent::kMouseWheel, input_event->GetType());
  synthetic_wheel = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_TRUE(synthetic_wheel->GetModifiers() &
              blink::WebInputEvent::kControlKey);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseBegan, synthetic_wheel->phase);
  EXPECT_EQ(blink::WebInputEvent::kBlocking, synthetic_wheel->dispatch_type);

  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the correct handled pinch event was received.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  ASSERT_EQ(WebInputEvent::kGesturePinchUpdate,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, disposition_handler_->ack_state());
  EXPECT_EQ(
      1.5f,
      disposition_handler_->acked_gesture_event().data.pinch_update.scale);
  EXPECT_EQ(0, client_->in_flight_event_count());

  // Second a second pinch event.
  SimulateGesturePinchUpdateEvent(0.3f, 20, 25, 0,
                                  blink::kWebGestureDeviceTouchpad);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  input_event = dispatched_messages[0]->ToEvent()->Event()->web_event.get();
  ASSERT_EQ(WebInputEvent::kMouseWheel, input_event->GetType());
  synthetic_wheel = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseChanged, synthetic_wheel->phase);
  EXPECT_EQ(blink::WebInputEvent::kBlocking, synthetic_wheel->dispatch_type);

  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the correct HANDLED pinch event was received.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, disposition_handler_->ack_state());
  EXPECT_FLOAT_EQ(
      0.3f,
      disposition_handler_->acked_gesture_event().data.pinch_update.scale);
}

// Test proper handling of touchpad Gesture{Pinch,Scroll}Update sequences.
TEST_F(InputRouterImplTest, TouchpadPinchAndScrollUpdate) {
  // All gesture events should be sent immediately.
  SimulateGestureScrollUpdateEvent(1.5f, 0.f, 0,
                                   blink::kWebGestureDeviceTouchpad);
  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchpad);
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(2U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_TRUE(dispatched_messages[1]->ToEvent());
  EXPECT_EQ(2, client_->in_flight_event_count());

  // Subsequent scroll and pinch events will also be sent immediately.
  SimulateTouchpadGesturePinchEventWithoutWheel(
      WebInputEvent::kGesturePinchUpdate, 1.5f, 20, 25, 0);
  DispatchedMessages temp_dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, temp_dispatched_messages.size());
  ASSERT_TRUE(temp_dispatched_messages[0]->ToEvent());
  dispatched_messages.emplace_back(std::move(temp_dispatched_messages.at(0)));
  EXPECT_EQ(3, client_->in_flight_event_count());

  SimulateGestureScrollUpdateEvent(1.5f, 1.5f, 0,
                                   blink::kWebGestureDeviceTouchpad);
  temp_dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, temp_dispatched_messages.size());
  ASSERT_TRUE(temp_dispatched_messages[0]->ToEvent());
  dispatched_messages.emplace_back(std::move(temp_dispatched_messages.at(0)));
  EXPECT_EQ(4, client_->in_flight_event_count());

  SimulateTouchpadGesturePinchEventWithoutWheel(
      WebInputEvent::kGesturePinchUpdate, 1.5f, 20, 25, 0);
  temp_dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, temp_dispatched_messages.size());
  ASSERT_TRUE(temp_dispatched_messages[0]->ToEvent());
  dispatched_messages.emplace_back(std::move(temp_dispatched_messages.at(0)));
  EXPECT_EQ(5, client_->in_flight_event_count());

  SimulateGestureScrollUpdateEvent(0.f, 1.5f, 0,
                                   blink::kWebGestureDeviceTouchpad);
  temp_dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, temp_dispatched_messages.size());
  ASSERT_TRUE(temp_dispatched_messages[0]->ToEvent());
  dispatched_messages.emplace_back(std::move(temp_dispatched_messages.at(0)));
  EXPECT_EQ(6, client_->in_flight_event_count());

  // Ack'ing events should decrease in-flight event count.
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(5, client_->in_flight_event_count());

  // Ack the second scroll.
  dispatched_messages[1]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(4, client_->in_flight_event_count());

  // Ack the pinch event.
  dispatched_messages[2]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(3, client_->in_flight_event_count());

  // Ack the scroll event.
  dispatched_messages[3]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2, client_->in_flight_event_count());

  // Ack the pinch event.
  dispatched_messages[4]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Ack the scroll event.
  dispatched_messages[5]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test proper routing of overscroll notifications received either from
// event acks or from |DidOverscroll| IPC messages.
TEST_F(InputRouterImplTest, OverscrollDispatch) {
  DidOverscrollParams overscroll;
  overscroll.accumulated_overscroll = gfx::Vector2dF(-14, 14);
  overscroll.latest_overscroll_delta = gfx::Vector2dF(-7, 0);
  overscroll.current_fling_velocity = gfx::Vector2dF(-1, 0);

  input_router_->DidOverscroll(overscroll);
  DidOverscrollParams client_overscroll = client_->GetAndResetOverscroll();
  EXPECT_EQ(overscroll.accumulated_overscroll,
            client_overscroll.accumulated_overscroll);
  EXPECT_EQ(overscroll.latest_overscroll_delta,
            client_overscroll.latest_overscroll_delta);
  // With browser side fling, the fling velocity doesn't come from overscroll
  // params of the renderer, instead the input router sets the
  // params.current_fling_velocity based on the velocity received from the fling
  // controller.
  EXPECT_EQ(gfx::Vector2dF(), client_overscroll.current_fling_velocity);

  DidOverscrollParams wheel_overscroll;
  wheel_overscroll.accumulated_overscroll = gfx::Vector2dF(7, -7);
  wheel_overscroll.latest_overscroll_delta = gfx::Vector2dF(3, 0);
  wheel_overscroll.current_fling_velocity = gfx::Vector2dF(1, 0);

  SimulateWheelEvent(0, 0, 3, 0, 0, false, WebMouseWheelEvent::kPhaseBegan);
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());

  dispatched_messages[0]->ToEvent()->CallCallback(
      InputEventAckSource::COMPOSITOR_THREAD, ui::LatencyInfo(),
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED, DidOverscrollParams(wheel_overscroll),
      base::nullopt);

  client_overscroll = client_->GetAndResetOverscroll();
  EXPECT_EQ(wheel_overscroll.accumulated_overscroll,
            client_overscroll.accumulated_overscroll);
  EXPECT_EQ(wheel_overscroll.latest_overscroll_delta,
            client_overscroll.latest_overscroll_delta);
  // With browser side fling, the fling velocity doesn't come from overscroll
  // params of the renderer, instead the input router sets the
  // params.current_fling_velocity based on the velocity received from the fling
  // controller.
  EXPECT_EQ(gfx::Vector2dF(), client_overscroll.current_fling_velocity);
}

// Test proper routing of whitelisted touch action notifications received from
// |SetWhiteListedTouchAction| IPC messages.
TEST_F(InputRouterImplTest, OnSetWhiteListedTouchAction) {
  cc::TouchAction touch_action = cc::kTouchActionPanY;
  OnSetWhiteListedTouchAction(touch_action, 0,
                              INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  cc::TouchAction white_listed_touch_action =
      client_->GetAndResetWhiteListedTouchAction();
  EXPECT_EQ(touch_action, white_listed_touch_action);
}

// Tests that touch event stream validation passes when events are filtered
// out. See crbug.com/581231 for details.
TEST_F(InputRouterImplTest, TouchValidationPassesWithFilteredInputEvents) {
  // Touch sequence with touch handler.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // This event will be filtered out, since no consumer exists.
  ReleaseTouchPoint(1);
  SendTouchEvent();
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, dispatched_messages.size());

  // If the validator didn't see the filtered out release event, it will crash
  // now, upon seeing a press for a touch which it believes to be still pressed.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
}

TEST_F(InputRouterImplTest, TouchActionInCallback) {
  OnHasTouchEventHandlers(true);

  // Send a touchstart
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      InputEventAckSource::COMPOSITOR_THREAD, ui::LatencyInfo(),
      INPUT_EVENT_ACK_STATE_CONSUMED, base::nullopt, cc::kTouchActionNone);
  ASSERT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  base::Optional<cc::TouchAction> allowed_touch_action =
      input_router_->AllowedTouchAction();
  DCHECK(allowed_touch_action.has_value());
  EXPECT_EQ(cc::TouchAction::kTouchActionNone, allowed_touch_action.value());
}

namespace {

class InputRouterImplScaleEventTest : public InputRouterImplTest {
 public:
  InputRouterImplScaleEventTest() {}

  void SetUp() override {
    InputRouterImplTest::SetUp();
    input_router_->SetDeviceScaleFactor(2.f);
  }

  template <typename T>
  const T* GetSentWebInputEvent() {
    EXPECT_EQ(1u, dispatched_messages_.size());

    return static_cast<const T*>(
        dispatched_messages_[0]->ToEvent()->Event()->web_event.get());
  }

  template <typename T>
  const T* GetFilterWebInputEvent() const {
    return static_cast<const T*>(client_->last_filter_event());
  }

  void UpdateDispatchedMessages() {
    dispatched_messages_ = GetAndResetDispatchedMessages();
  }

 protected:
  DispatchedMessages dispatched_messages_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputRouterImplScaleEventTest);
};

class InputRouterImplScaleMouseEventTest
    : public InputRouterImplScaleEventTest {
 public:
  InputRouterImplScaleMouseEventTest() {}

  void RunMouseEventTest(const std::string& name, WebInputEvent::Type type) {
    SCOPED_TRACE(name);
    SimulateMouseEvent(type, 10, 10);
    UpdateDispatchedMessages();
    const WebMouseEvent* sent_event = GetSentWebInputEvent<WebMouseEvent>();
    EXPECT_EQ(20, sent_event->PositionInWidget().x);
    EXPECT_EQ(20, sent_event->PositionInWidget().y);

    const WebMouseEvent* filter_event = GetFilterWebInputEvent<WebMouseEvent>();
    EXPECT_EQ(10, filter_event->PositionInWidget().x);
    EXPECT_EQ(10, filter_event->PositionInWidget().y);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputRouterImplScaleMouseEventTest);
};

}  // namespace

TEST_F(InputRouterImplScaleMouseEventTest, ScaleMouseEventTest) {
  RunMouseEventTest("Enter", WebInputEvent::kMouseEnter);
  RunMouseEventTest("Down", WebInputEvent::kMouseDown);
  RunMouseEventTest("Move", WebInputEvent::kMouseMove);
  RunMouseEventTest("Up", WebInputEvent::kMouseUp);
}

TEST_F(InputRouterImplScaleEventTest, ScaleMouseWheelEventTest) {
  SimulateWheelEvent(5, 5, 10, 10, 0, false, WebMouseWheelEvent::kPhaseBegan);
  UpdateDispatchedMessages();

  const WebMouseWheelEvent* sent_event =
      GetSentWebInputEvent<WebMouseWheelEvent>();
  EXPECT_EQ(10, sent_event->PositionInWidget().x);
  EXPECT_EQ(10, sent_event->PositionInWidget().y);
  EXPECT_EQ(20, sent_event->delta_x);
  EXPECT_EQ(20, sent_event->delta_y);
  EXPECT_EQ(2, sent_event->wheel_ticks_x);
  EXPECT_EQ(2, sent_event->wheel_ticks_y);

  const WebMouseWheelEvent* filter_event =
      GetFilterWebInputEvent<WebMouseWheelEvent>();
  EXPECT_EQ(5, filter_event->PositionInWidget().x);
  EXPECT_EQ(5, filter_event->PositionInWidget().y);
  EXPECT_EQ(10, filter_event->delta_x);
  EXPECT_EQ(10, filter_event->delta_y);
  EXPECT_EQ(1, filter_event->wheel_ticks_x);
  EXPECT_EQ(1, filter_event->wheel_ticks_y);

  EXPECT_EQ(sent_event->acceleration_ratio_x,
            filter_event->acceleration_ratio_x);
  EXPECT_EQ(sent_event->acceleration_ratio_y,
            filter_event->acceleration_ratio_y);
}

namespace {

class InputRouterImplScaleTouchEventTest
    : public InputRouterImplScaleEventTest {
 public:
  InputRouterImplScaleTouchEventTest() {}

  // Test tests if two finger touch event at (10, 20) and (100, 200) are
  // properly scaled. The touch event must be generated ans flushed into
  // the message sink prior to this method.
  void RunTouchEventTest(const std::string& name, WebTouchPoint::State state) {
    SCOPED_TRACE(name);
    const WebTouchEvent* sent_event = GetSentWebInputEvent<WebTouchEvent>();
    ASSERT_EQ(2u, sent_event->touches_length);
    EXPECT_EQ(state, sent_event->touches[0].state);
    EXPECT_EQ(20, sent_event->touches[0].PositionInWidget().x);
    EXPECT_EQ(40, sent_event->touches[0].PositionInWidget().y);
    EXPECT_EQ(10, sent_event->touches[0].PositionInScreen().x);
    EXPECT_EQ(20, sent_event->touches[0].PositionInScreen().y);
    EXPECT_EQ(40, sent_event->touches[0].radius_x);
    EXPECT_EQ(40, sent_event->touches[0].radius_y);

    EXPECT_EQ(200, sent_event->touches[1].PositionInWidget().x);
    EXPECT_EQ(400, sent_event->touches[1].PositionInWidget().y);
    EXPECT_EQ(100, sent_event->touches[1].PositionInScreen().x);
    EXPECT_EQ(200, sent_event->touches[1].PositionInScreen().y);
    EXPECT_EQ(40, sent_event->touches[1].radius_x);
    EXPECT_EQ(40, sent_event->touches[1].radius_y);

    const WebTouchEvent* filter_event = GetFilterWebInputEvent<WebTouchEvent>();
    ASSERT_EQ(2u, filter_event->touches_length);
    EXPECT_EQ(10, filter_event->touches[0].PositionInWidget().x);
    EXPECT_EQ(20, filter_event->touches[0].PositionInWidget().y);
    EXPECT_EQ(10, filter_event->touches[0].PositionInScreen().x);
    EXPECT_EQ(20, filter_event->touches[0].PositionInScreen().y);
    EXPECT_EQ(20, filter_event->touches[0].radius_x);
    EXPECT_EQ(20, filter_event->touches[0].radius_y);

    EXPECT_EQ(100, filter_event->touches[1].PositionInWidget().x);
    EXPECT_EQ(200, filter_event->touches[1].PositionInWidget().y);
    EXPECT_EQ(100, filter_event->touches[1].PositionInScreen().x);
    EXPECT_EQ(200, filter_event->touches[1].PositionInScreen().y);
    EXPECT_EQ(20, filter_event->touches[1].radius_x);
    EXPECT_EQ(20, filter_event->touches[1].radius_y);
  }

  void FlushTouchEvent(WebInputEvent::Type type) {
    SendTouchEvent();
    UpdateDispatchedMessages();
    ASSERT_EQ(1u, dispatched_messages_.size());
    ASSERT_TRUE(dispatched_messages_[0]->ToEvent());
    dispatched_messages_[0]->ToEvent()->CallCallback(
        INPUT_EVENT_ACK_STATE_CONSUMED);
    ASSERT_TRUE(TouchEventQueueEmpty());
  }

  void ReleaseTouchPointAndAck(int index) {
    ReleaseTouchPoint(index);
    SendTouchEvent();
    UpdateDispatchedMessages();
    ASSERT_EQ(1u, dispatched_messages_.size());
    ASSERT_TRUE(dispatched_messages_[0]->ToEvent());
    dispatched_messages_[0]->ToEvent()->CallCallback(
        INPUT_EVENT_ACK_STATE_CONSUMED);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputRouterImplScaleTouchEventTest);
};

}  // namespace

TEST_F(InputRouterImplScaleTouchEventTest, ScaleTouchEventTest) {
  ResetTouchAction();
  // Press
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::kTouchStart);

  RunTouchEventTest("Press", WebTouchPoint::kStatePressed);
  ReleaseTouchPointAndAck(1);
  ReleaseTouchPointAndAck(0);

  // Move
  PressTouchPoint(0, 0);
  PressTouchPoint(0, 0);
  FlushTouchEvent(WebInputEvent::kTouchStart);

  MoveTouchPoint(0, 10, 20);
  MoveTouchPoint(1, 100, 200);
  FlushTouchEvent(WebInputEvent::kTouchMove);
  RunTouchEventTest("Move", WebTouchPoint::kStateMoved);
  ReleaseTouchPointAndAck(1);
  ReleaseTouchPointAndAck(0);

  // Release
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::kTouchMove);

  ReleaseTouchPoint(0);
  ReleaseTouchPoint(1);
  FlushTouchEvent(WebInputEvent::kTouchEnd);
  RunTouchEventTest("Release", WebTouchPoint::kStateReleased);

  // Cancel
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::kTouchStart);

  CancelTouchPoint(0);
  CancelTouchPoint(1);
  FlushTouchEvent(WebInputEvent::kTouchCancel);
  RunTouchEventTest("Cancel", WebTouchPoint::kStateCancelled);
}

namespace {

class InputRouterImplScaleGestureEventTest
    : public InputRouterImplScaleEventTest {
 public:
  InputRouterImplScaleGestureEventTest() {}

  base::Optional<gfx::SizeF> GetContactSize(const WebGestureEvent& event) {
    switch (event.GetType()) {
      case WebInputEvent::Type::kGestureTapDown:
        return gfx::SizeF(event.data.tap_down.width,
                          event.data.tap_down.height);
      case WebInputEvent::Type::kGestureShowPress:
        return gfx::SizeF(event.data.show_press.width,
                          event.data.show_press.height);
      case WebInputEvent::Type::kGestureTap:
      case WebInputEvent::Type::kGestureTapUnconfirmed:
      case WebInputEvent::Type::kGestureDoubleTap:
        return gfx::SizeF(event.data.tap.width, event.data.tap.height);
      case WebInputEvent::Type::kGestureLongPress:
      case WebInputEvent::Type::kGestureLongTap:
        return gfx::SizeF(event.data.long_press.width,
                          event.data.long_press.height);
      case WebInputEvent::Type::kGestureTwoFingerTap:
        return gfx::SizeF(event.data.two_finger_tap.first_finger_width,
                          event.data.two_finger_tap.first_finger_height);
      default:
        return base::nullopt;
    }
  }

  void SetContactSize(WebGestureEvent& event, const gfx::SizeF& size) {
    switch (event.GetType()) {
      case WebInputEvent::Type::kGestureTapDown:
        event.data.tap_down.width = size.width();
        event.data.tap_down.height = size.height();
        break;
      case WebInputEvent::Type::kGestureShowPress:
        event.data.show_press.width = size.width();
        event.data.show_press.height = size.height();
        break;
      case WebInputEvent::Type::kGestureTap:
      case WebInputEvent::Type::kGestureTapUnconfirmed:
      case WebInputEvent::Type::kGestureDoubleTap:
        event.data.tap.width = size.width();
        event.data.tap.height = size.height();
        break;
      case WebInputEvent::Type::kGestureLongPress:
      case WebInputEvent::Type::kGestureLongTap:
        event.data.long_press.width = size.width();
        event.data.long_press.height = size.height();
        break;
      case WebInputEvent::Type::kGestureTwoFingerTap:
        event.data.two_finger_tap.first_finger_width = size.width();
        event.data.two_finger_tap.first_finger_height = size.height();
        break;
      default:
        break;
    }
  }

  WebGestureEvent BuildGestureEvent(WebInputEvent::Type type,
                                    const gfx::PointF& point,
                                    const gfx::SizeF& contact_size) {
    WebGestureEvent event = SyntheticWebGestureEventBuilder::Build(
        type, blink::kWebGestureDeviceTouchscreen);
    event.SetPositionInWidget(point);
    event.SetPositionInScreen(point);
    SetContactSize(event, contact_size);
    return event;
  }

  void SendGestureSequence(
      const std::vector<WebInputEvent::Type>& gesture_types) {
    const gfx::PointF orig(10, 20), scaled(20, 40);
    const gfx::SizeF contact_size(30, 40), contact_size_scaled(60, 80);

    for (WebInputEvent::Type type : gesture_types) {
      SCOPED_TRACE(WebInputEvent::GetName(type));

      WebGestureEvent event = BuildGestureEvent(type, orig, contact_size);
      SimulateGestureEvent(event);
      FlushGestureEvents({type});

      const WebGestureEvent* sent_event =
          GetSentWebInputEvent<WebGestureEvent>();
      TestLocationInSentEvent(sent_event, orig, scaled, contact_size_scaled);

      const WebGestureEvent* filter_event =
          GetFilterWebInputEvent<WebGestureEvent>();
      TestLocationInFilterEvent(filter_event, orig, contact_size);
    }
  }

  void FlushGestureEvents(
      const std::vector<WebInputEvent::Type>& expected_types) {
    UpdateDispatchedMessages();
    ASSERT_EQ(expected_types.size(), dispatched_messages_.size());
    for (size_t i = 0; i < dispatched_messages_.size(); i++) {
      ASSERT_TRUE(dispatched_messages_[i]->ToEvent());
      ASSERT_EQ(
          expected_types[i],
          dispatched_messages_[i]->ToEvent()->Event()->web_event->GetType());
      dispatched_messages_[i]->ToEvent()->CallCallback(
          INPUT_EVENT_ACK_STATE_CONSUMED);
    }
  }

  void TestLocationInSentEvent(
      const WebGestureEvent* sent_event,
      const gfx::PointF& orig,
      const gfx::PointF& scaled,
      const base::Optional<gfx::SizeF>& contact_size_scaled) {
    EXPECT_FLOAT_EQ(scaled.x(), sent_event->PositionInWidget().x);
    EXPECT_FLOAT_EQ(scaled.y(), sent_event->PositionInWidget().y);
    EXPECT_FLOAT_EQ(orig.x(), sent_event->PositionInScreen().x);
    EXPECT_FLOAT_EQ(orig.y(), sent_event->PositionInScreen().y);

    base::Optional<gfx::SizeF> event_contact_size = GetContactSize(*sent_event);
    if (event_contact_size && contact_size_scaled) {
      EXPECT_FLOAT_EQ(contact_size_scaled->width(),
                      event_contact_size->width());
      EXPECT_FLOAT_EQ(contact_size_scaled->height(),
                      event_contact_size->height());
    }
  }

  void TestLocationInFilterEvent(
      const WebGestureEvent* filter_event,
      const gfx::PointF& orig,
      const base::Optional<gfx::SizeF>& contact_size) {
    EXPECT_FLOAT_EQ(orig.x(), filter_event->PositionInWidget().x);
    EXPECT_FLOAT_EQ(orig.y(), filter_event->PositionInWidget().y);
    EXPECT_FLOAT_EQ(orig.x(), filter_event->PositionInScreen().x);
    EXPECT_FLOAT_EQ(orig.y(), filter_event->PositionInScreen().y);

    base::Optional<gfx::SizeF> event_contact_size =
        GetContactSize(*filter_event);
    if (event_contact_size && contact_size) {
      EXPECT_FLOAT_EQ(contact_size->width(), event_contact_size->width());
      EXPECT_FLOAT_EQ(contact_size->height(), event_contact_size->height());
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputRouterImplScaleGestureEventTest);
};

}  // namespace

TEST_F(InputRouterImplScaleGestureEventTest, GestureScroll) {
  const gfx::Vector2dF delta(10.f, 20.f), delta_scaled(20.f, 40.f);

  PressAndSetTouchActionAuto();

  SendGestureSequence(
      {WebInputEvent::kGestureTapDown, WebInputEvent::kGestureTapCancel});

  {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollBegin(
        delta.x(), delta.y(), blink::kWebGestureDeviceTouchscreen));
    FlushGestureEvents({WebInputEvent::kGestureScrollBegin});

    const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
    EXPECT_FLOAT_EQ(delta_scaled.x(),
                    sent_event->data.scroll_begin.delta_x_hint);
    EXPECT_FLOAT_EQ(delta_scaled.y(),
                    sent_event->data.scroll_begin.delta_y_hint);

    const WebGestureEvent* filter_event =
        GetFilterWebInputEvent<WebGestureEvent>();
    EXPECT_FLOAT_EQ(delta.x(), filter_event->data.scroll_begin.delta_x_hint);
    EXPECT_FLOAT_EQ(delta.y(), filter_event->data.scroll_begin.delta_y_hint);
  }

  {
    SimulateGestureScrollUpdateEvent(delta.x(), delta.y(), 0,
                                     blink::kWebGestureDeviceTouchscreen);
    FlushGestureEvents({WebInputEvent::kTouchScrollStarted,
                        WebInputEvent::kGestureScrollUpdate});
    // Erase TouchScrollStarted so we can inspect the GestureScrollUpdate.
    dispatched_messages_.erase(dispatched_messages_.begin());

    const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
    EXPECT_FLOAT_EQ(delta_scaled.x(), sent_event->data.scroll_update.delta_x);
    EXPECT_FLOAT_EQ(delta_scaled.y(), sent_event->data.scroll_update.delta_y);

    const WebGestureEvent* filter_event =
        GetFilterWebInputEvent<WebGestureEvent>();
    EXPECT_FLOAT_EQ(delta.x(), filter_event->data.scroll_update.delta_x);
    EXPECT_FLOAT_EQ(delta.y(), filter_event->data.scroll_update.delta_y);
  }

  SendGestureSequence({WebInputEvent::kGestureScrollEnd});
}

TEST_F(InputRouterImplScaleGestureEventTest, GesturePinch) {
  const gfx::PointF anchor(10.f, 20.f), anchor_scaled(20.f, 40.f);
  const float scale_change(1.5f);

  PressAndSetTouchActionAuto();

  SendGestureSequence(
      {WebInputEvent::kGestureTapDown, WebInputEvent::kGestureTapCancel});

  SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollBegin(
      0.f, 0.f, blink::kWebGestureDeviceTouchscreen));
  FlushGestureEvents({WebInputEvent::kGestureScrollBegin});

  SendGestureSequence({WebInputEvent::kGesturePinchBegin});

  SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildPinchUpdate(
      scale_change, anchor.x(), anchor.y(), 0,
      blink::kWebGestureDeviceTouchscreen));

  FlushGestureEvents({WebInputEvent::kGesturePinchUpdate});
  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  TestLocationInSentEvent(sent_event, anchor, anchor_scaled, base::nullopt);
  EXPECT_FLOAT_EQ(scale_change, sent_event->data.pinch_update.scale);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  TestLocationInFilterEvent(filter_event, anchor, base::nullopt);
  EXPECT_FLOAT_EQ(scale_change, filter_event->data.pinch_update.scale);

  SendGestureSequence(
      {WebInputEvent::kGesturePinchEnd, WebInputEvent::kGestureScrollEnd});
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureTap) {
  SendGestureSequence({WebInputEvent::kGestureTapDown,
                       WebInputEvent::kGestureShowPress,
                       WebInputEvent::kGestureTap});
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureDoubleTap) {
  SendGestureSequence(
      {WebInputEvent::kGestureTapDown, WebInputEvent::kGestureTapUnconfirmed,
       WebInputEvent::kGestureTapCancel, WebInputEvent::kGestureTapDown,
       WebInputEvent::kGestureTapCancel, WebInputEvent::kGestureDoubleTap});
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureLongPress) {
  SendGestureSequence(
      {WebInputEvent::kGestureTapDown, WebInputEvent::kGestureShowPress,
       WebInputEvent::kGestureLongPress, WebInputEvent::kGestureTapCancel,
       WebInputEvent::kGestureLongTap});
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureTwoFingerTap) {
  SendGestureSequence({WebInputEvent::kGestureTapDown,
                       WebInputEvent::kGestureTapCancel,
                       WebInputEvent::kGestureTwoFingerTap});
}

}  // namespace content
