// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mock_input_router_client.h"

#include "components/input/input_router.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseWheelEvent;

namespace content {

MockInputRouterClient::MockInputRouterClient()
    : input_router_(nullptr),
      filter_state_(blink::mojom::InputEventResultState::kNotConsumed),
      filter_input_event_called_(false),
      compositor_allowed_touch_action_(cc::TouchAction::kAuto) {}

MockInputRouterClient::~MockInputRouterClient() {}

blink::mojom::InputEventResultState MockInputRouterClient::FilterInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info) {
  filter_input_event_called_ = true;
  last_filter_event_ = input_event.Clone();
  return filter_state_;
}

void MockInputRouterClient::IncrementInFlightEventCount() {
  ++in_flight_event_count_;
}

void MockInputRouterClient::DecrementInFlightEventCount(
    blink::mojom::InputEventResultSource ack_source) {
  --in_flight_event_count_;
}

void MockInputRouterClient::NotifyUISchedulerOfGestureEventUpdate(
    blink::WebInputEvent::Type gesture_event) {}

void MockInputRouterClient::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  overscroll_ = params;
}

void MockInputRouterClient::OnSetCompositorAllowedTouchAction(
    cc::TouchAction compositor_allowed_touch_action) {
  compositor_allowed_touch_action_ = compositor_allowed_touch_action;
}

void MockInputRouterClient::DidStartScrollingViewport() {}

void MockInputRouterClient::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& gesture_event,
    const ui::LatencyInfo& latency_info) {
  if (input_router_)
    input_router_->SendGestureEvent(
        input::GestureEventWithLatencyInfo(gesture_event, latency_info));

  if (gesture_event.SourceDevice() != blink::WebGestureDevice::kTouchpad)
    return;

  if (gesture_event.GetType() == WebInputEvent::Type::kGestureScrollBegin) {
    is_wheel_scroll_in_progress_ = true;
  } else if (gesture_event.GetType() ==
             WebInputEvent::Type::kGestureScrollEnd) {
    is_wheel_scroll_in_progress_ = false;
  }
}

void MockInputRouterClient::ForwardWheelEventWithLatencyInfo(
    const blink::WebMouseWheelEvent& wheel_event,
    const ui::LatencyInfo& latency_info) {
  if (input_router_) {
    input_router_->SendWheelEvent(
        input::MouseWheelEventWithLatencyInfo(wheel_event, latency_info));
  }
}

bool MockInputRouterClient::IsWheelScrollInProgress() {
  return is_wheel_scroll_in_progress_;
}

bool MockInputRouterClient::IsAutoscrollInProgress() {
  return false;
}

gfx::Size MockInputRouterClient::GetRootWidgetViewportSize() {
  return gfx::Size(1920, 1080);
}

bool MockInputRouterClient::GetAndResetFilterEventCalled() {
  bool filter_input_event_called = filter_input_event_called_;
  filter_input_event_called_ = false;
  return filter_input_event_called;
}

ui::DidOverscrollParams MockInputRouterClient::GetAndResetOverscroll() {
  ui::DidOverscrollParams overscroll;
  std::swap(overscroll_, overscroll);
  return overscroll;
}

cc::TouchAction
MockInputRouterClient::GetAndResetCompositorAllowedTouchAction() {
  cc::TouchAction allowed = compositor_allowed_touch_action_;
  compositor_allowed_touch_action_ = cc::TouchAction::kAuto;
  return allowed;
}

bool MockInputRouterClient::NeedsBeginFrameForFlingProgress() {
  return false;
}

bool MockInputRouterClient::ShouldUseMobileFlingCurve() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return true;
#else
  return false;
#endif
}

gfx::Vector2dF MockInputRouterClient::GetPixelsPerInch(
    const gfx::PointF& position_in_screen) {
  return gfx::Vector2dF(input::kDefaultPixelsPerInch,
                        input::kDefaultPixelsPerInch);
}

blink::mojom::WidgetInputHandler*
MockInputRouterClient::GetWidgetInputHandler() {
  return &widget_input_handler_;
}

input::StylusInterface* MockInputRouterClient::GetStylusInterface() {
  return render_widget_host_view_;
}

void MockInputRouterClient::OnStartStylusWriting() {
  on_start_stylus_writing_called_ = true;
}

}  // namespace content
