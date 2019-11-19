// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_

#include <stddef.h>

#include <memory>

#include "content/browser/renderer_host/input/fling_controller.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/common/input/input_event.h"
#include "ui/events/blink/did_overscroll_params.h"

namespace content {

class InputRouter;

class MockInputRouterClient : public InputRouterClient,
                              public FlingControllerSchedulerClient {
 public:
  MockInputRouterClient();
  ~MockInputRouterClient() override;

  // InputRouterClient
  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) override;
  void IncrementInFlightEventCount() override;
  void DecrementInFlightEventCount(InputEventAckSource ack_source) override;
  void DidOverscroll(const ui::DidOverscrollParams& params) override;
  void OnSetWhiteListedTouchAction(cc::TouchAction touch_action) override;
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
  void FallbackCursorModeLockCursor(bool left,
                                    bool right,
                                    bool up,
                                    bool down) override {}
  void FallbackCursorModeSetCursorVisibility(bool visible) override {}
  gfx::Size GetRootWidgetViewportSize() override;

  bool GetAndResetFilterEventCalled();
  ui::DidOverscrollParams GetAndResetOverscroll();
  cc::TouchAction GetAndResetWhiteListedTouchAction();

  void set_input_router(InputRouter* input_router) {
    input_router_ = input_router;
  }

  void set_filter_state(InputEventAckState filter_state) {
    filter_state_ = filter_state;
  }
  int in_flight_event_count() const {
    return in_flight_event_count_;
  }
  blink::WebInputEvent::Type last_in_flight_event_type() const {
    return last_filter_event()->GetType();
  }
  void set_allow_send_event(bool allow) {
    filter_state_ = INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  }
  const blink::WebInputEvent* last_filter_event() const {
    return last_filter_event_->web_event.get();
  }

  // FlingControllerSchedulerClient
  void ScheduleFlingProgress(
      base::WeakPtr<FlingController> fling_controller) override {}
  void DidStopFlingingOnBrowser(
      base::WeakPtr<FlingController> fling_controller) override {}
  bool NeedsBeginFrameForFlingProgress() override;

 private:
  InputRouter* input_router_;
  int in_flight_event_count_;

  InputEventAckState filter_state_;

  bool filter_input_event_called_;
  std::unique_ptr<InputEvent> last_filter_event_;

  ui::DidOverscrollParams overscroll_;

  cc::TouchAction white_listed_touch_action_;

  bool is_wheel_scroll_in_progress_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_
