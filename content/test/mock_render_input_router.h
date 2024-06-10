// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_RENDER_INPUT_ROUTER_H_
#define CONTENT_TEST_MOCK_RENDER_INPUT_ROUTER_H_

#include <stddef.h>
#include <memory>
#include <utility>

#include "content/browser/renderer_host/input/mock_input_router.h"
#include "content/common/input/render_input_router.h"
#include "content/test/mock_widget_input_handler.h"

namespace content {

class MockRenderInputRouter : public RenderInputRouter {
 public:
  using RenderInputRouter::input_router_;

  explicit MockRenderInputRouter(
      InputRouterImplClient* host,
      std::unique_ptr<input::FlingSchedulerBase> fling_scheduler,
      RenderInputRouterDelegate* delegate,
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
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_RENDER_INPUT_ROUTER_H_
