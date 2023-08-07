// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EVENTS_EVENT_ACK_DATA_H_
#define EXTENSIONS_BROWSER_EVENTS_EVENT_ACK_DATA_H_

#include <map>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/uuid.h"

namespace content {
class ServiceWorkerContext;
}

namespace extensions {

enum class EventDispatchSource : int;

// Manages in-flight events for extension Service Worker.
class EventAckData {
 public:
  EventAckData();

  EventAckData(const EventAckData&) = delete;
  EventAckData& operator=(const EventAckData&) = delete;

  ~EventAckData();

  // Records the fact that an event with `event_id` was dispatched to an
  // extension Service Worker and we expect an ack for the event from the worker
  // later on.
  void IncrementInflightEvent(content::ServiceWorkerContext* context,
                              int render_process_id,
                              int64_t version_id,
                              int event_id,
                              base::TimeTicks dispatch_start_time,
                              EventDispatchSource dispatch_source);
  // Clears the record of our knowledge of an in-flight event with |event_id|.
  //
  // On failure, |failure_callback| is called synchronously or asynchronously.
  void DecrementInflightEvent(content::ServiceWorkerContext* context,
                              int render_process_id,
                              int64_t version_id,
                              int event_id,
                              bool worker_stopped,
                              base::OnceClosure failure_callback);

 private:
  // Information about an unacked event.
  struct EventInfo {
    // Uuid of the Service Worker's external request for the event.
    base::Uuid request_uuid;
    // RenderProcessHost id.
    int render_process_id;
    // Whether or not StartExternalRequest succeeded.
    bool start_ok;
    // The time the event was dispatched to the event router for the extension.
    base::TimeTicks dispatch_start_time;
    // The event dispatching processing flow that was followed for this event.
    EventDispatchSource dispatch_source;
  };

  // TODO(crbug.com/1441221): Mark events that are not acked within 5 minutes
  // (if the worker is still around) as stale, and emit
  // Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2 at that point.
  // Acks after that point should check staleness and not emit a second time.
  std::map<int, EventInfo> unacked_events_;

  base::WeakPtrFactory<EventAckData> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENTS_EVENT_ACK_DATA_H_
