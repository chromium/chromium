// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_COMPOSITOR_THREAD_EVENT_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_COMPOSITOR_THREAD_EVENT_QUEUE_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/widget/input/event_with_callback.h"

namespace blink {

namespace test {
class InputHandlerProxyEventQueueTest;
}

// CompositorThreadEventQueue is a coalescing queue. It will examine
// the current events in the queue and will attempt to coalesce with
// the last event.
class PLATFORM_EXPORT CompositorThreadEventQueue {
 public:
  CompositorThreadEventQueue();
  CompositorThreadEventQueue(const CompositorThreadEventQueue&) = delete;
  CompositorThreadEventQueue& operator=(const CompositorThreadEventQueue&) =
      delete;
  ~CompositorThreadEventQueue();

  // Adds an event to the queue. The event may be coalesced with the last event
  // if kRefactorCompositorThreadEventQueue is disabled.
  void Queue(std::unique_ptr<EventWithCallback> event);

  std::unique_ptr<EventWithCallback> Pop();

  // Performs coalescing of continuous gesture events in the queue.
  void CoalesceEvents(base::TimeTicks sample_time);

  bool IsNextEventReady(base::TimeTicks sample_time) const;

  // Notifies the queue that the current frame's dispatch loop has finished.
  // Snapshots the remaining events as backlog for the next frame.
  void DidFinishDispatch();

  WebInputEvent::Type PeekType() const;

  // Returns the timestamp of the event at the head of the queue.
  base::TimeTicks PeekTimestamp() const;

  const WebInputEvent* FirstOriginalEvent() const;

  bool empty() const { return queue_.empty(); }

  size_t size() const { return queue_.size(); }

 private:
  friend class test::InputHandlerProxyEventQueueTest;
  using EventQueue = base::circular_deque<std::unique_ptr<EventWithCallback>>;
  EventQueue queue_;
  size_t events_to_always_dispatch_ = 0;
  size_t backlog_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_COMPOSITOR_THREAD_EVENT_QUEUE_H_
