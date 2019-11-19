// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mock_input_router_client.h"

#include "content/browser/renderer_host/input/input_router.h"
#include "content/common/input/input_event.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

MockInputRouterClient::MockInputRouterClient()
    : input_router_(nullptr),
      in_flight_event_count_(0),
      filter_state_(INPUT_EVENT_ACK_STATE_NOT_CONSUMED),
      filter_input_event_called_(false),
      white_listed_touch_action_(cc::kTouchActionAuto) {}

MockInputRouterClient::~MockInputRouterClient() {}

InputEventAckState MockInputRouterClient::FilterInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info) {
  filter_input_event_called_ = true;
  last_filter_event_.reset(new InputEvent(input_event, latency_info));
  return filter_state_;
}

void MockInputRouterClient::IncrementInFlightEventCount() {
  ++in_flight_event_count_;
}

void MockInputRouterClient::DecrementInFlightEventCount(
    InputEventAckSource ack_source) {
  --in_flight_event_count_;
}

void MockInputRouterClient::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  overscroll_ = params;
}

void MockInputRouterClient::OnSetWhiteListedTouchAction(
    cc::TouchAction white_listed_touch_action) {
  white_listed_touch_action_ = white_listed_touch_action;
}

void MockInputRouterClient::DidStartScrollingViewport() {}

void MockInputRouterClient::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& gesture_event,
    const ui::LatencyInfo& latency_info) {
  if (input_router_)
    input_router_->SendGestureEvent(
        GestureEventWithLatencyInfo(gesture_event, latency_info));

  if (gesture_event.SourceDevice() != blink::WebGestureDevice::kTouchpad)
    return;

  if (gesture_event.GetType() == WebInputEvent::kGestureScrollBegin) {
    is_wheel_scroll_in_progress_ = true;
  } else if (gesture_event.GetType() == WebInputEvent::kGestureScrollEnd) {
    is_wheel_scroll_in_progress_ = false;
  }
}

void MockInputRouterClient::ForwardWheelEventWithLatencyInfo(
    const blink::WebMouseWheelEvent& wheel_event,
    const ui::LatencyInfo& latency_info) {
  if (input_router_) {
    input_router_->SendWheelEvent(
        MouseWheelEventWithLatencyInfo(wheel_event, latency_info));
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

cc::TouchAction MockInputRouterClient::GetAndResetWhiteListedTouchAction() {
  cc::TouchAction white_listed_touch_action = white_listed_touch_action_;
  white_listed_touch_action_ = cc::kTouchActionAuto;
  return white_listed_touch_action;
}

bool MockInputRouterClient::NeedsBeginFrameForFlingProgress() {
  return false;
}

}  // namespace content
