// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touchpad_pinch_event_queue.h"

#include "base/trace_event/trace_event.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/latency/latency_info.h"

namespace content {

namespace {

blink::WebMouseWheelEvent CreateSyntheticWheelFromTouchpadPinchEvent(
    const blink::WebGestureEvent& pinch_event,
    blink::WebMouseWheelEvent::Phase phase,
    bool cancelable) {
  DCHECK(pinch_event.GetType() == blink::WebInputEvent::kGesturePinchUpdate ||
         pinch_event.GetType() == blink::WebInputEvent::kGesturePinchEnd ||
         pinch_event.GetType() == blink::WebInputEvent::kGestureDoubleTap);
  float delta_y = 0.0f;
  float wheel_ticks_y = 0.0f;

  // The function to convert scales to deltaY values is designed to be
  // compatible with websites existing use of wheel events, and with existing
  // Windows trackpad behavior.  In particular, we want:
  //  - deltas should accumulate via addition: f(s1*s2)==f(s1)+f(s2)
  //  - deltas should invert via negation: f(1/s) == -f(s)
  //  - zoom in should be positive: f(s) > 0 iff s > 1
  //  - magnitude roughly matches wheels: f(2) > 25 && f(2) < 100
  //  - a formula that's relatively easy to use from JavaScript
  // Note that 'wheel' event deltaY values have their sign inverted.  So to
  // convert a wheel deltaY back to a scale use Math.exp(-deltaY/100).
  if (pinch_event.GetType() == blink::WebInputEvent::kGesturePinchUpdate) {
    DCHECK_GT(pinch_event.data.pinch_update.scale, 0);
    delta_y = 100.0f * log(pinch_event.data.pinch_update.scale);
    wheel_ticks_y = pinch_event.data.pinch_update.scale > 1 ? 1 : -1;
  }

  // For pinch gesture events, similar to ctrl+wheel zooming, allow content to
  // prevent the browser from zooming by sending fake wheel events with the ctrl
  // modifier set when we see trackpad pinch gestures.  Ideally we'd someday get
  // a platform 'pinch' event and send that instead.
  blink::WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::kMouseWheel,
      pinch_event.GetModifiers() | blink::WebInputEvent::kControlKey,
      pinch_event.TimeStamp());
  wheel_event.SetPositionInWidget(pinch_event.PositionInWidget());
  wheel_event.SetPositionInScreen(pinch_event.PositionInScreen());
  wheel_event.delta_units =
      ui::input_types::ScrollGranularity::kScrollByPrecisePixel;
  wheel_event.delta_x = 0;
  wheel_event.delta_y = delta_y;

  wheel_event.phase = phase;
  wheel_event.wheel_ticks_x = 0;
  wheel_event.wheel_ticks_y = wheel_ticks_y;

  if (cancelable)
    wheel_event.dispatch_type = blink::WebInputEvent::kBlocking;
  else
    wheel_event.dispatch_type = blink::WebInputEvent::kEventNonBlocking;

  return wheel_event;
}

}  // namespace

// This is a single queued pinch event to which we add trace events.
class QueuedTouchpadPinchEvent : public GestureEventWithLatencyInfo {
 public:
  QueuedTouchpadPinchEvent(const GestureEventWithLatencyInfo& original_event)
      : GestureEventWithLatencyInfo(original_event) {
    TRACE_EVENT_ASYNC_BEGIN0("input", "TouchpadPinchEventQueue::QueueEvent",
                             this);
  }

  ~QueuedTouchpadPinchEvent() {
    TRACE_EVENT_ASYNC_END0("input", "TouchpadPinchEventQueue::QueueEvent",
                           this);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QueuedTouchpadPinchEvent);
};

TouchpadPinchEventQueue::TouchpadPinchEventQueue(
    TouchpadPinchEventQueueClient* client)
    : touchpad_async_pinch_events_(
          base::FeatureList::IsEnabled(features::kTouchpadAsyncPinchEvents)),
      client_(client) {
  DCHECK(client_);
}

TouchpadPinchEventQueue::~TouchpadPinchEventQueue() = default;

