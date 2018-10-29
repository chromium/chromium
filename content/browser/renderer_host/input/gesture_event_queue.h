// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_EVENT_QUEUE_H_

#include <stddef.h>

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/fling_controller.h"
#include "content/common/content_export.h"
#include "content/public/common/input_event_ack_source.h"
#include "content/public/common/input_event_ack_state.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace content {
class GestureEventQueueTest;
class MockRenderWidgetHost;

// Interface with which the GestureEventQueue can forward gesture events, and
// dispatch gesture event responses.
class CONTENT_EXPORT GestureEventQueueClient {
 public:
  virtual ~GestureEventQueueClient() {}

  virtual void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& event) = 0;

  virtual void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                                 InputEventAckSource ack_source,
                                 InputEventAckState ack_result) = 0;
};

// Maintains WebGestureEvents in a queue before forwarding them to the renderer
// to apply a sequence of filters on them:
// 1. The sequence is filtered for bounces. A bounce is when the finger lifts
//    from the screen briefly during an in-progress scroll. Ifco this happens,
//    non-GestureScrollUpdate events are queued until the de-bounce interval
//    passes or another GestureScrollUpdate event occurs.
// 2. Unnecessary GestureFlingCancel events are filtered by fling controller.
//    These are GestureFlingCancels that have no corresponding GestureFlingStart
//    in the queue.
// 3. Taps immediately after a GestureFlingCancel (caused by the same tap) are
//    filtered by fling controller.
// 4. Whenever possible, events in the queue are coalesced to have as few events
//    as possible and therefore maximize the chance that the event stream can be
//    handled entirely by the compositor thread.
// Events in the queue are forwarded to the renderer one by one; i.e., each
// event is sent after receiving the ACK for previous one. The only exception is
// that if a GestureScrollUpdate is followed by a GesturePinchUpdate, they are
// sent together.
// TODO(rjkroege): Possibly refactor into a filter chain:
// http://crbug.com/148443.
class CONTENT_EXPORT GestureEventQueue {
 public:
  struct CONTENT_EXPORT Config {
    Config();

    FlingController::Config fling_config;

    // Determines whether non-scroll gesture events are "debounced" during an
    // active scroll sequence, suppressing brief scroll interruptions.
    // Zero by default (disabled).
    base::TimeDelta debounce_interval;
  };

  // Both |client| and |touchpad_client| must outlive the GestureEventQueue.
  GestureEventQueue(GestureEventQueueClient* client,
                    FlingControllerEventSenderClient* fling_event_sender_client,
                    FlingControllerSchedulerClient* fling_scheduler_client,
                    const Config& config);
  ~GestureEventQueue();

  // Uses fling controller to filter the gesture event. Returns true if the
  // event wasn't queued and was filtered.
  bool FlingControllerFilterEvent(const GestureEventWithLatencyInfo&);

  // Check for debouncing, or add the gesture event to the queue. Returns false
  // if the event wasn't queued.
  bool DebounceOrQueueEvent(const GestureEventWithLatencyInfo&);

  // Indicates that the caller has received an acknowledgement from the renderer
  // with state |ack_result| and event |type|. May send events if the queue is
  // not empty.
  void ProcessGestureAck(InputEventAckSource ack_source,
                         InputEventAckState ack_result,
                         blink::WebInputEvent::Type type,
                         const ui::LatencyInfo& latency);

  // Returns the |TouchpadTapSuppressionController| instance.
  TouchpadTapSuppressionController* GetTouchpadTapSuppressionController();

  void ForwardGestureEvent(const GestureEventWithLatencyInfo& gesture_event);

  bool empty() const {
    return coalesced_gesture_events_.empty() &&
           debouncing_deferral_queue_.empty();
  }

  // Calls |fling_controller_.StopFling| to halt an active fling if such exists.
  void StopFling();

  bool FlingCancellationIsDeferred() const;

  gfx::Vector2dF CurrentFlingVelocity() const;

  void set_debounce_interval_time_ms_for_testing(int interval_ms) {
    debounce_interval_ = base::TimeDelta::FromMilliseconds(interval_ms);
  }

 private:
  friend class GestureEventQueueTest;
  friend class MockRenderWidgetHost;

