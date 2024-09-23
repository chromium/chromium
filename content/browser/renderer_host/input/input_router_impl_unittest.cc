// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/input_router_impl.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "cc/input/touch_action.h"
#include "components/input/gesture_event_queue.h"
#include "components/input/switches.h"
#include "content/browser/renderer_host/input/mock_input_disposition_handler.h"
#include "content/browser/renderer_host/input/mock_input_router_client.h"
#include "content/browser/renderer_host/input/mock_render_widget_host_view_for_stylus_writing.h"
#include "content/browser/renderer_host/mock_render_widget_host.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/browser/site_instance_group.h"
#include "content/common/content_constants_internal.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/mock_widget_input_handler.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(USE_AURA)
#include "content/common/input/events_helper.h"
#include "ui/events/event.h"
#endif

using blink::SyntheticWebGestureEventBuilder;
using blink::SyntheticWebMouseEventBuilder;
using blink::SyntheticWebMouseWheelEventBuilder;
using blink::SyntheticWebTouchEvent;
using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
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
  } else if (type == WebInputEvent::Type::kMouseWheel) {
    static WebMouseWheelEvent wheel;
    event = &wheel;
  }
  CHECK(event);
  event->SetType(type);
  return *event;
}

}  // namespace


class InputRouterImplTestBase : public testing::Test {
 public:
  InputRouterImplTestBase()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  ~InputRouterImplTestBase() override {}

 protected:
  using DispatchedMessages = MockWidgetInputHandler::MessageVector;
  // testing::Test
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(input::switches::kValidateInputEventStream);
    client_ = std::make_unique<MockInputRouterClient>();
    disposition_handler_ = std::make_unique<MockInputDispositionHandler>();
    input_router_ = std::make_unique<input::InputRouterImpl>(
        client_.get(), disposition_handler_.get(), client_.get(), config_);

    client_->set_input_router(input_router());
    disposition_handler_->set_input_router(input_router());

    browser_context_ = std::make_unique<TestBrowserContext>();
    process_host_ =
        std::make_unique<MockRenderProcessHost>(browser_context_.get());
    site_instance_group_ =
        base::WrapRefCounted(SiteInstanceGroup::CreateForTesting(
            browser_context_.get(), process_host_.get()));
    widget_host_ = MakeNewWidgetHost();
    mock_view_ =
        new MockRenderWidgetHostViewForStylusWriting(widget_host_.get());
    client_->set_render_widget_host_view(mock_view_.get());
  }

  std::unique_ptr<RenderWidgetHostImpl> MakeNewWidgetHost() {
    int32_t routing_id = process_host_->GetNextRoutingID();
    return MockRenderWidgetHost::Create(
        /*frame_tree=*/nullptr, &delegate_, site_instance_group_->GetSafeRef(),
        routing_id);
  }

  void TearDown() override {
    // Process all pending tasks to avoid leaks.
    base::RunLoop().RunUntilIdle();

    input_router_.reset();
    client_.reset();
    if (mock_view_)
      delete mock_view_;
    widget_host_ = nullptr;
    process_host_->Cleanup();
    site_instance_group_.reset();
    process_host_ = nullptr;
  }

  void SetUpForTouchAckTimeoutTest(int desktop_timeout_ms,
                                   int mobile_timeout_ms) {
    config_.touch_config.desktop_touch_ack_timeout_delay =
        base::Milliseconds(desktop_timeout_ms);
    config_.touch_config.mobile_touch_ack_timeout_delay =
        base::Milliseconds(mobile_timeout_ms);
    config_.touch_config.touch_ack_timeout_supported = true;
    config_.touch_config.task_runner =
        base::SequencedTaskRunner::GetCurrentDefault();
    TearDown();
    SetUp();
    input_router()->NotifySiteIsMobileOptimized(false);
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type) {
    input::NativeWebKeyboardEventWithLatencyInfo key_event(
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
        x, y, dX, dY, modifiers,
        precise ? ui::ScrollGranularity::kScrollByPrecisePixel
                : ui::ScrollGranularity::kScrollByPixel);
    wheel_event.phase = phase;
    input_router_->SendWheelEvent(
        input::MouseWheelEventWithLatencyInfo(wheel_event));
  }

  void SimulateWheelEvent(WebMouseWheelEvent::Phase phase) {
    input_router_->SendWheelEvent(input::MouseWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(phase)));
  }

  void SimulateMouseEvent(WebInputEvent::Type type, int x, int y) {
    input_router_->SendMouseEvent(
        input::MouseEventWithLatencyInfo(
            SyntheticWebMouseEventBuilder::Build(type, x, y, 0)),
        disposition_handler_->CreateMouseEventCallback());
  }

  void SimulateGestureEvent(WebGestureEvent gesture) {
    if (gesture.GetType() == WebInputEvent::Type::kGestureScrollBegin &&
        gesture.SourceDevice() == blink::WebGestureDevice::kTouchscreen &&
        !gesture.data.scroll_begin.delta_x_hint &&
        !gesture.data.scroll_begin.delta_y_hint) {
      // Ensure non-zero scroll-begin offset-hint to make the event sane,
      // prevents unexpected filtering at TouchActionFilter.
      gesture.data.scroll_begin.delta_y_hint = 2.f;
    } else if (gesture.GetType() == WebInputEvent::Type::kGestureFlingStart &&
               gesture.SourceDevice() ==
                   blink::WebGestureDevice::kTouchscreen &&
               !gesture.data.fling_start.velocity_x &&
               !gesture.data.fling_start.velocity_y) {
      // Ensure non-zero touchscreen fling velocities, as the router will
      // validate against such. The velocity should be large enough to make
      // sure that the fling is still active while sending the GFC.
      gesture.data.fling_start.velocity_x = 500.f;
    } else if (gesture.GetType() == WebInputEvent::Type::kGestureFlingCancel) {
      // Set prevent boosting to make sure that the GFC cancels the active
      // fling.
      gesture.data.fling_cancel.prevent_boosting = true;
    }

    input_router_->SendGestureEvent(
        input::GestureEventWithLatencyInfo(gesture));
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
        (type == blink::WebInputEvent::Type::kGesturePinchUpdate
             ? SyntheticWebGestureEventBuilder::BuildPinchUpdate(
                   scale, anchor_x, anchor_y, modifiers,
                   blink::WebGestureDevice::kTouchpad)
             : SyntheticWebGestureEventBuilder::Build(
                   type, blink::WebGestureDevice::kTouchpad));
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
    input_router_->SendTouchEvent(
        input::TouchEventWithLatencyInfo(touch_event_));
    touch_event_.ResetPoints();
    return touch_event_id;
  }

  int PressTouchPoint(int x, int y) {
    return touch_event_.PressPoint(x, y, radius_x_, radius_y_);
  }

  void MoveTouchPoint(int index, int x, int y) {
    touch_event_.MovePoint(index, x, y, radius_x_, radius_y_);
  }

  void ReleaseTouchPoint(int index) { touch_event_.ReleasePoint(index); }

  void CancelTouchPoint(int index) { touch_event_.CancelPoint(index); }

  input::InputRouterImpl* input_router() const { return input_router_.get(); }

  bool TouchEventQueueEmpty() const {
    return input_router()->touch_event_queue_.Empty();
  }

  bool TouchEventTimeoutEnabled() const {
    return input_router()->touch_event_queue_.IsAckTimeoutEnabled();
  }

  bool HasPendingEvents() const { return input_router_->HasPendingEvents(); }

  bool HasTouchEventHandlers(bool has_handlers) { return has_handlers; }
  bool HasHitTestableScrollbar(bool has_scrollbar) { return has_scrollbar; }

  void OnHasTouchEventConsumers(
      blink::mojom::TouchEventConsumersPtr consumers) {
    input_router_->OnHasTouchEventConsumers(std::move(consumers));
  }

  void CancelTouchTimeout() {
    // InputRouterImpl::SetTouchActionFromMain calls
    // InputRouterImpl::UpdateTouchAckTimeoutEnabled and that will cancel the
    // touch timeout when the touch action is None.
    input_router_->SetTouchActionFromMain(cc::TouchAction::kNone);
  }

  void ResetTouchAction() {
    input_router_->touch_action_filter_.ResetTouchAction();
  }

  DispatchedMessages GetAndResetDispatchedMessages() {
    return client_->GetAndResetDispatchedMessages();
  }

  static void RunTasksAndWait(base::TimeDelta delay) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delay);
    run_loop.Run();
  }

  void PressAndSetTouchActionAuto() {
    PressTouchPoint(1, 1);
    SendTouchEvent();
    input_router_->SetTouchActionFromMain(cc::TouchAction::kAuto);
    GetAndResetDispatchedMessages();
    disposition_handler_->GetAndResetAckCount();
  }

  void PressAndSetTouchActionWritable() {
    PressTouchPoint(1, 1);
    SendTouchEvent();
    input_router_->SetTouchActionFromMain(
        cc::TouchAction::kAuto & ~cc::TouchAction::kInternalNotWritable);
    GetAndResetDispatchedMessages();
    disposition_handler_->GetAndResetAckCount();
  }

  void TouchActionSetFromMainNotOverridden() {
    input_router_->SetTouchActionFromMain(cc::TouchAction::kAuto);
    ASSERT_TRUE(input_router_->AllowedTouchAction().has_value());
    EXPECT_EQ(input_router_->AllowedTouchAction().value(),
              cc::TouchAction::kAuto);
    input_router_->TouchEventHandled(
        input::TouchEventWithLatencyInfo(touch_event_),
        blink::mojom::InputEventResultSource::kMainThread, ui::LatencyInfo(),
        blink::mojom::InputEventResultState::kNoConsumerExists, nullptr,
        blink::mojom::TouchActionOptional::New(cc::TouchAction::kPanY));
    EXPECT_EQ(input_router_->AllowedTouchAction().value(),
              cc::TouchAction::kAuto);
  }

  void ActiveTouchSequenceCountTest(
      blink::mojom::TouchActionOptionalPtr touch_action,
      blink::mojom::InputEventResultState state) {
    PressTouchPoint(1, 1);
    input_router_->SendTouchEvent(
        input::TouchEventWithLatencyInfo(touch_event_));
    input_router_->TouchEventHandled(
        input::TouchEventWithLatencyInfo(touch_event_),
        blink::mojom::InputEventResultSource::kMainThread, ui::LatencyInfo(),
        state, nullptr, std::move(touch_action));
    EXPECT_EQ(input_router_->touch_action_filter_.num_of_active_touches_, 1);
    ReleaseTouchPoint(0);
    input_router_->OnTouchEventAck(
        input::TouchEventWithLatencyInfo(touch_event_),
        blink::mojom::InputEventResultSource::kMainThread, state);
    EXPECT_EQ(input_router_->touch_action_filter_.num_of_active_touches_, 0);
  }

  void StopTimeoutMonitorTest() {
    ResetTouchAction();
    PressTouchPoint(1, 1);
    input_router_->SendTouchEvent(
        input::TouchEventWithLatencyInfo(touch_event_));
    EXPECT_TRUE(input_router_->touch_event_queue_.IsTimeoutRunningForTesting());
    input_router_->TouchEventHandled(
        input::TouchEventWithLatencyInfo(touch_event_),
        blink::mojom::InputEventResultSource::kCompositorThread,
        ui::LatencyInfo(), blink::mojom::InputEventResultState::kNotConsumed,
        nullptr, blink::mojom::TouchActionOptional::New(cc::TouchAction::kPan));
    EXPECT_TRUE(input_router_->touch_event_queue_.IsTimeoutRunningForTesting());
    input_router_->SetTouchActionFromMain(cc::TouchAction::kPan);
    EXPECT_FALSE(
        input_router_->touch_event_queue_.IsTimeoutRunningForTesting());
  }

  void OnTouchEventAckWithAckState(
      blink::mojom::InputEventResultSource source,
      blink::mojom::InputEventResultState ack_state,
      std::optional<cc::TouchAction> expected_touch_action,
      std::optional<cc::TouchAction> expected_allowed_touch_action) {
    auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
        HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
    input_router_->OnHasTouchEventConsumers(std::move(touch_event_consumers));
    EXPECT_FALSE(input_router_->AllowedTouchAction().has_value());
    PressTouchPoint(1, 1);
    input_router_->SendTouchEvent(
        input::TouchEventWithLatencyInfo(touch_event_));
    input_router_->OnTouchEventAck(
        input::TouchEventWithLatencyInfo(touch_event_), source, ack_state);
    EXPECT_EQ(input_router_->AllowedTouchAction(), expected_touch_action);
    EXPECT_EQ(
        input_router_->touch_action_filter_.compositor_allowed_touch_action(),
        expected_allowed_touch_action.value());
  }

  const float radius_x_ = 20.0f;
  const float radius_y_ = 20.0f;
  input::InputRouter::Config config_;
  std::unique_ptr<MockInputRouterClient> client_;
  std::unique_ptr<input::InputRouterImpl> input_router_;
  std::unique_ptr<MockInputDispositionHandler> disposition_handler_;
  raw_ptr<MockRenderWidgetHostViewForStylusWriting, DanglingUntriaged>
      mock_view_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  SyntheticWebTouchEvent touch_event_;
  std::unique_ptr<BrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> process_host_;
  scoped_refptr<SiteInstanceGroup> site_instance_group_;
  std::unique_ptr<RenderWidgetHostImpl> widget_host_;
  MockRenderWidgetHostDelegate delegate_;
};