void TouchpadPinchEventQueue::QueueEvent(
    const GestureEventWithLatencyInfo& event) {
  TRACE_EVENT0("input", "TouchpadPinchEventQueue::QueueEvent");

  if (!pinch_queue_.empty()) {
    QueuedTouchpadPinchEvent* last_event = pinch_queue_.back().get();
    if (last_event->CanCoalesceWith(event)) {
      // Terminate the LatencyInfo of the event before it gets coalesced away.
      event.latency.Terminate();

      last_event->CoalesceWith(event);
      DCHECK_EQ(blink::WebInputEvent::kGesturePinchUpdate,
                last_event->event.GetType());
      TRACE_EVENT_INSTANT1("input",
                           "TouchpadPinchEventQueue::CoalescedPinchEvent",
                           TRACE_EVENT_SCOPE_THREAD, "scale",
                           last_event->event.data.pinch_update.scale);
      return;
    }
  }

  pinch_queue_.push_back(std::make_unique<QueuedTouchpadPinchEvent>(event));
  TryForwardNextEventToRenderer();
}

void TouchpadPinchEventQueue::ProcessMouseWheelAck(
    InputEventAckSource ack_source,
    InputEventAckState ack_result,
    const MouseWheelEventWithLatencyInfo& ack_event) {
  TRACE_EVENT0("input", "TouchpadPinchEventQueue::ProcessMouseWheelAck");
  if (!pinch_event_awaiting_ack_)
    return;

  // |ack_event.event| should be the same as the wheel_event_awaiting_ack_. If
  // they aren't, then don't continue processing the ack. The two events can
  // potentially be different because MouseWheelEventQueue also dispatches wheel
  // events, and any wheel event ack that is received is sent to both
  // *EventQueue::ProcessMouseWheelAck methods.
  if (wheel_event_awaiting_ack_ != ack_event.event)
    return;

  if (pinch_event_awaiting_ack_->event.GetType() ==
          blink::WebInputEvent::kGesturePinchUpdate &&
      !first_event_prevented_.has_value())
    first_event_prevented_ = (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED);

  pinch_event_awaiting_ack_->latency.AddNewLatencyFrom(ack_event.latency);
  client_->OnGestureEventForPinchAck(*pinch_event_awaiting_ack_, ack_source,
                                     ack_result);

  pinch_event_awaiting_ack_.reset();
  wheel_event_awaiting_ack_.reset();
  TryForwardNextEventToRenderer();
}

void TouchpadPinchEventQueue::TryForwardNextEventToRenderer() {
  TRACE_EVENT0("input",
               "TouchpadPinchEventQueue::TryForwardNextEventToRenderer");

  if (pinch_queue_.empty() || pinch_event_awaiting_ack_)
    return;

  pinch_event_awaiting_ack_ = std::move(pinch_queue_.front());
  pinch_queue_.pop_front();

  if (pinch_event_awaiting_ack_->event.GetType() ==
      blink::WebInputEvent::kGesturePinchBegin) {
    client_->OnGestureEventForPinchAck(*pinch_event_awaiting_ack_,
                                       InputEventAckSource::BROWSER,
                                       INPUT_EVENT_ACK_STATE_IGNORED);
    pinch_event_awaiting_ack_.reset();
    TryForwardNextEventToRenderer();
    return;
  }

  blink::WebMouseWheelEvent::Phase phase =
      blink::WebMouseWheelEvent::kPhaseNone;
  bool cancelable = true;

  if (pinch_event_awaiting_ack_->event.GetType() ==
      blink::WebInputEvent::kGesturePinchEnd) {
    first_event_prevented_.reset();
    phase = blink::WebMouseWheelEvent::kPhaseEnded;
    cancelable = false;
  } else if (pinch_event_awaiting_ack_->event.GetType() ==
             blink::WebInputEvent::kGestureDoubleTap) {
    phase = blink::WebMouseWheelEvent::kPhaseNone;
    cancelable = true;
  } else {
    DCHECK_EQ(pinch_event_awaiting_ack_->event.GetType(),
              blink::WebInputEvent::kGesturePinchUpdate);
    // The first pinch update event should send a synthetic wheel with phase
    // began.
    if (!first_event_prevented_.has_value()) {
      phase = blink::WebMouseWheelEvent::kPhaseBegan;
      cancelable = true;
    } else {
      phase = blink::WebMouseWheelEvent::kPhaseChanged;
      cancelable =
          !touchpad_async_pinch_events_ || first_event_prevented_.value();
    }
  }

  wheel_event_awaiting_ack_ = CreateSyntheticWheelFromTouchpadPinchEvent(
      pinch_event_awaiting_ack_->event, phase, cancelable);
  const MouseWheelEventWithLatencyInfo synthetic_wheel(
      wheel_event_awaiting_ack_.value(), pinch_event_awaiting_ack_->latency);

  client_->SendMouseWheelEventForPinchImmediately(synthetic_wheel);
}

bool TouchpadPinchEventQueue::has_pending() const {
  return !pinch_queue_.empty() || pinch_event_awaiting_ack_;
}

}  // namespace content
