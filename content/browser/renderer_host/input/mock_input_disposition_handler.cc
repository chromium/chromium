// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mock_input_disposition_handler.h"

#include "base/functional/bind.h"
#include "components/input/input_router.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebInputEvent;

namespace content {

MockInputDispositionHandler::MockInputDispositionHandler()
    : input_router_(nullptr),
      ack_count_(0),
      ack_event_type_(WebInputEvent::Type::kUndefined),
      ack_state_(blink::mojom::InputEventResultState::kUnknown) {}

MockInputDispositionHandler::~MockInputDispositionHandler() {}

input::InputRouter::KeyboardEventCallback
MockInputDispositionHandler::CreateKeyboardEventCallback() {
  return base::BindOnce(&MockInputDispositionHandler::OnKeyboardEventAck,
                        base::Unretained(this));
}

input::InputRouter::MouseEventCallback
MockInputDispositionHandler::CreateMouseEventCallback() {
  return base::BindOnce(&MockInputDispositionHandler::OnMouseEventAck,
                        base::Unretained(this));
}

void MockInputDispositionHandler::OnWheelEventAck(
    const input::MouseWheelEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_wheel_event_ = event.event;
  acked_wheel_event_state_ = ack_result;
  RecordAckCalled(event.event.GetType(), ack_result);
}

void MockInputDispositionHandler::OnTouchEventAck(
    const input::TouchEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_touch_event_ = event;
  RecordAckCalled(event.event.GetType(), ack_result);
  if (touch_followup_event_)
    input_router_->SendTouchEvent(*touch_followup_event_);
  if (gesture_followup_event_)
    input_router_->SendGestureEvent(*gesture_followup_event_);
}

void MockInputDispositionHandler::OnGestureEventAck(
    const input::GestureEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_gesture_event_ = event.event;
  RecordAckCalled(event.event.GetType(), ack_result);
}

size_t MockInputDispositionHandler::GetAndResetAckCount() {
  size_t ack_count = ack_count_;
  ack_count_ = 0;
  return ack_count;
}

void MockInputDispositionHandler::RecordAckCalled(
    blink::WebInputEvent::Type type,
    blink::mojom::InputEventResultState ack_result) {
  ack_event_type_ = type;
  ++ack_count_;
  ack_state_ = ack_result;
}

void MockInputDispositionHandler::OnKeyboardEventAck(
    const input::NativeWebKeyboardEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_key_event_ =
      std::make_unique<input::NativeWebKeyboardEvent>(event.event);
  RecordAckCalled(event.event.GetType(), ack_result);
}

void MockInputDispositionHandler::OnMouseEventAck(
    const input::MouseEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_mouse_event_ = event.event;
  RecordAckCalled(event.event.GetType(), ack_result);
}

}  // namespace content
