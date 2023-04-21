// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/render_widget_host_latency_tracker.h"

#include <stddef.h>
#include <string>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ui/events/blink/web_input_event_traits.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using ui::LatencyInfo;

namespace content {
namespace {
const char* GetTraceNameFromType(blink::WebInputEvent::Type type) {
#define CASE_TYPE(t)              \
  case WebInputEvent::Type::k##t: \
    return "InputLatency::" #t
  switch (type) {
    CASE_TYPE(Undefined);
    CASE_TYPE(MouseDown);
    CASE_TYPE(MouseUp);
    CASE_TYPE(MouseMove);
    CASE_TYPE(MouseEnter);
    CASE_TYPE(MouseLeave);
    CASE_TYPE(ContextMenu);
    CASE_TYPE(MouseWheel);
    CASE_TYPE(RawKeyDown);
    CASE_TYPE(KeyDown);
    CASE_TYPE(KeyUp);
    CASE_TYPE(Char);
    CASE_TYPE(GestureScrollBegin);
    CASE_TYPE(GestureScrollEnd);
    CASE_TYPE(GestureScrollUpdate);
    CASE_TYPE(GestureFlingStart);
    CASE_TYPE(GestureFlingCancel);
    CASE_TYPE(GestureShowPress);
    CASE_TYPE(GestureTap);
    CASE_TYPE(GestureTapUnconfirmed);
    CASE_TYPE(GestureTapDown);
    CASE_TYPE(GestureTapCancel);
    CASE_TYPE(GestureDoubleTap);
    CASE_TYPE(GestureTwoFingerTap);
    CASE_TYPE(GestureShortPress);
    CASE_TYPE(GestureLongPress);
    CASE_TYPE(GestureLongTap);
    CASE_TYPE(GesturePinchBegin);
    CASE_TYPE(GesturePinchEnd);
    CASE_TYPE(GesturePinchUpdate);
    CASE_TYPE(TouchStart);
    CASE_TYPE(TouchMove);
    CASE_TYPE(TouchEnd);
    CASE_TYPE(TouchCancel);
    CASE_TYPE(TouchScrollStarted);
    CASE_TYPE(PointerDown);
    CASE_TYPE(PointerUp);
    CASE_TYPE(PointerMove);
    CASE_TYPE(PointerRawUpdate);
    CASE_TYPE(PointerCancel);
    CASE_TYPE(PointerCausedUaAction);
  }
#undef CASE_TYPE
  NOTREACHED();
  return "";
}
}  // namespace

RenderWidgetHostLatencyTracker::RenderWidgetHostLatencyTracker(
    RenderWidgetHostDelegate* delegate)
    : has_seen_first_gesture_scroll_update_(false),
      gesture_scroll_id_(-1),
      touch_trace_id_(-1),
      active_multi_finger_gesture_(false),
      touch_start_default_prevented_(false),
      render_widget_host_delegate_(delegate) {}

RenderWidgetHostLatencyTracker::~RenderWidgetHostLatencyTracker() {}

void RenderWidgetHostLatencyTracker::OnInputEvent(
    const blink::WebInputEvent& event,
    LatencyInfo* latency,
    ui::EventLatencyMetadata* event_latency_metadata) {
  DCHECK(latency);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  OnEventStart(latency);

  if (event.GetType() == WebInputEvent::Type::kTouchStart) {
    const WebTouchEvent& touch_event =
        *static_cast<const WebTouchEvent*>(&event);
    DCHECK_GE(touch_event.touches_length, static_cast<unsigned>(1));
    active_multi_finger_gesture_ = touch_event.touches_length != 1;
  }

  if (latency->source_event_type() == ui::SourceEventType::KEY_PRESS) {
    DCHECK(event.GetType() == WebInputEvent::Type::kChar ||
           event.GetType() == WebInputEvent::Type::kRawKeyDown);
  }

  // This is the only place to add the BEGIN_RWH component. So this component
  // should not already be present in the latency info.
  bool found_component = latency->FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr);
  DCHECK(!found_component);

