// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_CLIENT_H_

#include "cc/input/touch_action.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"

namespace ui {
class LatencyInfo;
struct DidOverscrollParams;
}

namespace content {

class InputRouterClient {
 public:
  virtual ~InputRouterClient() {}

  // Called just prior to events being sent to the renderer, giving the client
  // a chance to perform in-process event filtering.
  // The returned disposition will yield the following behavior:
  //   * |NOT_CONSUMED| will result in |input_event| being sent as usual.
  //   * |CONSUMED| or |NO_CONSUMER_EXISTS| will trigger the appropriate ack.
  //   * |UNKNOWN| will result in |input_event| being dropped.
  virtual blink::mojom::InputEventResultState FilterInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) = 0;

  // Called each time a WebInputEvent IPC is sent.
  virtual void IncrementInFlightEventCount() = 0;

  // Called each time a WebInputEvent ACK IPC is received.
  virtual void DecrementInFlightEventCount(
      blink::mojom::InputEventResultSource ack_source) = 0;

  // Called each time the browser UI scheduler should be notified of a scroll
  // state update.
  virtual void NotifyUISchedulerOfScrollStateUpdate(
      BrowserUIThreadScheduler::ScrollState scroll_state) = 0;

  // Called when the router has received an overscroll notification from the
  // renderer.
  virtual void DidOverscroll(const ui::DidOverscrollParams& params) = 0;

  // Called when the router has received an allowed touch action notification
  // from the renderer.
  virtual void OnSetCompositorAllowedTouchAction(cc::TouchAction) = 0;

  // Called when a GSB has started scrolling a viewport.
  virtual void DidStartScrollingViewport() = 0;

  // Called when the input router generates an event. It is intended that the
  // client will do some processing on |gesture_event| and then send it back
  // to the InputRouter via SendGestureEvent.
  virtual void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency_info) = 0;

  // Called when the input router generates a wheel event. It is intended that
  // the client will do some processing on |wheel_event| and then send it back
  // to the InputRouter via SendWheelEvent.
  virtual void ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& latency_info) = 0;

  // Called to see if there is an ongoing wheel scroll sequence on the client.
  virtual bool IsWheelScrollInProgress() = 0;

  // Called to see if the mouse has entered the autoscroll mode. Note that when
  // this function returns true it does not necessarily mean that a GSB with
  // autoscroll source is sent since the GSB gets sent on the first mouse move
  // in autoscroll mode rather than on middle click/mouse-down.
  virtual bool IsAutoscrollInProgress() = 0;

  // Called to toggle whether the RenderWidgetHost should capture all mouse
  // input.
  virtual void SetMouseCapture(bool capture) = 0;
  virtual void RequestMouseLock(
      bool from_user_gesture,
      bool unadjusted_movement,
      blink::mojom::WidgetInputHandlerHost::RequestMouseLockCallback
          response) = 0;

  // Returns the size of visible viewport in screen space, in DIPs.
  virtual gfx::Size GetRootWidgetViewportSize() = 0;

  // Called when an invalid input event source is sent from the renderer.
  virtual void OnInvalidInputEventSource() = 0;
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_CLIENT_H_
