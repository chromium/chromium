// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mock_input_router.h"

#include "content/browser/renderer_host/input/input_router_client.h"

namespace content {

void MockInputRouter::SendMouseEvent(
    const MouseEventWithLatencyInfo& mouse_event,
    MouseEventCallback event_result_callback) {
  sent_mouse_event_ = true;
}
void MockInputRouter::SendWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  sent_wheel_event_ = true;
}
void MockInputRouter::SendKeyboardEvent(
    const NativeWebKeyboardEventWithLatencyInfo& key_event,
    KeyboardEventCallback event_result_callback) {
  sent_keyboard_event_ = true;
}
void MockInputRouter::SendGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  sent_gesture_event_ = true;
}
void MockInputRouter::SendTouchEvent(
    const TouchEventWithLatencyInfo& touch_event) {
  send_touch_event_not_cancelled_ =
      client_->FilterInputEvent(touch_event.event, touch_event.latency) ==
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

bool MockInputRouter::HasPendingEvents() const {
  return false;
}

base::Optional<cc::TouchAction> MockInputRouter::AllowedTouchAction() {
  return cc::kTouchActionAuto;
}

base::Optional<cc::TouchAction> MockInputRouter::ActiveTouchAction() {
  return cc::kTouchActionAuto;
}

mojo::PendingRemote<mojom::WidgetInputHandlerHost>
MockInputRouter::BindNewHost() {
  return mojo::NullRemote();
}

mojo::PendingRemote<mojom::WidgetInputHandlerHost>
MockInputRouter::BindNewFrameHost() {
  return mojo::NullRemote();
}

void MockInputRouter::OnHasTouchEventHandlers(bool has_handlers) {
  has_handlers_ = has_handlers;
}

}  // namespace content