class InputRouterImplTest : public InputRouterImplTestBase {
 public:
  InputRouterImplTest() = default;

  std::optional<cc::TouchAction> AllowedTouchAction() {
    return input_router_->touch_action_filter_.allowed_touch_action_;
  }

  cc::TouchAction CompositorAllowedTouchAction() {
    return input_router_->touch_action_filter_.compositor_allowed_touch_action_;
  }
};

TEST_F(InputRouterImplTest, HandledInputEvent) {
  client_->set_filter_state(blink::mojom::InputEventResultState::kConsumed);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  // Make sure no input event is sent to the renderer.
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_messages.size());

  // OnKeyboardEventAck should be triggered without actual ack.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}

TEST_F(InputRouterImplTest, ClientCanceledKeyboardEvent) {
  client_->set_filter_state(
      blink::mojom::InputEventResultState::kNoConsumerExists);

  // Simulate a keyboard event that has no consumer.
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  // Make sure no input event is sent to the renderer.
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_messages.size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // Simulate a keyboard event that should be dropped.
  client_->set_filter_state(blink::mojom::InputEventResultState::kUnknown);
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  // Make sure no input event is sent to the renderer, and no ack is sent.
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_messages.size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
}

// Tests ported from RenderWidgetHostTest --------------------------------------