  class GestureEventWithLatencyInfoAndAckState
      : public GestureEventWithLatencyInfo {
   public:
    GestureEventWithLatencyInfoAndAckState(const GestureEventWithLatencyInfo&);
    InputEventAckState ack_state() const { return ack_state_; }
    void set_ack_info(InputEventAckSource source, InputEventAckState state) {
      ack_source_ = source;
      ack_state_ = state;
    }
    InputEventAckSource ack_source() const { return ack_source_; }

   private:
    InputEventAckSource ack_source_ = InputEventAckSource::UNKNOWN;
    InputEventAckState ack_state_ = INPUT_EVENT_ACK_STATE_UNKNOWN;
  };

  bool OnScrollBegin(const GestureEventWithLatencyInfo& gesture_event);

  // Inovked on the expiration of the debounce interval to release
  // deferred events.
  void SendScrollEndingEventsNow();

  // Sub-filter for removing bounces from in-progress scrolls.
  bool ShouldForwardForBounceReduction(
      const GestureEventWithLatencyInfo& gesture_event);

  // Puts the events in a queue to forward them one by one; i.e., forward them
  // whenever ACK for previous event is received. This queue also tries to
  // coalesce events as much as possible.
  void QueueAndForwardIfNecessary(
      const GestureEventWithLatencyInfo& gesture_event);

  // Merge or append a GestureScrollUpdate or GesturePinchUpdate into
  // the coalescing queue, forwarding immediately if appropriate.
  void QueueScrollOrPinchAndForwardIfNecessary(
      const GestureEventWithLatencyInfo& gesture_event);

  // ACK completed events in order until we have reached an incomplete event.
  // Will preserve the FIFO order as events originally arrived.
  void AckCompletedEvents();
  void AckGestureEventToClient(const GestureEventWithLatencyInfo&,
                               InputEventAckSource,
                               InputEventAckState);

  // Used when |allow_multiple_inflight_events_| is false. Will only send next
  // event after receiving ACK for the previous one.
  void LegacyProcessGestureAck(InputEventAckSource,
                               InputEventAckState,
                               blink::WebInputEvent::Type,
                               const ui::LatencyInfo&);

  // The number of sent events for which we're awaiting an ack.  These events
  // remain at the head of the queue until ack'ed.
  size_t EventsInFlightCount() const;

  bool FlingInProgressForTest() const;

  // The receiver of all forwarded gesture events.
  GestureEventQueueClient* client_;

  // True if a GestureScrollUpdate sequence is in progress.
  bool scrolling_in_progress_;

  // True if two related gesture events were sent before without waiting
  // for an ACK, so the next gesture ACK should be ignored.
  bool ignore_next_ack_;

  // True if compositor event queue is enabled. GestureEventQueue won't coalesce
  // events and will forward events immediately (instead of waiting for previous
  // ack).
  bool allow_multiple_inflight_events_;

  bool processing_acks_ = false;

  using GestureQueue = base::circular_deque<GestureEventWithLatencyInfo>;
  using GestureQueueWithAckState =
      base::circular_deque<GestureEventWithLatencyInfoAndAckState>;

  // If |allow_multiple_inflight_events_|, |coalesced_gesture_events_| stores
  // outstanding events that have been sent to the renderer but not yet been
  // ACKed.
  // Otherwise it stores coalesced gesture events not yet sent to the renderer.
  // If |ignore_next_ack_| is false, then the event at the front of the queue
  // has been sent and is awaiting an ACK, and all other events have yet to be
  // sent. If |ignore_next_ack_| is true, then the two events at the front of
  // the queue have been sent, and the second is awaiting an ACK. All other
  // events have yet to be sent.
  GestureQueueWithAckState coalesced_gesture_events_;

  // Timer to release a previously deferred gesture event.
  base::OneShotTimer debounce_deferring_timer_;

  // Queue of events that have been deferred for debounce.
  GestureQueue debouncing_deferral_queue_;

  // Time window in which to debounce scroll/fling ends. Note that an interval
  // of zero effectively disables debouncing.
  base::TimeDelta debounce_interval_;

  // An object for filtering unnecessary GFC events, as well as gestureTap/mouse
  // events that happen immediately after touchscreen/touchpad fling canceling
  // taps.
  FlingController fling_controller_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_EVENT_QUEUE_H_
