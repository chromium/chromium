// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_RENDER_INPUT_ROUTER_H_
#define CONTENT_TEST_MOCK_RENDER_INPUT_ROUTER_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "components/input/render_input_router.h"
#include "content/test/mock_widget_input_handler.h"

using blink::WebGestureEvent;

namespace content {

class MockRenderInputRouter : public input::RenderInputRouter {
 public:
  using input::RenderInputRouter::input_router_;

  explicit MockRenderInputRouter(
      input::RenderInputRouterClient* host,
      std::unique_ptr<input::FlingSchedulerBase> fling_scheduler,
      input::RenderInputRouterDelegate* delegate,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  MockRenderInputRouter(const MockRenderInputRouter&) = delete;
  MockRenderInputRouter& operator=(const MockRenderInputRouter&) = delete;

  ~MockRenderInputRouter() override;

  // InputRouterImplClient overrides.
  blink::mojom::WidgetInputHandler* GetWidgetInputHandler() override;

  // InputDispositionHandler overrides.
  void OnTouchEventAck(const input::TouchEventWithLatencyInfo& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;

  void SetupForInputRouterTest();

  void ForwardTouchEventWithLatencyInfo(
      const blink::WebTouchEvent& touch_event,
      const ui::LatencyInfo& ui_latency) override;

  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& ui_latency) override;

  std::optional<WebGestureEvent> GetAndResetLastForwardedGestureEvent();

  void SetLastWheelOrTouchEventLatencyInfo(ui::LatencyInfo latency_info) {
    last_wheel_or_touch_event_latency_info_ = latency_info;
  }

  std::optional<ui::LatencyInfo> GetLastWheelOrTouchEventLatencyInfo() {
    return last_wheel_or_touch_event_latency_info_;
  }

  MockWidgetInputHandler::MessageVector GetAndResetDispatchedMessages();

  std::optional<blink::WebInputEvent::Type> acked_touch_event_type() const {
    return acked_touch_event_type_;
  }

  std::optional<blink::WebInputEvent::Type> acked_touch_event_type_;

  std::unique_ptr<MockWidgetInputHandler> mock_widget_input_handler_;

 private:
  std::optional<ui::LatencyInfo> last_wheel_or_touch_event_latency_info_;
  std::optional<WebGestureEvent> last_forwarded_gesture_event_;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_RENDER_INPUT_ROUTER_H_