TEST_F(InputRouterImplTest, HandleKeyEventsWeSent) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  // Make sure we sent the input event to the renderer.
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
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
  ASSERT_EQ(WebInputEvent::Type::kMouseWheel,
            dispatched_messages[0]->ToEvent()->Event()->Event().GetType());
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          &dispatched_messages[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(0, wheel_event->delta_x);
  EXPECT_EQ(-5, wheel_event->delta_y);

  // Check that the ACK sends the second message immediately.
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  // The coalesced events can queue up a delayed ack
  // so that additional input events can be processed before
  // we turn off coalescing.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_EQ(WebInputEvent::Type::kMouseWheel,
            dispatched_messages[0]->ToEvent()->Event()->Event().GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &dispatched_messages[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(8, wheel_event->delta_x);
  EXPECT_EQ(-10 + -6, wheel_event->delta_y);  // coalesced

  // Ack the second event (which had the third coalesced into it).
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_EQ(WebInputEvent::Type::kMouseWheel,
            dispatched_messages[0]->ToEvent()->Event()->Event().GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &dispatched_messages[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(9, wheel_event->delta_x);
  EXPECT_EQ(-7, wheel_event->delta_y);

  // Ack the fourth event.
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_EQ(WebInputEvent::Type::kMouseWheel,
            dispatched_messages[0]->ToEvent()->Event()->Event().GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &dispatched_messages[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(0, wheel_event->delta_x);
  EXPECT_EQ(-10, wheel_event->delta_y);

  // Ack the fifth event.
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_EQ(WebInputEvent::Type::kMouseWheel,
            dispatched_messages[0]->ToEvent()->Event()->Event().GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &dispatched_messages[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(0, wheel_event->delta_x);
  EXPECT_EQ(0, wheel_event->delta_y);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);

  // After the final ack, the queue should be empty.
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_messages.size());
}

// Test that the active touch sequence count increment when the touch start is
// not ACKed from the main thread.
TEST_F(InputRouterImplTest, ActiveTouchSequenceCountWithoutTouchAction) {
  ActiveTouchSequenceCountTest(
      nullptr, blink::mojom::InputEventResultState::kSetNonBlocking);
}

TEST_F(InputRouterImplTest,
       ActiveTouchSequenceCountWithoutTouchActionNoConsumer) {
  ActiveTouchSequenceCountTest(
      nullptr, blink::mojom::InputEventResultState::kNoConsumerExists);
}

// Test that the active touch sequence count increment when the touch start is
// ACKed from the main thread.
TEST_F(InputRouterImplTest, ActiveTouchSequenceCountWithTouchAction) {
  ActiveTouchSequenceCountTest(
      blink::mojom::TouchActionOptional::New(cc::TouchAction::kPanY),
      blink::mojom::InputEventResultState::kSetNonBlocking);
}

TEST_F(InputRouterImplTest, ActiveTouchSequenceCountWithTouchActionNoConsumer) {
  ActiveTouchSequenceCountTest(
      blink::mojom::TouchActionOptional::New(cc::TouchAction::kPanY),
      blink::mojom::InputEventResultState::kNoConsumerExists);
}

// Test that after touch action is set from the main thread, the touch action
// won't be overridden by the call to TouchEventHandled.
TEST_F(InputRouterImplTest, TouchActionSetFromMainNotOverridden) {
  TouchActionSetFromMainNotOverridden();
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateConsumed) {
  std::optional<cc::TouchAction> expected_touch_action;
  OnTouchEventAckWithAckState(
      blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kConsumed, expected_touch_action,
      cc::TouchAction::kAuto);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateNotConsumed) {
  std::optional<cc::TouchAction> expected_touch_action;
  OnTouchEventAckWithAckState(
      blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNotConsumed, expected_touch_action,
      cc::TouchAction::kAuto);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateConsumedShouldBubble) {
  std::optional<cc::TouchAction> expected_touch_action;
  OnTouchEventAckWithAckState(
      blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNotConsumed, expected_touch_action,
      cc::TouchAction::kAuto);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateNoConsumerExists) {
  std::optional<cc::TouchAction> expected_touch_action;
  OnTouchEventAckWithAckState(
      blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists,
      expected_touch_action, cc::TouchAction::kAuto);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateIgnored) {
  std::optional<cc::TouchAction> expected_touch_action;
  OnTouchEventAckWithAckState(
      blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kIgnored, expected_touch_action,
      cc::TouchAction::kAuto);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateNonBlocking) {
  std::optional<cc::TouchAction> expected_touch_action;
  OnTouchEventAckWithAckState(
      blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kSetNonBlocking,
      expected_touch_action, cc::TouchAction::kAuto);
}

TEST_F(InputRouterImplTest, TouchActionAutoWithAckStateNonBlockingDueToFling) {
  std::optional<cc::TouchAction> expected_touch_action;
  OnTouchEventAckWithAckState(
      blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kSetNonBlockingDueToFling,
      expected_touch_action, cc::TouchAction::kAuto);
}

// Tests that touch-events are sent properly.
TEST_F(InputRouterImplTest, TouchEventQueue) {
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));

  PressTouchPoint(1, 1);
  SendTouchEvent();
  input_router_->SetTouchActionFromMain(cc::TouchAction::kAuto);
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
  touch_start_event[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kTouchStart,
            disposition_handler_->acked_touch_event().event.GetType());
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  touch_move_event[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kTouchMove,
            disposition_handler_->acked_touch_event().event.GetType());
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
}

// Tests that the touch-queue is emptied after a page stops listening for touch
// events and the outstanding ack is received.
TEST_F(InputRouterImplTest, TouchEventQueueFlush) {
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));
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
  touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(false), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // After the ack, the touch-event queue should be empty, and none of the
  // flushed touch-events should have been sent to the renderer.
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
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
      blink::mojom::InputEventResultState::kNotConsumed);

  // There should be a ScrollBegin, ScrollUpdate, second MouseWheel, and second
  // ScrollUpdate sent.
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(4U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_TRUE(dispatched_messages[1]->ToEvent());
  ASSERT_TRUE(dispatched_messages[2]->ToEvent());
  ASSERT_TRUE(dispatched_messages[3]->ToEvent());
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollBegin,
            dispatched_messages[0]->ToEvent()->Event()->Event().GetType());
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            dispatched_messages[1]->ToEvent()->Event()->Event().GetType());
  ASSERT_EQ(WebInputEvent::Type::kMouseWheel,
            dispatched_messages[2]->ToEvent()->Event()->Event().GetType());
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            dispatched_messages[3]->ToEvent()->Event()->Event().GetType());

  // Indicate that the GestureScrollBegin event was consumed.
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Check that the ack for the first MouseWheel, ScrollBegin, and the second
  // MouseWheel were processed.
  EXPECT_EQ(3U, disposition_handler_->GetAndResetAckCount());

  // The last acked wheel event should be the second one since the input router
  // has already sent the immediate ack for the second wheel event.
  EXPECT_EQ(disposition_handler_->acked_wheel_event().delta_y, -10);
  EXPECT_EQ(blink::mojom::InputEventResultState::kIgnored,
            disposition_handler_->acked_wheel_event_state());

  // Ack the first gesture scroll update.
  dispatched_messages[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Check that the ack for the first ScrollUpdate were processed.
  EXPECT_EQ(
      -5,
      disposition_handler_->acked_gesture_event().data.scroll_update.delta_y);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // Ack the second gesture scroll update.
  dispatched_messages[3]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Check that the ack for the second ScrollUpdate were processed.
  EXPECT_EQ(
      -10,
      disposition_handler_->acked_gesture_event().data.scroll_update.delta_y);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}

TEST_F(InputRouterImplTest, TouchTypesIgnoringAck) {
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));
  // Only acks for TouchCancel should always be ignored.
  ASSERT_TRUE(ShouldBlockEventStream(
      GetEventWithType(WebInputEvent::Type::kTouchStart)));
  ASSERT_TRUE(ShouldBlockEventStream(
      GetEventWithType(WebInputEvent::Type::kTouchMove)));
  ASSERT_TRUE(
      ShouldBlockEventStream(GetEventWithType(WebInputEvent::Type::kTouchEnd)));

  // Precede the TouchCancel with an appropriate TouchStart;
  PressTouchPoint(1, 1);
  SendTouchEvent();
  input_router_->SetTouchActionFromMain(cc::TouchAction::kAuto);
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
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

// Flaky on Linux: https://crbug.com/1295039
// Flaky on at least Win7 and Win10 as well: https://crbug.com/1326564
TEST_F(InputRouterImplTest, DISABLED_GestureTypesIgnoringAck) {
  // We test every gesture type, ensuring that the stream of gestures is valid.

  const auto eventTypes = std::to_array<WebInputEvent::Type>(
      {WebInputEvent::Type::kGestureTapDown,
       WebInputEvent::Type::kGestureShowPress,
       WebInputEvent::Type::kGestureTapCancel,
       WebInputEvent::Type::kGestureScrollBegin,
       WebInputEvent::Type::kGestureFlingStart,
       WebInputEvent::Type::kGestureFlingCancel,
       WebInputEvent::Type::kGestureTapDown,
       WebInputEvent::Type::kGestureTap,
       WebInputEvent::Type::kGestureTapDown,
       WebInputEvent::Type::kGestureLongPress,
       WebInputEvent::Type::kGestureTapCancel,
       WebInputEvent::Type::kGestureLongTap,
       WebInputEvent::Type::kGestureTapDown,
       WebInputEvent::Type::kGestureTapUnconfirmed,
       WebInputEvent::Type::kGestureTapCancel,
       WebInputEvent::Type::kGestureTapDown,
       WebInputEvent::Type::kGestureDoubleTap,
       WebInputEvent::Type::kGestureTapDown,
       WebInputEvent::Type::kGestureTapCancel,
       WebInputEvent::Type::kGestureTwoFingerTap,
       WebInputEvent::Type::kGestureTapDown,
       WebInputEvent::Type::kGestureTapCancel,
       WebInputEvent::Type::kGestureScrollBegin,
       WebInputEvent::Type::kGestureScrollUpdate,
       WebInputEvent::Type::kGesturePinchBegin,
       WebInputEvent::Type::kGesturePinchUpdate,
       WebInputEvent::Type::kGesturePinchEnd,
       WebInputEvent::Type::kGestureScrollEnd});
  for (WebInputEvent::Type type : eventTypes) {
    if (type == WebInputEvent::Type::kGestureFlingStart ||
        type == WebInputEvent::Type::kGestureFlingCancel) {
      SimulateGestureEvent(type, blink::WebGestureDevice::kTouchscreen);
      DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();

      if (type == WebInputEvent::Type::kGestureFlingCancel) {
        // The fling controller generates and sends a GSE while handling the
        // GFC.
        EXPECT_EQ(1U, dispatched_messages.size());
        EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
      } else {
        EXPECT_EQ(0U, dispatched_messages.size());
        EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
      }

      EXPECT_EQ(0, client_->in_flight_event_count());
      EXPECT_FALSE(HasPendingEvents());
    } else if (ShouldBlockEventStream(GetEventWithType(type))) {
      PressAndSetTouchActionAuto();
      SimulateGestureEvent(type, blink::WebGestureDevice::kTouchscreen);
      DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();

      if (type == WebInputEvent::Type::kGestureScrollUpdate) {
        // TouchScrollStarted is also dispatched.
        EXPECT_EQ(2U, dispatched_messages.size());
      } else {
        EXPECT_EQ(1U, dispatched_messages.size());
      }
      EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(1, client_->in_flight_event_count());
      EXPECT_TRUE(HasPendingEvents());
      ASSERT_TRUE(
          dispatched_messages[dispatched_messages.size() - 1]->ToEvent());

      dispatched_messages[dispatched_messages.size() - 1]
          ->ToEvent()
          ->CallCallback(blink::mojom::InputEventResultState::kNotConsumed);

      EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
      EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(0, client_->in_flight_event_count());
      EXPECT_FALSE(HasPendingEvents());
    } else {
      SimulateGestureEvent(type, blink::WebGestureDevice::kTouchscreen);
      EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
      EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(0, client_->in_flight_event_count());
      EXPECT_FALSE(HasPendingEvents());
    }
  }
}

