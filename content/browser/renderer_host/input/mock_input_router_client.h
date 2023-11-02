// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_

#include <stddef.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/input/fling_controller.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "ui/events/blink/did_overscroll_params.h"

namespace content {

class InputRouter;

class MockInputRouterClient : public InputRouterClient,
                              public FlingControllerSchedulerClient {
 public:
  MockInputRouterClient();
  ~MockInputRouterClient() override;

  // InputRouterClient
  blink::mojom::InputEventResultState FilterInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) override;
  void IncrementInFlightEventCount() override;
  void DecrementInFlightEventCount(
      blink::mojom::InputEventResultSource ack_source) override;
  void NotifyUISchedulerOfScrollStateUpdate(
      BrowserUIThreadScheduler::ScrollState scroll_state) override;
  void DidOverscroll(const ui::DidOverscrollParams& params) override;
  void OnSetCompositorAllowedTouchAction(cc::TouchAction touch_action) override;
  void DidStartScrollingViewport() override;
  void ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& latency_info) override;
  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency_info) override;
  bool IsWheelScrollInProgress() override;
  bool IsAutoscrollInProgress() override;
  void SetMouseCapture(bool capture) override {}
  void RequestMouseLock(
      bool user_gesture,
      bool unadjusted_movement,
      blink::mojom::WidgetInputHandlerHost::RequestMouseLockCallback response)
      override {}
  gfx::Size GetRootWidgetViewportSize() override;
  void OnInvalidInputEventSource() override {}

  bool GetAndResetFilterEventCalled();
  ui::DidOverscrollParams GetAndResetOverscroll();
  cc::TouchAction GetAndResetCompositorAllowedTouchAction();

  void set_input_router(InputRouter* input_router) {
    input_router_ = input_router;
  }

  void set_filter_state(blink::mojom::InputEventResultState filter_state) {
    filter_state_ = filter_state;
  }
  int in_flight_event_count() const {
    return in_flight_event_count_;
  }
  blink::WebInputEvent::Type last_in_flight_event_type() const {
    return last_filter_event()->GetType();
  }
  void set_allow_send_event(bool allow) {
    filter_state_ = blink::mojom::InputEventResultState::kNoConsumerExists;
  }
  const blink::WebInputEvent* last_filter_event() const {
    return last_filter_event_.get();
  }

  // FlingControllerSchedulerClient
  void ScheduleFlingProgress(
      base::WeakPtr<FlingController> fling_controller) override {}
  void DidStopFlingingOnBrowser(
      base::WeakPtr<FlingController> fling_controller) override {}
  bool NeedsBeginFrameForFlingProgress() override;

 private:
  raw_ptr<InputRouter> input_router_;
  int in_flight_event_count_;

  blink::mojom::InputEventResultState filter_state_;

  bool filter_input_event_called_;
  std::unique_ptr<blink::WebInputEvent> last_filter_event_;

  ui::DidOverscrollParams overscroll_;

  cc::TouchAction compositor_allowed_touch_action_;

  bool is_wheel_scroll_in_progress_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_
