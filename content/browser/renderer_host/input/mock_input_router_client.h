// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_

#include <stddef.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/input/fling_controller.h"
#include "components/input/input_router.h"
#include "components/input/input_router_client.h"
#include "content/browser/renderer_host/input/mock_render_widget_host_view_for_stylus_writing.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/test/mock_widget_input_handler.h"
#include "ui/events/blink/did_overscroll_params.h"

namespace content {

class MockInputRouterClient : public input::InputRouterClient,
                              public input::FlingControllerSchedulerClient {
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
  void NotifyUISchedulerOfGestureEventUpdate(
      blink::WebInputEvent::Type gesture_event) override;
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
  void SetAutoscrollSelectionActiveInMainFrame(
      bool autoscroll_selection) override {}
  void RequestMouseLock(
      bool user_gesture,
      bool unadjusted_movement,
      blink::mojom::WidgetInputHandlerHost::RequestMouseLockCallback response)
      override {}
  gfx::Size GetRootWidgetViewportSize() override;
  void OnInvalidInputEventSource() override {}
  blink::mojom::WidgetInputHandler* GetWidgetInputHandler() override;
  void OnImeCancelComposition() override {}
  void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::optional<std::vector<gfx::Rect>>& character_bounds,
      const std::optional<std::vector<gfx::Rect>>& line_bounds) override {}
  input::StylusInterface* GetStylusInterface() override;
  void OnStartStylusWriting() override;

  bool GetAndResetFilterEventCalled();
  ui::DidOverscrollParams GetAndResetOverscroll();
  cc::TouchAction GetAndResetCompositorAllowedTouchAction();

  void set_input_router(input::InputRouter* input_router) {
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
  bool on_start_stylus_writing_called() const {
    return on_start_stylus_writing_called_;
  }
  MockWidgetInputHandler::MessageVector GetAndResetDispatchedMessages() {
    return widget_input_handler_.GetAndResetDispatchedMessages();
  }
  void set_render_widget_host_view(
      MockRenderWidgetHostViewForStylusWriting* view) {
    render_widget_host_view_ = view;
  }

  // FlingControllerSchedulerClient
  void ScheduleFlingProgress(
      base::WeakPtr<input::FlingController> fling_controller) override {}
  void DidStopFlingingOnBrowser(
      base::WeakPtr<input::FlingController> fling_controller) override {}
  bool NeedsBeginFrameForFlingProgress() override;
  bool ShouldUseMobileFlingCurve() override;
  gfx::Vector2dF GetPixelsPerInch(
      const gfx::PointF& position_in_screen) override;

 private:
  raw_ptr<input::InputRouter, DanglingUntriaged> input_router_;
  int in_flight_event_count_ = 0;

  blink::mojom::InputEventResultState filter_state_;

  bool filter_input_event_called_;
  std::unique_ptr<blink::WebInputEvent> last_filter_event_;

  ui::DidOverscrollParams overscroll_;

  cc::TouchAction compositor_allowed_touch_action_;

  bool is_wheel_scroll_in_progress_ = false;
  MockWidgetInputHandler widget_input_handler_;
  raw_ptr<MockRenderWidgetHostViewForStylusWriting> render_widget_host_view_;
  bool on_start_stylus_writing_called_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_