  if (!event.TimeStamp().is_null() &&
      !latency->FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                            nullptr)) {
    base::TimeTicks timestamp_now = base::TimeTicks::Now();
    base::TimeTicks timestamp_original = event.TimeStamp();

    // Timestamp from platform input can wrap, e.g. 32 bits timestamp
    // for Xserver and Window MSG time will wrap about 49.6 days. Do a
    // sanity check here and if wrap does happen, use TimeTicks::Now()
    // as the timestamp instead.
    if ((timestamp_now - timestamp_original).InDays() > 0)
      timestamp_original = timestamp_now;

    latency->AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, timestamp_original);
  }

  base::TimeTicks begin_rwh_timestamp = base::TimeTicks::Now();
  latency->AddLatencyNumberWithTraceName(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      GetTraceNameFromType(event.GetType()), begin_rwh_timestamp);
  event_latency_metadata->arrived_in_browser_main_timestamp =
      begin_rwh_timestamp;

  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
    has_seen_first_gesture_scroll_update_ = false;
    gesture_scroll_id_ = latency->trace_id();
    latency->set_gesture_scroll_id(gesture_scroll_id_);
  } else if (event.GetType() ==
             blink::WebInputEvent::Type::kGestureScrollUpdate) {
    // Make a copy of the INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT with a
    // different name INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT.
    // So we can track the latency specifically for scroll update events.
    base::TimeTicks original_event_timestamp;
    if (latency->FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                             &original_event_timestamp)) {
      latency->AddLatencyNumberWithTimestamp(
          has_seen_first_gesture_scroll_update_
              ? ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT
              : ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          original_event_timestamp);
    }

    has_seen_first_gesture_scroll_update_ = true;
    latency->set_gesture_scroll_id(gesture_scroll_id_);
  } else if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd) {
    latency->set_gesture_scroll_id(gesture_scroll_id_);
    gesture_scroll_id_ = -1;
  } else if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
    // Store the trace id for the TouchStart event on all other Touch events
    // until the corresponding end or cancel event so they can be grouped
    // together.
    if (event.GetType() == blink::WebInputEvent::Type::kTouchStart)
      touch_trace_id_ = latency->trace_id();
    latency->set_touch_trace_id(touch_trace_id_);
    if (event.GetType() == blink::WebInputEvent::Type::kTouchEnd ||
        event.GetType() == blink::WebInputEvent::Type::kTouchCancel)
      touch_trace_id_ = -1;
  }
}

void RenderWidgetHostLatencyTracker::OnInputEventAck(
    const blink::WebInputEvent& event,
    LatencyInfo* latency,
    blink::mojom::InputEventResultState ack_result) {
  DCHECK(latency);

  // Latency ends if an event is acked but does not cause render scheduling.
  bool rendering_scheduled = latency->FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT, nullptr);
  rendering_scheduled |= latency->FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT, nullptr);

  if (WebInputEvent::IsTouchEventType(event.GetType())) {
    const WebTouchEvent& touch_event =
        *static_cast<const WebTouchEvent*>(&event);
    if (event.GetType() == WebInputEvent::Type::kTouchStart) {
      touch_start_default_prevented_ =
          ack_result == blink::mojom::InputEventResultState::kConsumed;
    } else if (event.GetType() == WebInputEvent::Type::kTouchEnd ||
               event.GetType() == WebInputEvent::Type::kTouchCancel) {
      active_multi_finger_gesture_ = touch_event.touches_length > 2;
    }
  }

  // If this event couldn't have caused a gesture event, and it didn't trigger
  // rendering, we're done processing it. If the event got coalesced then
  // terminate it as well. We also exclude cases where we're against the scroll
  // extent from scrolling metrics.
  if (!rendering_scheduled || latency->coalesced() ||
      (event.GetType() == WebInputEvent::Type::kGestureScrollUpdate &&
       ack_result == blink::mojom::InputEventResultState::kNoConsumerExists)) {
    latency->Terminate();
  }
}

void RenderWidgetHostLatencyTracker::OnEventStart(ui::LatencyInfo* latency) {
  static uint64_t global_trace_id = 0;
  latency->set_trace_id(++global_trace_id);
  latency->set_ukm_source_id(
      render_widget_host_delegate_->GetCurrentPageUkmSourceId());
}

}  // namespace content