TEST_F(InputRouterImplTest, MouseTypesIgnoringAck) {
  int start_type = static_cast<int>(WebInputEvent::Type::kMouseDown);
  int end_type = static_cast<int>(WebInputEvent::Type::kContextMenu);
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
          blink::mojom::InputEventResultState::kNotConsumed);
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
  const auto kRequiredEventAckTypes = std::to_array<WebInputEvent::Type>(
      {WebInputEvent::Type::kMouseMove, WebInputEvent::Type::kMouseWheel,
       WebInputEvent::Type::kRawKeyDown, WebInputEvent::Type::kKeyDown,
       WebInputEvent::Type::kKeyUp, WebInputEvent::Type::kChar,
       WebInputEvent::Type::kGestureScrollBegin,
       WebInputEvent::Type::kGestureScrollUpdate,
       WebInputEvent::Type::kTouchStart, WebInputEvent::Type::kTouchMove});
  for (WebInputEvent::Type required_ack_type : kRequiredEventAckTypes) {
    ASSERT_TRUE(ShouldBlockEventStream(GetEventWithType(required_ack_type)))
        << WebInputEvent::GetName(required_ack_type);
  }
}

TEST_F(InputRouterImplTest, GestureTypesIgnoringAckInterleaved) {
  // Interleave a few events that do and do not ignore acks. All gesture events
  // should be dispatched immediately, but the acks will be blocked on blocking
  // events.
  PressAndSetTouchActionAuto();
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  // Should have sent |kTouchScrollStarted| and |kGestureScrollUpdate|.
  EXPECT_EQ(2U, dispatched_messages.size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            client_->last_in_flight_event_type());

  SimulateGestureEvent(WebInputEvent::Type::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::Type::kGestureTapDown,
            client_->last_in_flight_event_type());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  DispatchedMessages temp_dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, temp_dispatched_messages.size());
  dispatched_messages.emplace_back(std::move(temp_dispatched_messages.at(0)));
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::Type::kGestureShowPress,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::Type::kGestureShowPress,
            client_->last_in_flight_event_type());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  temp_dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, temp_dispatched_messages.size());
  dispatched_messages.emplace_back(std::move(temp_dispatched_messages.at(0)));
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(3, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            client_->last_in_flight_event_type());

  SimulateGestureEvent(WebInputEvent::Type::kGestureTapCancel,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(3, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::Type::kGestureTapCancel,
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
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2, client_->in_flight_event_count());

  // Ack the second GestureScrollUpdate
  dispatched_messages[2]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Ack the last GestureScrollUpdate
  dispatched_messages[3]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());

  ReleaseTouchPoint(0);
  SendTouchEvent();
}

// Test that GestureShowPress events don't get out of order due to
// ignoring their acks.
TEST_F(InputRouterImplTest, GestureShowPressIsInOrder) {
  PressAndSetTouchActionAuto();
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // GesturePinchBegin ignores its ack.
  SimulateGestureEvent(WebInputEvent::Type::kGesturePinchBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // GesturePinchUpdate ignores its ack.
  // This also verifies that GesturePinchUpdates for touchscreen are sent
  // to the renderer (in contrast to the TrackpadPinchUpdate test).
  SimulateGestureEvent(WebInputEvent::Type::kGesturePinchUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchUpdate,
            client_->last_in_flight_event_type());

  // GestureScrollUpdate waits for an ack.
  // This dispatches TouchScrollStarted and GestureScrollUpdate.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(2U, dispatched_messages.size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            client_->last_in_flight_event_type());

  // GestureShowPress will be sent immediately since GestureEventQueue allows
  // multiple in-flight events. However the acks will be blocked on outstanding
  // in-flight events.
  SimulateGestureEvent(WebInputEvent::Type::kGestureShowPress,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::Type::kGestureShowPress,
            client_->last_in_flight_event_type());

  SimulateGestureEvent(WebInputEvent::Type::kGestureShowPress,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());
  EXPECT_EQ(WebInputEvent::Type::kGestureShowPress,
            client_->last_in_flight_event_type());

  // Ack the GestureScrollUpdate to release the two GestureShowPress acks.
  dispatched_messages[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
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
  RunTasksAndWait(base::Milliseconds(kDesktopTimeoutMs + 1));

  // The timed-out event should have been ack'ed.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  // Ack'ing the timed-out event should fire a TouchCancel.
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());

  // The remainder of the touch sequence should be forwarded.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  PressAndSetTouchActionAuto();
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  ASSERT_TRUE(TouchEventTimeoutEnabled());

  // A mobile-optimized site should use the mobile timeout. For this test that
  // timeout value is 0, which disables the timeout.
  input_router()->NotifySiteIsMobileOptimized(true);
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  input_router()->NotifySiteIsMobileOptimized(false);
  EXPECT_TRUE(TouchEventTimeoutEnabled());

  // TouchAction::kNone (and no other touch-action) should disable the timeout.
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));
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
      blink::mojom::InputEventResultState::kConsumed);
  touch_release_event2[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  input_router_->SetTouchActionFromMain(cc::TouchAction::kNone);
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
      blink::mojom::InputEventResultState::kConsumed);
  touch_release_event3[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // As the touch-action is reset by a new touch sequence, the timeout behavior
  // should be restored.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  ResetTouchAction();
  input_router_->SetTouchActionFromMain(cc::TouchAction::kAuto);
  EXPECT_TRUE(TouchEventTimeoutEnabled());
}

// Test that a touch sequenced preceded by TouchAction::kNone is not affected by
// the touch timeout.
TEST_F(InputRouterImplTest,
       TouchAckTimeoutDisabledForTouchSequenceAfterTouchActionNone) {
  const int kDesktopTimeoutMs = 1;
  const int kMobileTimeoutMs = 2;
  SetUpForTouchAckTimeoutTest(kDesktopTimeoutMs, kMobileTimeoutMs);
  ASSERT_TRUE(TouchEventTimeoutEnabled());
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));

  // Start a touch sequence.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());

  // TouchAction::kNone should disable the timeout.
  CancelTouchTimeout();
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultSource::kMainThread, ui::LatencyInfo(),
      blink::mojom::InputEventResultState::kConsumed, nullptr,
      blink::mojom::TouchActionOptional::New(cc::TouchAction::kNone));
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  MoveTouchPoint(0, 1, 2);
  SendTouchEvent();
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_FALSE(TouchEventTimeoutEnabled());
  EXPECT_EQ(1U, dispatched_messages.size());

  // Delay the move ack. The timeout should not fire.
  RunTasksAndWait(base::Milliseconds(kDesktopTimeoutMs + 1));
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // End the touch sequence.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  input_router_->SetTouchActionFromMain(cc::TouchAction::kAuto);
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
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
  RunTasksAndWait(base::Milliseconds(kDesktopTimeoutMs + 1));
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}

// Test that TouchActionFilter::ResetTouchAction is called before the
// first touch event for a touch sequence reaches the renderer.
TEST_F(InputRouterImplTest, TouchActionResetBeforeEventReachesRenderer) {
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));

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
      blink::mojom::InputEventResultSource::kMainThread, ui::LatencyInfo(),
      blink::mojom::InputEventResultState::kConsumed, nullptr,
      blink::mojom::TouchActionOptional::New(cc::TouchAction::kNone));
  touch_move_event1[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Ensure touch action is still none, as the next touch start hasn't been
  // acked yet. ScrollBegin and ScrollEnd don't require acks.
  SimulateGestureEvent(WebInputEvent::Type::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  // This allows the next touch sequence to start.
  touch_release_event1[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Ensure touch action has been set to auto, as a new touch sequence has
  // started.
  touch_press_event2[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultSource::kCompositorThread,
      ui::LatencyInfo(), blink::mojom::InputEventResultState::kConsumed,
      nullptr, blink::mojom::TouchActionOptional::New(cc::TouchAction::kAuto));
  touch_press_event2[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  touch_move_event2[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  DispatchedMessages gesture_scroll_begin = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, gesture_scroll_begin.size());
  gesture_scroll_begin[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  touch_release_event2[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
}

// Test that TouchActionFilter::ResetTouchAction is called when a new touch
// sequence has no consumer.
TEST_F(InputRouterImplTest, TouchActionResetWhenTouchHasNoConsumer) {
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));

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
      blink::mojom::InputEventResultSource::kMainThread, ui::LatencyInfo(),
      blink::mojom::InputEventResultState::kConsumed, nullptr,
      blink::mojom::TouchActionOptional::New(cc::TouchAction::kNone));
  touch_move_event1[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

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
  SimulateGestureEvent(WebInputEvent::Type::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  touch_release_event1[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  touch_press_event2[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNoConsumerExists);

  PressAndSetTouchActionAuto();
  // Ensure touch action has been set to auto, as the touch had no consumer.
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
}

// Test that TouchActionFilter::ResetTouchAction is called when the touch
// handler is removed.
TEST_F(InputRouterImplTest, TouchActionResetWhenTouchHandlerRemoved) {
  // Touch sequence with touch handler.
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));
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
      blink::mojom::InputEventResultSource::kMainThread, ui::LatencyInfo(),
      blink::mojom::InputEventResultState::kConsumed, nullptr,
      blink::mojom::TouchActionOptional::New(cc::TouchAction::kNone));
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  dispatched_messages[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::Type::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  dispatched_messages[2]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  // Sequence without a touch handler. Note that in this case, the view may not
  // necessarily forward touches to the router (as no touch handler exists).
  touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(false), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));

  // Ensure touch action has been set to auto, as the touch handler has been
  // removed.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
}

// Tests that async touch-moves are ack'd from the browser side.
TEST_F(InputRouterImplTest, AsyncTouchMoveAckedImmediately) {
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));

  PressTouchPoint(1, 1);
  SendTouchEvent();
  input_router_->SetTouchActionFromMain(cc::TouchAction::kAuto);
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // Receive an ACK for the first touch-event.
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kTouchStart,
            disposition_handler_->ack_event_type());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollBegin,
            disposition_handler_->ack_event_type());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(2U, dispatched_messages.size());
  EXPECT_EQ(WebInputEvent::Type::kTouchScrollStarted,
            dispatched_messages[0]->ToEvent()->Event()->Event().GetType());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            dispatched_messages[1]->ToEvent()->Event()->Event().GetType());
  // Ack the GestureScrollUpdate.
  dispatched_messages[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // Now since we're scrolling send an async move.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_EQ(WebInputEvent::Type::kTouchMove,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());

  // To catch crbug/1072364 send another scroll which returns kNoConsumerExists
  // and ensure we're still async scrolling since we've already started the
  // scroll.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            dispatched_messages[0]->ToEvent()->Event()->Event().GetType());
  // Ack the GestureScrollUpdate.
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // Now since we're scrolling (even with NoConsumerExists) send an async move.
  MoveTouchPoint(0, 10, 5);
  SendTouchEvent();
  EXPECT_EQ(WebInputEvent::Type::kTouchMove,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetAndResetDispatchedMessages().size());
}

