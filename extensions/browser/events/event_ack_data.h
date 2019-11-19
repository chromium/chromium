// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EVENTS_EVENT_ACK_DATA_H_
#define EXTENSIONS_BROWSER_EVENTS_EVENT_ACK_DATA_H_

#include <map>

#include "base/memory/weak_ptr.h"

namespace content {
class ServiceWorkerContext;
}

namespace extensions {

// Manages inflight events for extension Service Worker.
class EventAckData {
 public:
  EventAckData();
  ~EventAckData();

  // Records the fact that an event with |event_id| was dispatched to an
  // extension Service Worker and we expect an ack for the event from the worker
  // later on.
  void IncrementInflightEvent(content::ServiceWorkerContext* context,
                              int render_process_id,
                              int64_t version_id,
                              int event_id);
  // Clears the record of our knowledge of an inflight event with |event_id|.
  //
  // On failure, |failure_callback| is called synchronously or asynchronously.
  void DecrementInflightEvent(content::ServiceWorkerContext* context,
                              int render_process_id,
                              int64_t version_id,
                              int event_id,
                              bool worker_stopped,
                              base::OnceClosure failure_callback);

 private:
  class CoreThreadEventInfo;

  static void StartExternalRequestOnCoreThread(
      content::ServiceWorkerContext* context,
      int render_process_id,
      int64_t version_id,
      int event_id,
      scoped_refptr<EventAckData::CoreThreadEventInfo> unacked_events);

  static void FinishExternalRequestOnCoreThread(
      content::ServiceWorkerContext* context,
      int render_process_id,
      int64_t version_id,
      int event_id,
      bool worker_stopped,
      scoped_refptr<CoreThreadEventInfo> unacked_events,
      base::OnceClosure failure_callback);

  // Contains map of unacked event information keyed by event id.
  // Created on UI thread, but accessed only on the core thread.
  scoped_refptr<CoreThreadEventInfo> unacked_events_;

  base::WeakPtrFactory<EventAckData> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EventAckData);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENTS_EVENT_ACK_DATA_H_
