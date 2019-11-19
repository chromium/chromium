// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOUSE_WHEEL_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOUSE_WHEEL_EVENT_QUEUE_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/public/common/input_event_ack_source.h"
#include "content/public/common/input_event_ack_state.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace content {

// This class represents a single queued mouse wheel event. Its main use
// is that it is reported via trace events.
class QueuedWebMouseWheelEvent : public MouseWheelEventWithLatencyInfo {
 public:
  QueuedWebMouseWheelEvent(const MouseWheelEventWithLatencyInfo& original_event)
      : MouseWheelEventWithLatencyInfo(original_event) {
    TRACE_EVENT_ASYNC_BEGIN0("input", "MouseWheelEventQueue::QueueEvent", this);
  }

  ~QueuedWebMouseWheelEvent() {
    TRACE_EVENT_ASYNC_END0("input", "MouseWheelEventQueue::QueueEvent", this);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QueuedWebMouseWheelEvent);
};

// Interface with which MouseWheelEventQueue can forward mouse wheel events,
// and dispatch mouse wheel event responses.
class CONTENT_EXPORT MouseWheelEventQueueClient {
 public:
  virtual ~MouseWheelEventQueueClient() {}

  virtual void SendMouseWheelEventImmediately(
      const MouseWheelEventWithLatencyInfo& event) = 0;
  virtual void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& event,
      const ui::LatencyInfo& latency_info) = 0;
  virtual void OnMouseWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                                    InputEventAckSource ack_source,
                                    InputEventAckState ack_result) = 0;
  virtual bool IsWheelScrollInProgress() = 0;
  virtual bool IsAutoscrollInProgress() = 0;
};

// A queue for throttling and coalescing mouse wheel events.
class CONTENT_EXPORT MouseWheelEventQueue {
 public:
  // The |client| must outlive the MouseWheelEventQueue.
  // |IsWheelScrollInProgress| indicates whether mouse wheel events should
  // generate Scroll[Begin|Update|End] on unhandled acknowledge events.
  MouseWheelEventQueue(MouseWheelEventQueueClient* client);

  ~MouseWheelEventQueue();

  // Adds an event to the queue. The event may be coalesced with previously
  // queued events (e.g. consecutive mouse-wheel events can be coalesced into a
  // single mouse-wheel event). The event may also be immediately forwarded to
  // the renderer (e.g. when there are no other queued mouse-wheel event).
  void QueueEvent(const MouseWheelEventWithLatencyInfo& event);

  // Notifies the queue that a mouse wheel event has been processed by the
  // renderer.
  void ProcessMouseWheelAck(InputEventAckSource ack_source,
                            InputEventAckState ack_result,
                            const MouseWheelEventWithLatencyInfo& ack_event);

  // When GestureScrollBegin is received, and it is a different source
  // than mouse wheels terminate the current GestureScroll if there is one.
  // When Gesture{ScrollEnd,FlingStart} is received, resume generating
  // gestures.
  void OnGestureScrollEvent(const GestureEventWithLatencyInfo& gesture_event);

  bool has_pending() const WARN_UNUSED_RESULT {
    return !wheel_queue_.empty() || event_sent_for_gesture_ack_;
  }

  size_t queued_size() const { return wheel_queue_.size(); }
  bool event_in_flight() const {
    return event_sent_for_gesture_ack_ != nullptr;
  }

  blink::WebMouseWheelEvent get_wheel_event_awaiting_ack_for_testing() {
    return event_sent_for_gesture_ack_->event;
  }

 private:
  void TryForwardNextEventToRenderer();
  void SendScrollEnd(blink::WebGestureEvent update_event, bool synthetic);
  void SendScrollBegin(const blink::WebGestureEvent& gesture_update,
                       bool synthetic);
  void RecordLatchingUmaMetric(bool latched);

  // True if gesture scroll events can be generated for the wheel event sent for
  // ack.
  bool CanGenerateGestureScroll(InputEventAckState ack_result) const;

  MouseWheelEventQueueClient* client_;

  base::circular_deque<std::unique_ptr<QueuedWebMouseWheelEvent>> wheel_queue_;
  std::unique_ptr<QueuedWebMouseWheelEvent> event_sent_for_gesture_ack_;

  // True if the ack for the first wheel event in a scroll sequence is not
  // consumed. This lets us to send the rest of the wheel events in the sequence
  // as non-blocking.
  bool send_wheel_events_async_;

  blink::WebGestureDevice scrolling_device_;

  DISALLOW_COPY_AND_ASSIGN(MouseWheelEventQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOUSE_WHEEL_EVENT_QUEUE_H_