// Test that the double tap gesture depends on the touch action of the first
// tap.
TEST_F(InputRouterImplTest, DoubleTapGestureDependsOnFirstTap) {
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));

  // Sequence 1.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  CancelTouchTimeout();
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultSource::kMainThread, ui::LatencyInfo(),
      blink::mojom::InputEventResultState::kConsumed, nullptr,
      blink::mojom::TouchActionOptional::New(cc::TouchAction::kNone));
  ReleaseTouchPoint(0);
  SendTouchEvent();

  // Sequence 2
  PressTouchPoint(1, 1);
  SendTouchEvent();

  // First tap.
  SimulateGestureEvent(WebInputEvent::Type::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);

  // The GestureTapUnconfirmed is converted into a tap, as the touch action is
  // none.
  SimulateGestureEvent(WebInputEvent::Type::kGestureTapUnconfirmed,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(4U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_TRUE(dispatched_messages[1]->ToEvent());
  ASSERT_TRUE(dispatched_messages[2]->ToEvent());
  ASSERT_TRUE(dispatched_messages[3]->ToEvent());
  // This test will become invalid if GestureTap stops requiring an ack.
  ASSERT_TRUE(ShouldBlockEventStream(
      GetEventWithType(WebInputEvent::Type::kGestureTap)));
  EXPECT_EQ(3, client_->in_flight_event_count());

  dispatched_messages[3]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(2, client_->in_flight_event_count());

  // This tap gesture is dropped, since the GestureTapUnconfirmed was turned
  // into a tap.
  SimulateGestureEvent(WebInputEvent::Type::kGestureTap,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());

  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  dispatched_messages[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNoConsumerExists);

  // Second Tap.
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  SimulateGestureEvent(WebInputEvent::Type::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());

  // Although the touch-action is now auto, the double tap still won't be
  // dispatched, because the first tap occurred when the touch-action was none.
  SimulateGestureEvent(WebInputEvent::Type::kGestureDoubleTap,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());
  // This test will become invalid if GestureDoubleTap stops requiring an ack.
  ASSERT_TRUE(ShouldBlockEventStream(
      GetEventWithType(WebInputEvent::Type::kGestureDoubleTap)));
  ASSERT_EQ(1, client_->in_flight_event_count());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test that GesturePinchUpdate is handled specially for trackpad
TEST_F(InputRouterImplTest, TouchpadPinchUpdate) {
  // GesturePinchUpdate for trackpad sends synthetic wheel events.
  // Note that the Touchscreen case is verified as NOT doing this as
  // part of the ShowPressIsInOrder test.

  SimulateGestureEvent(WebInputEvent::Type::kGesturePinchBegin,
                       blink::WebGestureDevice::kTouchpad);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  ASSERT_EQ(WebInputEvent::Type::kGesturePinchBegin,
            disposition_handler_->ack_event_type());

  SimulateGesturePinchUpdateEvent(1.5f, 20, 25, 0,
                                  blink::WebGestureDevice::kTouchpad);

  // Verify we actually sent a special wheel event to the renderer.
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  const WebInputEvent* input_event =
      &dispatched_messages[0]->ToEvent()->Event()->Event();
  ASSERT_EQ(WebInputEvent::Type::kMouseWheel, input_event->GetType());
  const WebMouseWheelEvent* synthetic_wheel =
      static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(20, synthetic_wheel->PositionInWidget().x());
  EXPECT_EQ(25, synthetic_wheel->PositionInWidget().y());
  EXPECT_EQ(20, synthetic_wheel->PositionInScreen().x());
  EXPECT_EQ(25, synthetic_wheel->PositionInScreen().y());
  EXPECT_TRUE(synthetic_wheel->GetModifiers() &
              blink::WebInputEvent::kControlKey);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseBegan, synthetic_wheel->phase);
  EXPECT_EQ(blink::WebInputEvent::DispatchType::kBlocking,
            synthetic_wheel->dispatch_type);

  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  // Check that the correct unhandled pinch event was received.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  ASSERT_EQ(WebInputEvent::Type::kGesturePinchUpdate,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(blink::mojom::InputEventResultState::kNotConsumed,
            disposition_handler_->ack_state());
  EXPECT_EQ(
      1.5f,
      disposition_handler_->acked_gesture_event().data.pinch_update.scale);
  EXPECT_EQ(0, client_->in_flight_event_count());

  // Second a second pinch event.
  SimulateGesturePinchUpdateEvent(0.3f, 20, 25, 0,
                                  blink::WebGestureDevice::kTouchpad);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  input_event = &dispatched_messages[0]->ToEvent()->Event()->Event();
  ASSERT_EQ(WebInputEvent::Type::kMouseWheel, input_event->GetType());
  synthetic_wheel = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseChanged, synthetic_wheel->phase);
  EXPECT_EQ(blink::WebInputEvent::DispatchType::kEventNonBlocking,
            synthetic_wheel->dispatch_type);
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kIgnored);

  // Check that the correct HANDLED pinch event was received.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchUpdate,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(blink::mojom::InputEventResultState::kIgnored,
            disposition_handler_->ack_state());
  EXPECT_FLOAT_EQ(
      0.3f,
      disposition_handler_->acked_gesture_event().data.pinch_update.scale);

  SimulateGestureEvent(WebInputEvent::Type::kGesturePinchEnd,
                       blink::WebGestureDevice::kTouchpad);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  input_event = &dispatched_messages[0]->ToEvent()->Event()->Event();
  ASSERT_EQ(WebInputEvent::Type::kMouseWheel, input_event->GetType());
  synthetic_wheel = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseEnded, synthetic_wheel->phase);
  EXPECT_EQ(blink::WebInputEvent::DispatchType::kEventNonBlocking,
            synthetic_wheel->dispatch_type);
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kIgnored);

  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchEnd,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(blink::mojom::InputEventResultState::kIgnored,
            disposition_handler_->ack_state());

  // The first event is blocked. We should send following wheel events as
  // blocking events.
  SimulateGestureEvent(WebInputEvent::Type::kGesturePinchBegin,
                       blink::WebGestureDevice::kTouchpad);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  ASSERT_EQ(WebInputEvent::Type::kGesturePinchBegin,
            disposition_handler_->ack_event_type());

  SimulateGesturePinchUpdateEvent(1.5f, 20, 25, 0,
                                  blink::WebGestureDevice::kTouchpad);

  // Verify we actually sent a special wheel event to the renderer.
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  input_event = &dispatched_messages[0]->ToEvent()->Event()->Event();
  ASSERT_EQ(WebInputEvent::Type::kMouseWheel, input_event->GetType());
  synthetic_wheel = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_TRUE(synthetic_wheel->GetModifiers() &
              blink::WebInputEvent::kControlKey);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseBegan, synthetic_wheel->phase);
  EXPECT_EQ(blink::WebInputEvent::DispatchType::kBlocking,
            synthetic_wheel->dispatch_type);

  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Check that the correct handled pinch event was received.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  ASSERT_EQ(WebInputEvent::Type::kGesturePinchUpdate,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(blink::mojom::InputEventResultState::kConsumed,
            disposition_handler_->ack_state());
  EXPECT_EQ(
      1.5f,
      disposition_handler_->acked_gesture_event().data.pinch_update.scale);
  EXPECT_EQ(0, client_->in_flight_event_count());

  // Second a second pinch event.
  SimulateGesturePinchUpdateEvent(0.3f, 20, 25, 0,
                                  blink::WebGestureDevice::kTouchpad);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  input_event = &dispatched_messages[0]->ToEvent()->Event()->Event();
  ASSERT_EQ(WebInputEvent::Type::kMouseWheel, input_event->GetType());
  synthetic_wheel = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseChanged, synthetic_wheel->phase);
  EXPECT_EQ(blink::WebInputEvent::DispatchType::kBlocking,
            synthetic_wheel->dispatch_type);

  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Check that the correct HANDLED pinch event was received.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchUpdate,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(blink::mojom::InputEventResultState::kConsumed,
            disposition_handler_->ack_state());
  EXPECT_FLOAT_EQ(
      0.3f,
      disposition_handler_->acked_gesture_event().data.pinch_update.scale);
}

