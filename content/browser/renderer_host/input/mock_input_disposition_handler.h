// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_DISPOSITION_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_DISPOSITION_HANDLER_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "components/input/input_disposition_handler.h"
#include "components/input/input_router.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"

namespace content {

class MockInputDispositionHandler : public input::InputDispositionHandler {
 public:
  MockInputDispositionHandler();
  ~MockInputDispositionHandler() override;

  input::InputRouter::KeyboardEventCallback CreateKeyboardEventCallback();
  input::InputRouter::MouseEventCallback CreateMouseEventCallback();

  // InputDispositionHandler
  void OnWheelEventAck(const input::MouseWheelEventWithLatencyInfo& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;
  void OnTouchEventAck(const input::TouchEventWithLatencyInfo& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;
  void OnGestureEventAck(
      const input::GestureEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) override;

  size_t GetAndResetAckCount();

  void set_input_router(input::InputRouter* input_router) {
    input_router_ = input_router;
  }

  void set_followup_touch_event(
      std::unique_ptr<input::GestureEventWithLatencyInfo> event) {
    gesture_followup_event_ = std::move(event);
  }

  void set_followup_touch_event(
      std::unique_ptr<input::TouchEventWithLatencyInfo> event) {
    touch_followup_event_ = std::move(event);
  }

  blink::mojom::InputEventResultState ack_state() const { return ack_state_; }

  blink::mojom::InputEventResultState acked_wheel_event_state() const {
    return acked_wheel_event_state_;
  }

  blink::WebInputEvent::Type ack_event_type() const { return ack_event_type_; }

  const input::NativeWebKeyboardEvent& acked_keyboard_event() const {
    return *acked_key_event_;
  }
  const blink::WebMouseWheelEvent& acked_wheel_event() const {
    return acked_wheel_event_;
  }
  const input::TouchEventWithLatencyInfo& acked_touch_event() const {
    return acked_touch_event_;
  }
  const blink::WebGestureEvent& acked_gesture_event() const {
    return acked_gesture_event_;
  }

 private:
  void RecordAckCalled(blink::WebInputEvent::Type eventType,
                       blink::mojom::InputEventResultState ack_result);

  void OnKeyboardEventAck(
      const input::NativeWebKeyboardEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result);
  void OnMouseEventAck(const input::MouseEventWithLatencyInfo& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result);

  raw_ptr<input::InputRouter, DanglingUntriaged> input_router_;

  size_t ack_count_;
  blink::WebInputEvent::Type ack_event_type_;
  blink::mojom::InputEventResultState ack_state_;
  blink::mojom::InputEventResultState acked_wheel_event_state_;
  std::unique_ptr<input::NativeWebKeyboardEvent> acked_key_event_;
  blink::WebMouseWheelEvent acked_wheel_event_;
  input::TouchEventWithLatencyInfo acked_touch_event_;
  blink::WebGestureEvent acked_gesture_event_;
  blink::WebMouseEvent acked_mouse_event_;

  std::unique_ptr<input::GestureEventWithLatencyInfo> gesture_followup_event_;
  std::unique_ptr<input::TouchEventWithLatencyInfo> touch_followup_event_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_DISPOSITION_HANDLER_H_
