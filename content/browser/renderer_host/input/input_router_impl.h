// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "cc/input/touch_action.h"
#include "content/browser/renderer_host/input/fling_scheduler.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/mouse_wheel_event_queue.h"
#include "content/browser/renderer_host/input/passthrough_touch_event_queue.h"
#include "content/browser/renderer_host/input/touch_action_filter.h"
#include "content/browser/renderer_host/input/touchpad_pinch_event_queue.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_stream_validator.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"

namespace ui {
class LatencyInfo;
}  // namespace ui

namespace content {

class InputDispositionHandler;

class MockRenderWidgetHost;

class CONTENT_EXPORT InputRouterImplClient : public InputRouterClient {
 public:
  virtual blink::mojom::WidgetInputHandler* GetWidgetInputHandler() = 0;
  virtual void OnImeCancelComposition() = 0;
  virtual void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& bounds) = 0;
  virtual RenderWidgetHostViewBase* GetRenderWidgetHostViewBase() = 0;
  virtual void OnStartStylusWriting() = 0;
};

// A default implementation for browser input event routing.
class CONTENT_EXPORT InputRouterImpl
    : public InputRouter,
      public GestureEventQueueClient,
      public FlingControllerEventSenderClient,
      public MouseWheelEventQueueClient,
      public PassthroughTouchEventQueueClient,
      public TouchpadPinchEventQueueClient,
      public blink::mojom::WidgetInputHandlerHost {
 public:
  InputRouterImpl(InputRouterImplClient* client,
                  InputDispositionHandler* disposition_handler,
                  FlingControllerSchedulerClient* fling_scheduler_client,
                  const Config& config);

  InputRouterImpl(const InputRouterImpl&) = delete;
  InputRouterImpl& operator=(const InputRouterImpl&) = delete;

  ~InputRouterImpl() override;

  // InputRouter
  void SendMouseEvent(const MouseEventWithLatencyInfo& mouse_event,
                      MouseEventCallback event_result_callback) override;
  void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override;
  void SendKeyboardEvent(const NativeWebKeyboardEventWithLatencyInfo& key_event,
                         KeyboardEventCallback event_result_callback) override;
  void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) override;
  void SendTouchEvent(const TouchEventWithLatencyInfo& touch_event) override;
  void NotifySiteIsMobileOptimized(bool is_mobile_optimized) override;
  bool HasPendingEvents() const override;
  void SetDeviceScaleFactor(float device_scale_factor) override;
  void SetForceEnableZoom(bool enabled) override;
  absl::optional<cc::TouchAction> AllowedTouchAction() override;
  absl::optional<cc::TouchAction> ActiveTouchAction() override;
  mojo::PendingRemote<blink::mojom::WidgetInputHandlerHost> BindNewHost(
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;
  void StopFling() override;
  void ForceSetTouchActionAuto() override;

  // InputHandlerHost impl
  void SetTouchActionFromMain(cc::TouchAction touch_action) override;
  void SetPanAction(blink::mojom::PanAction pan_action) override;
  void DidOverscroll(blink::mojom::DidOverscrollParamsPtr params) override;
  void ImeCancelComposition() override;
  void DidStartScrollingViewport() override;
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& bounds) override;
  void SetMouseCapture(bool capture) override;
  void RequestMouseLock(bool from_user_gesture,
                        bool unadjusted_movement,
                        RequestMouseLockCallback response) override;
  // Notifies touch action filter and touch event queue whether there are
  // JavaScript touch event handlers or not, or whether the platform has
  // hit-testable scrollbars.
  void OnHasTouchEventConsumers(
      blink::mojom::TouchEventConsumersPtr consumers) override;
  void WaitForInputProcessed(base::OnceClosure callback) override;
  void FlushTouchEventQueue() override;

  // Exposed so that tests can swap out the implementation and intercept calls.
  mojo::Receiver<blink::mojom::WidgetInputHandlerHost>&
  host_receiver_for_testing() {
    return host_receiver_;
  }

  void ForceResetTouchActionForTest();

  bool IsFlingActiveForTest();

 private:
  friend class InputRouterImplTest;
  friend class InputRouterImplTestBase;
  friend class MockRenderWidgetHost;
  friend class RenderWidgetHostSitePerProcessTest;
  friend class SitePerProcessBrowserTouchActionTest;

  // Keeps track of last position of touch points and sets MovementXY for them.
  void SetMovementXYForTouchPoints(blink::WebTouchEvent* event);

  void SendMouseEventImmediately(const MouseEventWithLatencyInfo& mouse_event,
                                 MouseEventCallback event_result_callback);

  // PassthroughTouchEventQueueClient
  void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& touch_event) override;
  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;
  void OnFilteringTouchEvent(const blink::WebTouchEvent& touch_event) override;
  void FlushDeferredGestureQueue() override;

  // GestureEventFilterClient
  void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& gesture_event) override;
  void OnGestureEventAck(
      const GestureEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result,
      blink::mojom::ScrollResultDataPtr scroll_result_data) override;

  // FlingControllerEventSenderClient
  void SendGeneratedWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override;
  void SendGeneratedGestureScrollEvents(
      const GestureEventWithLatencyInfo& gesture_event) override;
  gfx::Size GetRootWidgetViewportSize() override;

  // MouseWheelEventQueueClient
  void SendMouseWheelEventImmediately(
      const MouseWheelEventWithLatencyInfo& touch_event,
      MouseWheelEventQueueClient::MouseWheelEventHandledCallback callback)
      override;
  void OnMouseWheelEventAck(
      const MouseWheelEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) override;
  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency_info) override;
  bool IsWheelScrollInProgress() override;
  bool IsAutoscrollInProgress() override;

  // TouchpadPinchEventQueueClient
  void SendMouseWheelEventForPinchImmediately(
      const MouseWheelEventWithLatencyInfo& event,
      TouchpadPinchEventQueueClient::MouseWheelEventHandledCallback callback)
      override;
  void OnGestureEventForPinchAck(
      const GestureEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) override;

  bool HandleGestureScrollForStylusWriting(const blink::WebGestureEvent& event);

  void FilterAndSendWebInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info,
      blink::mojom::WidgetInputHandler::DispatchEventCallback callback);

  void KeyboardEventHandled(
      const NativeWebKeyboardEventWithLatencyInfo& event,
      KeyboardEventCallback event_result_callback,
      blink::mojom::InputEventResultSource source,
      const ui::LatencyInfo& latency,
      blink::mojom::InputEventResultState state,
      blink::mojom::DidOverscrollParamsPtr overscroll,
      blink::mojom::TouchActionOptionalPtr touch_action,
      blink::mojom::ScrollResultDataPtr scroll_result_data);
  void MouseEventHandled(const MouseEventWithLatencyInfo& event,
                         MouseEventCallback event_result_callback,
                         blink::mojom::InputEventResultSource source,
                         const ui::LatencyInfo& latency,
                         blink::mojom::InputEventResultState state,
                         blink::mojom::DidOverscrollParamsPtr overscroll,
                         blink::mojom::TouchActionOptionalPtr touch_action,
                         blink::mojom::ScrollResultDataPtr scroll_result_data);
  void TouchEventHandled(const TouchEventWithLatencyInfo& touch_event,
                         blink::mojom::InputEventResultSource source,
                         const ui::LatencyInfo& latency,
                         blink::mojom::InputEventResultState state,
                         blink::mojom::DidOverscrollParamsPtr overscroll,
                         blink::mojom::TouchActionOptionalPtr touch_action,
                         blink::mojom::ScrollResultDataPtr scroll_result_data);
  void GestureEventHandled(
      const GestureEventWithLatencyInfo& gesture_event,
      blink::mojom::InputEventResultSource source,
      const ui::LatencyInfo& latency,
      blink::mojom::InputEventResultState state,
      blink::mojom::DidOverscrollParamsPtr overscroll,
      blink::mojom::TouchActionOptionalPtr touch_action,
      blink::mojom::ScrollResultDataPtr scroll_result_data);
  void MouseWheelEventHandled(
      const MouseWheelEventWithLatencyInfo& event,
      MouseWheelEventQueueClient::MouseWheelEventHandledCallback callback,
      blink::mojom::InputEventResultSource source,
      const ui::LatencyInfo& latency,
      blink::mojom::InputEventResultState state,
      blink::mojom::DidOverscrollParamsPtr overscroll,
      blink::mojom::TouchActionOptionalPtr touch_action,
      blink::mojom::ScrollResultDataPtr scroll_result_data);

  // Called when a touch timeout-affecting bit has changed, in turn toggling the
  // touch ack timeout feature of the |touch_event_queue_| as appropriate. Input
  // to that determination includes current view properties and the allowed
  // touch action. Note that this will only affect platforms that have a
  // non-zero touch timeout configuration.
  void UpdateTouchAckTimeoutEnabled();

  void SendGestureEventWithoutQueueing(
      GestureEventWithLatencyInfo& gesture_event,
      const FilterGestureEventResult& existing_result);
  void ProcessDeferredGestureEventQueue();
  void OnSetCompositorAllowedTouchAction(cc::TouchAction touch_action);

  raw_ptr<InputRouterImplClient> client_;
  raw_ptr<InputDispositionHandler> disposition_handler_;

  // Whether the TouchScrollStarted event has been sent for the current
  // gesture scroll yet.
  bool touch_scroll_started_sent_;

  // Whether stylus writing has started.
  bool stylus_writing_started_ = false;

  // Stores the pan action that can take place like stylus writing, moving
  // cursor or scrolling. This is sent when the pointer is hovering and set from
  // the main thread.
  blink::mojom::PanAction pan_action_ = blink::mojom::PanAction::kNone;

  MouseWheelEventQueue wheel_event_queue_;
  PassthroughTouchEventQueue touch_event_queue_;
  TouchpadPinchEventQueue touchpad_pinch_event_queue_;
  GestureEventQueue gesture_event_queue_;
  TouchActionFilter touch_action_filter_;
  InputEventStreamValidator input_stream_validator_;
  InputEventStreamValidator output_stream_validator_;

  float device_scale_factor_;

  // Last touch position relative to screen. Used to compute movementX/Y.
  base::flat_map<int, gfx::Point> global_touch_position_;

  // The host receiver associated with the widget input handler from
  // the widget.
  mojo::Receiver<blink::mojom::WidgetInputHandlerHost> host_receiver_{this};

  base::WeakPtr<InputRouterImpl> weak_this_;
  base::WeakPtrFactory<InputRouterImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_IMPL_H_
