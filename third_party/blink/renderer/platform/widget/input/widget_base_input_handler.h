// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_WIDGET_BASE_INPUT_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_WIDGET_BASE_INPUT_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-blink.h"
#include "third_party/blink/public/platform/input/input_handler_proxy.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_touch_action.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/types/scroll_types.h"

namespace cc {
struct ElementId;
struct OverscrollBehavior;
}  // namespace cc

namespace ui {
class LatencyInfo;
}

namespace viz {
class FrameSinkId;
}

namespace blink {

class WidgetBase;

class PLATFORM_EXPORT WidgetBaseInputHandler {
 public:
  WidgetBaseInputHandler(WidgetBase* widget);
  WidgetBaseInputHandler(const WidgetBaseInputHandler&) = delete;
  WidgetBaseInputHandler& operator=(const WidgetBaseInputHandler&) = delete;

  // Hit test the given point to find out the frame underneath and
  // returns the FrameSinkId for that frame. |local_point| returns the point
  // in the coordinate space of the FrameSinkId that was hit.
  viz::FrameSinkId GetFrameSinkIdAtPoint(const gfx::PointF& point,
                                         gfx::PointF* local_point);

  using HandledEventCallback = base::OnceCallback<void(
      mojom::InputEventResultState ack_state,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<InputHandlerProxy::DidOverscrollParams>,
      base::Optional<WebTouchAction>)>;

  // Handle input events from the input event provider.
  void HandleInputEvent(const blink::WebCoalescedInputEvent& coalesced_event,
                        HandledEventCallback callback);

  // Handle overscroll from Blink. Returns whether the should be sent to the
  // browser. This will return false if an event is currently being processed
  // and will be returned part of the input ack.
  bool DidOverscrollFromBlink(const gfx::Vector2dF& overscrollDelta,
                              const gfx::Vector2dF& accumulatedOverscroll,
                              const gfx::PointF& position,
                              const gfx::Vector2dF& velocity,
                              const cc::OverscrollBehavior& behavior);

  void InjectGestureScrollEvent(blink::WebGestureDevice device,
                                const gfx::Vector2dF& delta,
                                ui::ScrollGranularity granularity,
                                cc::ElementId scrollable_area_element_id,
                                blink::WebInputEvent::Type injected_type);

  bool handling_input_event() const { return handling_input_event_; }
  void set_handling_input_event(bool handling_input_event) {
    handling_input_event_ = handling_input_event;
  }

  // Process the touch action, returning whether the action should be relayed
  // to the browser.
  bool ProcessTouchAction(WebTouchAction touch_action);

  // Process the new cursor and returns true if it has changed from the last
  // cursor.
  bool DidChangeCursor(const ui::Cursor& cursor);

 private:
  class HandlingState;
  struct InjectScrollGestureParams {
    WebGestureDevice device;
    gfx::Vector2dF scroll_delta;
    ui::ScrollGranularity granularity;
    cc::ElementId scrollable_area_element_id;
    blink::WebInputEvent::Type type;
  };

  WebInputEventResult HandleTouchEvent(
      const WebCoalescedInputEvent& coalesced_event);

  void HandleInjectedScrollGestures(
      std::vector<InjectScrollGestureParams> injected_scroll_params,
      const WebInputEvent& input_event,
      const ui::LatencyInfo& original_latency_info);

  WidgetBase* widget_;

  // Are we currently handling an input event?
  bool handling_input_event_ = false;

  // Current state from HandleInputEvent. This variable is stack allocated
  // and is not owned.
  HandlingState* handling_input_state_ = nullptr;

  // We store the current cursor object so we can avoid spamming SetCursor
  // messages.
  base::Optional<ui::Cursor> current_cursor_;

  // Indicates if the next sequence of Char events should be suppressed or not.
  bool suppress_next_char_events_ = false;

  // Whether the last injected scroll gesture was a GestureScrollBegin. Used to
  // determine which GestureScrollUpdate is the first in a gesture sequence for
  // latency classification.
  bool last_injected_gesture_was_begin_ = false;

  const bool supports_buffered_touch_ = false;

  base::WeakPtrFactory<WidgetBaseInputHandler> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_WIDGET_BASE_INPUT_HANDLER_H_
