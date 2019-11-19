// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_H_

#include "base/callback_forward.h"
#include "cc/input/touch_action.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/passthrough_touch_event_queue.h"
#include "content/common/widget.mojom.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/input_event_ack_state.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace content {

// The InputRouter allows the embedder to customize how input events are
// sent to the renderer, and how responses are dispatched to the browser.
// While the router should respect the relative order in which events are
// received, it is free to customize when those events are dispatched.
class InputRouter {
 public:
  struct CONTENT_EXPORT Config {
    Config();
    GestureEventQueue::Config gesture_config;
    PassthroughTouchEventQueue::Config touch_config;
  };

  virtual ~InputRouter() = default;

  // Note: if the event is processed immediately, the supplied callback is run
  // *synchronously*. If |this| is destroyed while waiting on a result from
  // the renderer, then callbacks are *not* run.
  using MouseEventCallback =
      base::OnceCallback<void(const MouseEventWithLatencyInfo& event,
                              InputEventAckSource ack_source,
                              InputEventAckState ack_result)>;
  virtual void SendMouseEvent(const MouseEventWithLatencyInfo& mouse_event,
                              MouseEventCallback event_result_callback) = 0;

  virtual void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) = 0;

  using KeyboardEventCallback = base::OnceCallback<void(
      const NativeWebKeyboardEventWithLatencyInfo& event,
      InputEventAckSource ack_source,
      InputEventAckState ack_result)>;
  virtual void SendKeyboardEvent(
      const NativeWebKeyboardEventWithLatencyInfo& key_event,
      KeyboardEventCallback event_result_callback) = 0;

  virtual void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) = 0;

  virtual void SendTouchEvent(
      const TouchEventWithLatencyInfo& touch_event) = 0;

  // Notify the router about whether the current page is mobile-optimized (i.e.,
  // the site has a mobile-friendly viewport).
  virtual void NotifySiteIsMobileOptimized(bool is_mobile_optimized) = 0;

  // Whether there are any events pending dispatch to or ack from the renderer.
  virtual bool HasPendingEvents() const = 0;

  // A scale factor to scale the coordinate in WebInputEvent from DIP
  // to viewport.
  virtual void SetDeviceScaleFactor(float device_scale_factor) = 0;

  // Sets the frame tree node id of associated frame, used when tracing
  // input event latencies to relate events to their target frames. Since
  // input always flows to Local Frame Roots, the |frameTreeNodeId| is
  // relative to the Frame associated with the Local Frame Root for the
  // widget owning this InputRouter.
  virtual void SetFrameTreeNodeId(int frameTreeNodeId) = 0;

  // Return the currently allowed touch-action.
  virtual base::Optional<cc::TouchAction> AllowedTouchAction() = 0;

  // Return the currently active touch-action.
  virtual base::Optional<cc::TouchAction> ActiveTouchAction() = 0;

  virtual void SetForceEnableZoom(bool enabled) = 0;

  // Create and bind a new host channel.
  virtual mojo::PendingRemote<mojom::WidgetInputHandlerHost> BindNewHost() = 0;

  // Create and bind a new frame based host channel.
  virtual mojo::PendingRemote<mojom::WidgetInputHandlerHost>
  BindNewFrameHost() = 0;

  // Used to stop an active fling if such exists.
  virtual void StopFling() = 0;

  // Called when a set-touch-action message is received from the renderer
  // for a touch start event that is currently in flight.
  virtual void OnSetTouchAction(cc::TouchAction touch_action) = 0;

  // In the case when a gesture event is bubbled from a child frame to the main
  // frame, we set the touch action in the main frame Auto even if there is no
  // pending touch start.
  virtual void ForceSetTouchActionAuto() = 0;

  // Called when the renderer notifies a change in whether or not it has touch
  // event handlers registered.
  virtual void OnHasTouchEventHandlers(bool has_handlers) = 0;

  // Will resolve the given callback once all prior input has been fully
  // propagated through the system such that subsequent input will be subject
  // to its effects. e.g. Input that follows a scroll gesture that affects
  // OOPIF hit-testing will need to wait until updated CompositorFrames have
  // been submitted to the browser.
  virtual void WaitForInputProcessed(base::OnceClosure callback) = 0;

  // Acks any pending touch events that are waiting for acks from the renderer.
  // Any future acks for those events from the renderer will be ignored.
  virtual void FlushTouchEventQueue() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_H_
