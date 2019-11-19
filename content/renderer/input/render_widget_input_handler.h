// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_H_
#define CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/input/input_event_ack.h"
#include "content/common/input/input_event_dispatch_type.h"
#include "content/renderer/input/main_thread_event_queue.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/types/scroll_types.h"

namespace blink {
struct WebFloatPoint;
struct WebFloatSize;
}  // namespace blink

namespace cc {
struct ElementId;
struct OverscrollBehavior;
}

namespace ui {
class LatencyInfo;
}

namespace viz {
class FrameSinkId;
}

namespace content {

class RenderWidget;
class RenderWidgetInputHandlerDelegate;

// RenderWidgetInputHandler is an IPC-agnostic input handling class.
// IPC transport code should live in RenderWidget or RenderWidgetMusConnection.
class CONTENT_EXPORT RenderWidgetInputHandler {
 public:
  RenderWidgetInputHandler(RenderWidgetInputHandlerDelegate* delegate,
                           RenderWidget* widget);
  virtual ~RenderWidgetInputHandler();

  // Hit test the given point to find out the frame underneath and
  // returns the FrameSinkId for that frame. |local_point| returns the point
  // in the coordinate space of the FrameSinkId that was hit.
  viz::FrameSinkId GetFrameSinkIdAtPoint(const gfx::PointF& point,
                                         gfx::PointF* local_point);

  // Handle input events from the input event provider.
  virtual void HandleInputEvent(
      const blink::WebCoalescedInputEvent& coalesced_event,
      const ui::LatencyInfo& latency_info,
      HandledEventCallback callback);

  // Handle overscroll from Blink.
  void DidOverscrollFromBlink(const blink::WebFloatSize& overscrollDelta,
                              const blink::WebFloatSize& accumulatedOverscroll,
                              const blink::WebFloatPoint& position,
                              const blink::WebFloatSize& velocity,
                              const cc::OverscrollBehavior& behavior);

  void InjectGestureScrollEvent(blink::WebGestureDevice device,
                                const blink::WebFloatSize& delta,
                                ui::input_types::ScrollGranularity granularity,
                                cc::ElementId scrollable_area_element_id,
                                blink::WebInputEvent::Type injected_type);

  bool handling_input_event() const { return handling_input_event_; }
  void set_handling_input_event(bool handling_input_event) {
    handling_input_event_ = handling_input_event;
  }

  // Process the touch action, returning whether the action should be relayed
  // to the browser.
  bool ProcessTouchAction(cc::TouchAction touch_action);

  // Process the new cursor and returns true if it has changed from the last
  // cursor.
  bool DidChangeCursor(const WebCursor& cursor);

 private:
  struct InjectScrollGestureParams {
    blink::WebGestureDevice device;
    gfx::Vector2dF scroll_delta;
    ui::input_types::ScrollGranularity granularity;
    cc::ElementId scrollable_area_element_id;
    blink::WebInputEvent::Type type;
  };

  blink::WebInputEventResult HandleTouchEvent(
      const blink::WebCoalescedInputEvent& coalesced_event);

  void HandleInjectedScrollGestures(
      std::vector<InjectScrollGestureParams> injected_scroll_params,
      const WebInputEvent& input_event,
      const ui::LatencyInfo& original_latency_info);

  RenderWidgetInputHandlerDelegate* const delegate_;

  RenderWidget* const widget_;

  // Are we currently handling an input event?
  bool handling_input_event_;

  // We store the current cursor object so we can avoid spamming SetCursor
  // messages.
  base::Optional<WebCursor> current_cursor_;

  // Used to intercept overscroll notifications while an event is being
  // handled. If the event causes overscroll, the overscroll metadata can be
  // bundled in the event ack, saving an IPC.  Note that we must continue
  // supporting overscroll IPC notifications due to fling animation updates.
  std::unique_ptr<ui::DidOverscrollParams>* handling_event_overscroll_;

  base::Optional<cc::TouchAction> handling_touch_action_;

  // Used to hold a sequence of parameters corresponding to scroll gesture
  // events that should be injected once the current input event is done
  // being processed.
  std::unique_ptr<std::vector<InjectScrollGestureParams>>*
      handling_injected_scroll_params_;

  // Type of the input event we are currently handling.
  blink::WebInputEvent::Type handling_event_type_;

  // Indicates if the next sequence of Char events should be suppressed or not.
  bool suppress_next_char_events_;

  // Whether the last injected scroll gesture was a GestureScrollBegin. Used to
  // determine which GestureScrollUpdate is the first in a gesture sequence for
  // latency classification.
  bool last_injected_gesture_was_begin_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetInputHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_H_
