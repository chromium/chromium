// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mock_input_disposition_handler.h"

#include "base/bind.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

MockInputDispositionHandler::MockInputDispositionHandler()
    : input_router_(nullptr),
      ack_count_(0),
      ack_event_type_(WebInputEvent::kUndefined),
      ack_state_(INPUT_EVENT_ACK_STATE_UNKNOWN) {}

MockInputDispositionHandler::~MockInputDispositionHandler() {}

InputRouter::KeyboardEventCallback
MockInputDispositionHandler::CreateKeyboardEventCallback() {
  return base::BindOnce(&MockInputDispositionHandler::OnKeyboardEventAck,
                        base::Unretained(this));
}

InputRouter::MouseEventCallback
MockInputDispositionHandler::CreateMouseEventCallback() {
  return base::BindOnce(&MockInputDispositionHandler::OnMouseEventAck,
                        base::Unretained(this));
}

void MockInputDispositionHandler::OnWheelEventAck(
    const MouseWheelEventWithLatencyInfo& event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_wheel_event_ = event.event;
  acked_wheel_event_state_ = ack_result;
  RecordAckCalled(event.event.GetType(), ack_result);
}

void MockInputDispositionHandler::OnTouchEventAck(
    const TouchEventWithLatencyInfo& event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_touch_event_ = event;
  RecordAckCalled(event.event.GetType(), ack_result);
  if (touch_followup_event_)
    input_router_->SendTouchEvent(*touch_followup_event_);
  if (gesture_followup_event_)
    input_router_->SendGestureEvent(*gesture_followup_event_);
}

void MockInputDispositionHandler::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
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
    InputEventAckState ack_result) {
  ack_event_type_ = type;
  ++ack_count_;
  ack_state_ = ack_result;
}

void MockInputDispositionHandler::OnKeyboardEventAck(
    const NativeWebKeyboardEventWithLatencyInfo& event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_key_event_ = std::make_unique<NativeWebKeyboardEvent>(event.event);
  RecordAckCalled(event.event.GetType(), ack_result);
}

void MockInputDispositionHandler::OnMouseEventAck(
    const MouseEventWithLatencyInfo& event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_mouse_event_ = event.event;
  RecordAckCalled(event.event.GetType(), ack_result);
}

}  // namespace content
