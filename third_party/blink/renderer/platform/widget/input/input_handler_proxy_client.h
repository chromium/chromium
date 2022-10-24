// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_INPUT_HANDLER_PROXY_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_INPUT_HANDLER_PROXY_CLIENT_H_

#include <memory>

namespace cc {
class EventMetrics;
}

namespace blink {
class WebCoalescedInputEvent;
class WebGestureEvent;
class WebInputEventAttribution;

// All callbacks invoked from the compositor thread.
class InputHandlerProxyClient {
 public:
  // Called just before the InputHandlerProxy shuts down.
  virtual void WillShutdown() = 0;

  // Dispatch a non blocking event to the main thread. This is used when a
  // gesture fling from a touchpad is processed and the target only has
  // passive event listeners. `metrics` contains information about the event
  // which can be used in reporting latency metrics.
  virtual void DispatchNonBlockingEventToMainThread(
      std::unique_ptr<WebCoalescedInputEvent> event,
      const blink::WebInputEventAttribution& attribution,
      std::unique_ptr<cc::EventMetrics> metrics) = 0;

  virtual void DidAnimateForInput() = 0;

  virtual void DidStartScrollingViewport() = 0;

  // Used to send a GSB to the main thread when the scrolling should switch to
  // the main thread. `update_metrics` contains information about the original
  // `update_event` which can be used to create metrics information for the
  // generated GSB event.
  virtual void GenerateScrollBeginAndSendToMainThread(
      const blink::WebGestureEvent& update_event,
      const blink::WebInputEventAttribution& attribution,
      const cc::EventMetrics* update_metrics) = 0;

  virtual void SetAllowedTouchAction(cc::TouchAction touch_action) = 0;

  virtual bool AllowsScrollResampling() = 0;

 protected:
  virtual ~InputHandlerProxyClient() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_INPUT_HANDLER_PROXY_CLIENT_H_