// Test proper handling of touchpad Gesture{Pinch,Scroll}Update sequences.
TEST_F(InputRouterImplTest, TouchpadPinchAndScrollUpdate) {
  // All gesture events should be sent immediately.
  SimulateGestureScrollUpdateEvent(1.5f, 0.f, 0,
                                   blink::WebGestureDevice::kTouchpad);
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchpad);
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(2U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  ASSERT_TRUE(dispatched_messages[1]->ToEvent());
  EXPECT_EQ(2, client_->in_flight_event_count());

  // Subsequent scroll and pinch events will also be sent immediately.
  SimulateTouchpadGesturePinchEventWithoutWheel(
      WebInputEvent::Type::kGesturePinchUpdate, 1.5f, 20, 25, 0);
  DispatchedMessages temp_dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, temp_dispatched_messages.size());
  ASSERT_TRUE(temp_dispatched_messages[0]->ToEvent());
  dispatched_messages.emplace_back(std::move(temp_dispatched_messages.at(0)));
  EXPECT_EQ(2, client_->in_flight_event_count());

  SimulateGestureScrollUpdateEvent(1.5f, 1.5f, 0,
                                   blink::WebGestureDevice::kTouchpad);
  temp_dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, temp_dispatched_messages.size());
  ASSERT_TRUE(temp_dispatched_messages[0]->ToEvent());
  dispatched_messages.emplace_back(std::move(temp_dispatched_messages.at(0)));
  EXPECT_EQ(3, client_->in_flight_event_count());

  SimulateTouchpadGesturePinchEventWithoutWheel(
      WebInputEvent::Type::kGesturePinchUpdate, 1.5f, 20, 25, 0);
  temp_dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, temp_dispatched_messages.size());
  ASSERT_TRUE(temp_dispatched_messages[0]->ToEvent());
  dispatched_messages.emplace_back(std::move(temp_dispatched_messages.at(0)));
  EXPECT_EQ(3, client_->in_flight_event_count());

  SimulateGestureScrollUpdateEvent(0.f, 1.5f, 0,
                                   blink::WebGestureDevice::kTouchpad);
  temp_dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, temp_dispatched_messages.size());
  ASSERT_TRUE(temp_dispatched_messages[0]->ToEvent());
  dispatched_messages.emplace_back(std::move(temp_dispatched_messages.at(0)));
  EXPECT_EQ(4, client_->in_flight_event_count());

  // Ack'ing events should decrease in-flight event count.
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(3, client_->in_flight_event_count());

  // Ack the second scroll.
  dispatched_messages[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_FALSE(dispatched_messages[2]->ToEvent()->HasCallback());
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2, client_->in_flight_event_count());

  // Ack the scroll event.
  dispatched_messages[3]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_FALSE(dispatched_messages[4]->ToEvent()->HasCallback());
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Ack the scroll event.
  dispatched_messages[5]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test proper routing of overscroll notifications received either from
// event acks or from |DidOverscroll| IPC messages.
TEST_F(InputRouterImplTest, OverscrollDispatch) {
  blink::mojom::DidOverscrollParams overscroll;
  overscroll.accumulated_overscroll = gfx::Vector2dF(-14, 14);
  overscroll.latest_overscroll_delta = gfx::Vector2dF(-7, 0);
  overscroll.current_fling_velocity = gfx::Vector2dF(-1, 0);

  input_router_->DidOverscroll(overscroll.Clone());
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

  blink::mojom::DidOverscrollParams wheel_overscroll;
  wheel_overscroll.accumulated_overscroll = gfx::Vector2dF(7, -7);
  wheel_overscroll.latest_overscroll_delta = gfx::Vector2dF(3, 0);
  wheel_overscroll.current_fling_velocity = gfx::Vector2dF(1, 0);

  SimulateWheelEvent(0, 0, 3, 0, 0, false, WebMouseWheelEvent::kPhaseBegan);
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());

  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultSource::kCompositorThread,
      ui::LatencyInfo(), blink::mojom::InputEventResultState::kNotConsumed,
      wheel_overscroll.Clone(), nullptr);

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

// Tests that touch event stream validation passes when events are filtered
// out. See https://crbug.com/581231 for details.
TEST_F(InputRouterImplTest, TouchValidationPassesWithFilteredInputEvents) {
  // Touch sequence with touch handler.
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNoConsumerExists);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNoConsumerExists);

  // This event will not be filtered out even though no consumer exists.
  ReleaseTouchPoint(1);
  SendTouchEvent();
  dispatched_messages = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, dispatched_messages.size());

  // If the validator didn't see the filtered out release event, it will crash
  // now, upon seeing a press for a touch which it believes to be still pressed.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNoConsumerExists);
}

TEST_F(InputRouterImplTest, TouchActionInCallback) {
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));

  // Send a touchstart
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  std::optional<cc::TouchAction> expected_touch_action = cc::TouchAction::kPan;
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultSource::kCompositorThread,
      ui::LatencyInfo(), blink::mojom::InputEventResultState::kConsumed,
      nullptr, blink::mojom::TouchActionOptional::New(cc::TouchAction::kPan));
  ASSERT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  std::optional<cc::TouchAction> allowed_touch_action = AllowedTouchAction();
  cc::TouchAction compositor_allowed_touch_action =
      CompositorAllowedTouchAction();
  EXPECT_FALSE(allowed_touch_action.has_value());
  EXPECT_EQ(expected_touch_action.value(), compositor_allowed_touch_action);
}

// TODO(crbug.com/40623448): enable this when the bug is fixed.
TEST_F(InputRouterImplTest,
       DISABLED_TimeoutMonitorStopWithMainThreadTouchAction) {
  SetUpForTouchAckTimeoutTest(1, 1);
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));
  StopTimeoutMonitorTest();
}

namespace {

class InputRouterImplStylusWritingTest : public InputRouterImplTest {
 public:
  InputRouterImplStylusWritingTest() = default;
};

}  // namespace

// Tests that hover action stylus writable is set when pan action is received.
TEST_F(InputRouterImplStylusWritingTest, SetHoverActionStylusWritableToView) {
  // Hover action is not set before pan action is received.
  ASSERT_FALSE(mock_view_->hover_action_stylus_writable());

  // Hover action is stylus writable only when pan action is writable.
  input_router_->SetPanAction(blink::mojom::PanAction::kStylusWritable);
  ASSERT_TRUE(mock_view_->hover_action_stylus_writable());

  // Hover action is not stylus writable when pan action is cursor control.
  input_router_->SetPanAction(blink::mojom::PanAction::kMoveCursorOrScroll);
  ASSERT_FALSE(mock_view_->hover_action_stylus_writable());

  // Set hover action as stylus writable to assert it changes for kScroll.
  input_router_->SetPanAction(blink::mojom::PanAction::kStylusWritable);
  ASSERT_TRUE(mock_view_->hover_action_stylus_writable());
  // Hover action is not stylus writable when pan action is scroll.
  input_router_->SetPanAction(blink::mojom::PanAction::kScroll);
  ASSERT_FALSE(mock_view_->hover_action_stylus_writable());
}

// Tests that stylus writing is not started when touch action is not writable.
TEST_F(InputRouterImplStylusWritingTest,
       StylusWritingNotStartedForNotWritableTouchAction) {
  PressAndSetTouchActionAuto();

  // Set ShouldInitiateStylusWriting() to return true, to ensure scroll events
  // are not filtered when touch action is not writable.
  mock_view_->set_supports_stylus_writing(true);
  ASSERT_TRUE(client_->GetStylusInterface()->ShouldInitiateStylusWriting());
  SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollBegin(
      2.f, 2.f, blink::WebGestureDevice::kTouchscreen, /* pointer_count */ 1));
  // scroll begin is not filtered when kInternalNotWritable is set.
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  ASSERT_FALSE(client_->on_start_stylus_writing_called());
}

// Tests that stylus writing is not started when touch action is writable, but
// request to start stylus writing returned false.
TEST_F(InputRouterImplStylusWritingTest,
       StylusWritingNotStartedForTouchActionWritable) {
  PressAndSetTouchActionWritable();

  // ShouldInitiateStylusWriting() returns false by default.
  ASSERT_FALSE(client_->GetStylusInterface()->ShouldInitiateStylusWriting());
  SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollBegin(
      2.f, 2.f, blink::WebGestureDevice::kTouchscreen, /* pointer_count */ 1));
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollBegin,
            disposition_handler_->ack_event_type());
  ASSERT_FALSE(client_->on_start_stylus_writing_called());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  // This dispatches TouchScrollStarted and GestureScrollUpdate.
  ASSERT_EQ(2U, dispatched_messages.size());
  EXPECT_EQ(WebInputEvent::Type::kTouchScrollStarted,
            dispatched_messages[0]->ToEvent()->Event()->Event().GetType());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            dispatched_messages[1]->ToEvent()->Event()->Event().GetType());
  dispatched_messages[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            disposition_handler_->ack_event_type());
}

