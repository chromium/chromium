// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_EXTERNAL_WIDGET_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_EXTERNAL_WIDGET_CLIENT_H_

#include <vector>

#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
struct VisualProperties;
class WebCoalescedInputEvent;
class WebGestureEvent;

// The interface from blink to Widgets with implementations outside of blink.
class WebExternalWidgetClient {
 public:
  virtual ~WebExternalWidgetClient() = default;

  // Called when the associated WebExternalWidget receives input and
  // needs the implementation to handle it.
  virtual WebInputEventResult HandleInputEvent(
      const WebCoalescedInputEvent&) = 0;

  // Called when the associated WebExternalWidget wishes to dispatch
  // any pending buffered touch events. The implementation may choose to buffer
  // individual pointer events (received via HandleInputEvent) and dispatch
  // a single touch event indicating the changes since the last touch event.
  // This method is typically invoked once per frame whereas HandleInputEvent
  // may be invoked many times per frame (i.e. multiple fingers on the touch
  // surface).
  virtual WebInputEventResult DispatchBufferedTouchEvents() = 0;

  // Called when the associated WebExternalWidget has adjusted its size.
  virtual void DidResize(const gfx::Size& size) = 0;

  // Record the time it took for the first paint after the widget transitioned
  // from background inactive to active.
  virtual void RecordTimeToFirstActivePaint(base::TimeDelta duration) {}

  using LayerTreeFrameSinkCallback = base::OnceCallback<void(
      std::unique_ptr<cc::LayerTreeFrameSink>,
      std::unique_ptr<cc::RenderFrameMetadataObserver>)>;

  // Requests a LayerTreeFrameSink to submit CompositorFrames to.
  virtual void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) = 0;

  // Notification that the BeginMainFrame completed, was committed into the
  // compositor (thread) and submitted to the display compositor.
  virtual void DidCommitAndDrawCompositorFrame() = 0;

  // Called before gesture events are processed and allows the
  // client to handle the event itself. Return true if event was handled
  // and further processing should stop.
  virtual bool WillHandleGestureEvent(const WebGestureEvent& event) {
    return false;
  }

  virtual bool SupportsBufferedTouchEvents() { return false; }

  // Returns whether we handled a GestureScrollEvent.
  virtual void DidHandleGestureScrollEvent(
      const WebGestureEvent& gesture_event,
      const gfx::Vector2dF& unused_delta,
      const cc::OverscrollBehavior& overscroll_behavior,
      bool event_processed) {}

  // The state of the focus has changed for the WebWidget. |enabled|
  // is the new state.
  virtual void FocusChanged(bool enabled) {}

  // Apply the visual properties to the widget.
  virtual void UpdateVisualProperties(
      const VisualProperties& visual_properties) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_EXTERNAL_WIDGET_CLIENT_H_
