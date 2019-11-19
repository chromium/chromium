// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_PINCH_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_PINCH_EVENT_QUEUE_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/common/input/event_with_latency_info.h"
#include "content/public/common/input_event_ack_source.h"
#include "content/public/common/input_event_ack_state.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace content {

class QueuedTouchpadPinchEvent;

// Interface with which TouchpadPinchEventQueue can forward synthetic mouse
// wheel events and notify of pinch events.
class CONTENT_EXPORT TouchpadPinchEventQueueClient {
 public:
  virtual ~TouchpadPinchEventQueueClient() = default;

  virtual void SendMouseWheelEventForPinchImmediately(
      const MouseWheelEventWithLatencyInfo& event) = 0;
  virtual void OnGestureEventForPinchAck(
      const GestureEventWithLatencyInfo& event,
      InputEventAckSource ack_source,
      InputEventAckState ack_result) = 0;
};

// A queue for sending synthetic mouse wheel events for touchpad pinches.
// In order for a page to prevent touchpad pinch zooming, we send synthetic
// wheel events which may be cancelled. Once we've determined whether a page
// has prevented the pinch, the TouchpadPinchEventQueueClient may proceed with
// handling the pinch.
// See README.md for further details.
class CONTENT_EXPORT TouchpadPinchEventQueue {
 public:
  // The |client| must outlive the TouchpadPinchEventQueue.
  TouchpadPinchEventQueue(TouchpadPinchEventQueueClient* client);
  ~TouchpadPinchEventQueue();

  // Adds the given touchpad pinch |event| to the queue. The event may be
  // coalesced with previously queued events.
  void QueueEvent(const GestureEventWithLatencyInfo& event);

  // Notifies the queue that a synthetic mouse wheel event has been processed
  // by the renderer.
  void ProcessMouseWheelAck(InputEventAckSource ack_source,
                            InputEventAckState ack_result,
                            const MouseWheelEventWithLatencyInfo& ack_event);

  bool has_pending() const WARN_UNUSED_RESULT;

  blink::WebMouseWheelEvent get_wheel_event_awaiting_ack_for_testing() {
    return wheel_event_awaiting_ack_.value();
  }

 private:
  void TryForwardNextEventToRenderer();

  const bool touchpad_async_pinch_events_;
  TouchpadPinchEventQueueClient* client_;

  base::circular_deque<std::unique_ptr<QueuedTouchpadPinchEvent>> pinch_queue_;
  std::unique_ptr<QueuedTouchpadPinchEvent> pinch_event_awaiting_ack_;
  base::Optional<blink::WebMouseWheelEvent> wheel_event_awaiting_ack_;
  base::Optional<bool> first_event_prevented_;

  DISALLOW_COPY_AND_ASSIGN(TouchpadPinchEventQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_PINCH_EVENT_QUEUE_H_