// Tests that stylus writing is not started when touch action is writable,
// request to start stylus writing returns true but pointer count is more
// than 1.
TEST_F(InputRouterImplStylusWritingTest, StylusWritingNotStartedForMultiTouch) {
  PressAndSetTouchActionWritable();

  // Set ShouldInitiateStylusWriting() to return true.
  mock_view_->set_supports_stylus_writing(true);
  ASSERT_TRUE(client_->GetStylusInterface()->ShouldInitiateStylusWriting());
  SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollBegin(
      2.f, 2.f, blink::WebGestureDevice::kTouchscreen, /* pointer_count */ 2));
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  // Scroll begin is not filtered when pointer count is 2.
  ASSERT_EQ(1U, dispatched_messages.size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  // Message not sent to client that stylus writing has been started.
  ASSERT_FALSE(client_->on_start_stylus_writing_called());
}

// Tests that stylus writing is started when touch action is writable, and
// request to start stylus writing returns true, and pointer count is 1.
TEST_F(InputRouterImplStylusWritingTest,
       StylusWritingStartedForTouchActionWritable) {
  PressAndSetTouchActionWritable();

  // Set ShouldInitiateStylusWriting() to return true.
  mock_view_->set_supports_stylus_writing(true);
  ASSERT_TRUE(client_->GetStylusInterface()->ShouldInitiateStylusWriting());
  // GestureScrollBegin is filtered.
  SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollBegin(
      2.f, 2.f, blink::WebGestureDevice::kTouchscreen, /* pointer_count */ 1));
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(0U, dispatched_messages.size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollBegin,
            disposition_handler_->ack_event_type());
  // Message sent to client that stylus writing has been started.
  ASSERT_TRUE(client_->on_start_stylus_writing_called());

  // GestureScrollUpdate and GestureScrollEnd are also filtered.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(0U, dispatched_messages.size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            disposition_handler_->ack_event_type());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(0U, dispatched_messages.size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            disposition_handler_->ack_event_type());
}

// Tests that GestureScrollBegin is filtered even if compositor touch action
// allows scroll.
TEST_F(InputRouterImplStylusWritingTest,
       StylusWritingFiltersGSBEvenWhenCompositorTouchActionAllows) {
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  OnHasTouchEventConsumers(std::move(touch_event_consumers));
  // Send a touchstart
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedMessages dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
  ASSERT_TRUE(dispatched_messages[0]->ToEvent());
  std::optional<cc::TouchAction> expected_touch_action = cc::TouchAction::kPan;
  dispatched_messages[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultSource::kCompositorThread,
      ui::LatencyInfo(), blink::mojom::InputEventResultState::kNotConsumed,
      nullptr, blink::mojom::TouchActionOptional::New(cc::TouchAction::kPan));
  ASSERT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  std::optional<cc::TouchAction> allowed_touch_action = AllowedTouchAction();
  cc::TouchAction compositor_allowed_touch_action =
      CompositorAllowedTouchAction();
  EXPECT_FALSE(allowed_touch_action.has_value());
  EXPECT_EQ(expected_touch_action.value(), compositor_allowed_touch_action);

  // Stylus GestureScrollBegin is filtered until we get touch action from Main.
  WebGestureEvent scroll_begin_event =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(
          2.f, 2.f, blink::WebGestureDevice::kTouchscreen,
          /* pointer_count */ 1);
  scroll_begin_event.primary_pointer_type =
      blink::WebPointerProperties::PointerType::kPen;
  SimulateGestureEvent(scroll_begin_event);
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(0U, dispatched_messages.size());

  input_router_->SetTouchActionFromMain(cc::TouchAction::kAuto);
  allowed_touch_action = AllowedTouchAction();
  EXPECT_TRUE(allowed_touch_action.has_value());
  dispatched_messages = GetAndResetDispatchedMessages();
  ASSERT_EQ(1U, dispatched_messages.size());
}

namespace {

class InputRouterImplScaleEventTest : public InputRouterImplTestBase {
 public:
  InputRouterImplScaleEventTest() {}

  InputRouterImplScaleEventTest(const InputRouterImplScaleEventTest&) = delete;
  InputRouterImplScaleEventTest& operator=(
      const InputRouterImplScaleEventTest&) = delete;

  void SetUp() override {
    InputRouterImplTestBase::SetUp();
    input_router_->SetDeviceScaleFactor(2.f);
  }

  template <typename T>
  const T* GetSentWebInputEvent() {
    EXPECT_EQ(1u, dispatched_messages_.size());

    return static_cast<const T*>(
        &dispatched_messages_[0]->ToEvent()->Event()->Event());
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
};

class InputRouterImplScaleMouseEventTest
    : public InputRouterImplScaleEventTest {
 public:
  InputRouterImplScaleMouseEventTest() {}

  InputRouterImplScaleMouseEventTest(
      const InputRouterImplScaleMouseEventTest&) = delete;
  InputRouterImplScaleMouseEventTest& operator=(
      const InputRouterImplScaleMouseEventTest&) = delete;

  void RunMouseEventTest(const std::string& name, WebInputEvent::Type type) {
    SCOPED_TRACE(name);
    SimulateMouseEvent(type, 10, 10);
    UpdateDispatchedMessages();
    const WebMouseEvent* sent_event = GetSentWebInputEvent<WebMouseEvent>();
    EXPECT_EQ(20, sent_event->PositionInWidget().x());
    EXPECT_EQ(20, sent_event->PositionInWidget().y());

    const WebMouseEvent* filter_event = GetFilterWebInputEvent<WebMouseEvent>();
    EXPECT_EQ(10, filter_event->PositionInWidget().x());
    EXPECT_EQ(10, filter_event->PositionInWidget().y());
  }
};

}  // namespace

TEST_F(InputRouterImplScaleMouseEventTest, ScaleMouseEventTest) {
  RunMouseEventTest("Enter", WebInputEvent::Type::kMouseEnter);
  RunMouseEventTest("Down", WebInputEvent::Type::kMouseDown);
  RunMouseEventTest("Move", WebInputEvent::Type::kMouseMove);
  RunMouseEventTest("Up", WebInputEvent::Type::kMouseUp);
}

TEST_F(InputRouterImplScaleEventTest, ScaleMouseWheelEventTest) {
  SimulateWheelEvent(5, 5, 10, 10, 0, false, WebMouseWheelEvent::kPhaseBegan);
  UpdateDispatchedMessages();

  const WebMouseWheelEvent* sent_event =
      GetSentWebInputEvent<WebMouseWheelEvent>();
  EXPECT_EQ(10, sent_event->PositionInWidget().x());
  EXPECT_EQ(10, sent_event->PositionInWidget().y());
  EXPECT_EQ(20, sent_event->delta_x);
  EXPECT_EQ(20, sent_event->delta_y);
  EXPECT_EQ(2, sent_event->wheel_ticks_x);
  EXPECT_EQ(2, sent_event->wheel_ticks_y);

  const WebMouseWheelEvent* filter_event =
      GetFilterWebInputEvent<WebMouseWheelEvent>();
  EXPECT_EQ(5, filter_event->PositionInWidget().x());
  EXPECT_EQ(5, filter_event->PositionInWidget().y());
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

  InputRouterImplScaleTouchEventTest(
      const InputRouterImplScaleTouchEventTest&) = delete;
  InputRouterImplScaleTouchEventTest& operator=(
      const InputRouterImplScaleTouchEventTest&) = delete;

  // Test tests if two finger touch event at (10, 20) and (100, 200) are
  // properly scaled. The touch event must be generated ans flushed into
  // the message sink prior to this method.
  void RunTouchEventTest(const std::string& name, WebTouchPoint::State state) {
    SCOPED_TRACE(name);
    const WebTouchEvent* sent_event = GetSentWebInputEvent<WebTouchEvent>();
    ASSERT_EQ(2u, sent_event->touches_length);
    EXPECT_EQ(state, sent_event->touches[0].state);
    EXPECT_EQ(20, sent_event->touches[0].PositionInWidget().x());
    EXPECT_EQ(40, sent_event->touches[0].PositionInWidget().y());
    EXPECT_EQ(10, sent_event->touches[0].PositionInScreen().x());
    EXPECT_EQ(20, sent_event->touches[0].PositionInScreen().y());
    EXPECT_EQ(2 * radius_x_, sent_event->touches[0].radius_x);
    EXPECT_EQ(2 * radius_x_, sent_event->touches[0].radius_y);

    EXPECT_EQ(200, sent_event->touches[1].PositionInWidget().x());
    EXPECT_EQ(400, sent_event->touches[1].PositionInWidget().y());
    EXPECT_EQ(100, sent_event->touches[1].PositionInScreen().x());
    EXPECT_EQ(200, sent_event->touches[1].PositionInScreen().y());
    EXPECT_EQ(2 * radius_x_, sent_event->touches[1].radius_x);
    EXPECT_EQ(2 * radius_x_, sent_event->touches[1].radius_y);

    const WebTouchEvent* filter_event = GetFilterWebInputEvent<WebTouchEvent>();
    ASSERT_EQ(2u, filter_event->touches_length);
    EXPECT_EQ(10, filter_event->touches[0].PositionInWidget().x());
    EXPECT_EQ(20, filter_event->touches[0].PositionInWidget().y());
    EXPECT_EQ(10, filter_event->touches[0].PositionInScreen().x());
    EXPECT_EQ(20, filter_event->touches[0].PositionInScreen().y());
    EXPECT_EQ(radius_x_, filter_event->touches[0].radius_x);
    EXPECT_EQ(radius_x_, filter_event->touches[0].radius_y);

    EXPECT_EQ(100, filter_event->touches[1].PositionInWidget().x());
    EXPECT_EQ(200, filter_event->touches[1].PositionInWidget().y());
    EXPECT_EQ(100, filter_event->touches[1].PositionInScreen().x());
    EXPECT_EQ(200, filter_event->touches[1].PositionInScreen().y());
    EXPECT_EQ(radius_x_, filter_event->touches[1].radius_x);
    EXPECT_EQ(radius_x_, filter_event->touches[1].radius_y);
  }

  void FlushTouchEvent(WebInputEvent::Type type) {
    SendTouchEvent();
    UpdateDispatchedMessages();
    ASSERT_EQ(1u, dispatched_messages_.size());
    ASSERT_TRUE(dispatched_messages_[0]->ToEvent());
    dispatched_messages_[0]->ToEvent()->CallCallback(
        blink::mojom::InputEventResultState::kConsumed);
    ASSERT_TRUE(TouchEventQueueEmpty());
  }

  void ReleaseTouchPointAndAck(int index) {
    ReleaseTouchPoint(index);
    SendTouchEvent();
    UpdateDispatchedMessages();
    ASSERT_EQ(1u, dispatched_messages_.size());
    ASSERT_TRUE(dispatched_messages_[0]->ToEvent());
    dispatched_messages_[0]->ToEvent()->CallCallback(
        blink::mojom::InputEventResultState::kConsumed);
  }
};

}  // namespace

TEST_F(InputRouterImplScaleTouchEventTest, ScaleTouchEventTest) {
  ResetTouchAction();
  // Press
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::Type::kTouchStart);

  RunTouchEventTest("Press", WebTouchPoint::State::kStatePressed);
  ReleaseTouchPointAndAck(1);
  ReleaseTouchPointAndAck(0);

  // Move
  PressTouchPoint(0, 0);
  PressTouchPoint(0, 0);
  FlushTouchEvent(WebInputEvent::Type::kTouchStart);

  MoveTouchPoint(0, 10, 20);
  MoveTouchPoint(1, 100, 200);
  FlushTouchEvent(WebInputEvent::Type::kTouchMove);
  RunTouchEventTest("Move", WebTouchPoint::State::kStateMoved);
  ReleaseTouchPointAndAck(1);
  ReleaseTouchPointAndAck(0);

  // Release
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::Type::kTouchMove);

  ReleaseTouchPoint(0);
  ReleaseTouchPoint(1);
  FlushTouchEvent(WebInputEvent::Type::kTouchEnd);
  RunTouchEventTest("Release", WebTouchPoint::State::kStateReleased);

  // Cancel
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::Type::kTouchStart);

  CancelTouchPoint(0);
  CancelTouchPoint(1);
  FlushTouchEvent(WebInputEvent::Type::kTouchCancel);
  RunTouchEventTest("Cancel", WebTouchPoint::State::kStateCancelled);
}

namespace {

class InputRouterImplScaleGestureEventTest
    : public InputRouterImplScaleEventTest {
 public:
  InputRouterImplScaleGestureEventTest() {}

  InputRouterImplScaleGestureEventTest(
      const InputRouterImplScaleGestureEventTest&) = delete;
  InputRouterImplScaleGestureEventTest& operator=(
      const InputRouterImplScaleGestureEventTest&) = delete;

  std::optional<gfx::SizeF> GetContactSize(const WebGestureEvent& event) {
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
        return std::nullopt;
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
        type, blink::WebGestureDevice::kTouchscreen);
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
      ASSERT_EQ(expected_types[i],
                dispatched_messages_[i]->ToEvent()->Event()->Event().GetType());
      dispatched_messages_[i]->ToEvent()->CallCallback(
          blink::mojom::InputEventResultState::kConsumed);
    }
  }

  void TestLocationInSentEvent(
      const WebGestureEvent* sent_event,
      const gfx::PointF& orig,
      const gfx::PointF& scaled,
      const std::optional<gfx::SizeF>& contact_size_scaled) {
    EXPECT_FLOAT_EQ(scaled.x(), sent_event->PositionInWidget().x());
    EXPECT_FLOAT_EQ(scaled.y(), sent_event->PositionInWidget().y());
    EXPECT_FLOAT_EQ(orig.x(), sent_event->PositionInScreen().x());
    EXPECT_FLOAT_EQ(orig.y(), sent_event->PositionInScreen().y());

    std::optional<gfx::SizeF> event_contact_size = GetContactSize(*sent_event);
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
      const std::optional<gfx::SizeF>& contact_size) {
    EXPECT_FLOAT_EQ(orig.x(), filter_event->PositionInWidget().x());
    EXPECT_FLOAT_EQ(orig.y(), filter_event->PositionInWidget().y());
    EXPECT_FLOAT_EQ(orig.x(), filter_event->PositionInScreen().x());
    EXPECT_FLOAT_EQ(orig.y(), filter_event->PositionInScreen().y());

    std::optional<gfx::SizeF> event_contact_size =
        GetContactSize(*filter_event);
    if (event_contact_size && contact_size) {
      EXPECT_FLOAT_EQ(contact_size->width(), event_contact_size->width());
      EXPECT_FLOAT_EQ(contact_size->height(), event_contact_size->height());
    }
  }
};

}  // namespace

TEST_F(InputRouterImplScaleGestureEventTest, GestureScroll) {
  const gfx::Vector2dF delta(10.f, 20.f), delta_scaled(20.f, 40.f);

  PressAndSetTouchActionAuto();

  SendGestureSequence({WebInputEvent::Type::kGestureTapDown,
                       WebInputEvent::Type::kGestureTapCancel});

  {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollBegin(
        delta.x(), delta.y(), blink::WebGestureDevice::kTouchscreen));
    FlushGestureEvents({WebInputEvent::Type::kGestureScrollBegin});

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
                                     blink::WebGestureDevice::kTouchscreen);
    FlushGestureEvents({WebInputEvent::Type::kTouchScrollStarted,
                        WebInputEvent::Type::kGestureScrollUpdate});
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

  SendGestureSequence({WebInputEvent::Type::kGestureScrollEnd});
}

TEST_F(InputRouterImplScaleGestureEventTest, GesturePinch) {
  const gfx::PointF anchor(10.f, 20.f), anchor_scaled(20.f, 40.f);
  const float scale_change(1.5f);

  PressAndSetTouchActionAuto();

  SendGestureSequence({WebInputEvent::Type::kGestureTapDown,
                       WebInputEvent::Type::kGestureTapCancel});

  SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollBegin(
      0.f, 0.f, blink::WebGestureDevice::kTouchscreen));
  FlushGestureEvents({WebInputEvent::Type::kGestureScrollBegin});

  SendGestureSequence({WebInputEvent::Type::kGesturePinchBegin});

  SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildPinchUpdate(
      scale_change, anchor.x(), anchor.y(), 0,
      blink::WebGestureDevice::kTouchscreen));

  FlushGestureEvents({WebInputEvent::Type::kGesturePinchUpdate});
  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  TestLocationInSentEvent(sent_event, anchor, anchor_scaled, std::nullopt);
  EXPECT_FLOAT_EQ(scale_change, sent_event->data.pinch_update.scale);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  TestLocationInFilterEvent(filter_event, anchor, std::nullopt);
  EXPECT_FLOAT_EQ(scale_change, filter_event->data.pinch_update.scale);

  SendGestureSequence({WebInputEvent::Type::kGesturePinchEnd,
                       WebInputEvent::Type::kGestureScrollEnd});
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureTap) {
  SendGestureSequence({WebInputEvent::Type::kGestureTapDown,
                       WebInputEvent::Type::kGestureShowPress,
                       WebInputEvent::Type::kGestureTap});
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureDoubleTap) {
  SendGestureSequence({WebInputEvent::Type::kGestureTapDown,
                       WebInputEvent::Type::kGestureTapUnconfirmed,
                       WebInputEvent::Type::kGestureTapCancel,
                       WebInputEvent::Type::kGestureTapDown,
                       WebInputEvent::Type::kGestureTapCancel,
                       WebInputEvent::Type::kGestureDoubleTap});
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureLongPress) {
  SendGestureSequence({WebInputEvent::Type::kGestureTapDown,
                       WebInputEvent::Type::kGestureShowPress,
                       WebInputEvent::Type::kGestureLongPress,
                       WebInputEvent::Type::kGestureTapCancel,
                       WebInputEvent::Type::kGestureLongTap});
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureTwoFingerTap) {
  SendGestureSequence({WebInputEvent::Type::kGestureTapDown,
                       WebInputEvent::Type::kGestureTapCancel,
                       WebInputEvent::Type::kGestureTwoFingerTap});
}

}  // namespace content
