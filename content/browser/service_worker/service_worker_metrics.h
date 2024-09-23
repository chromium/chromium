// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_

#include <stddef.h>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/service_worker_context.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "ui/base/page_transition_types.h"

namespace content {

class ServiceWorkerMetrics {
 public:
  // Used for UMA. Append-only.
  enum ReadResponseResult {
    READ_OK,
    READ_HEADERS_ERROR,
    READ_DATA_ERROR,
    NUM_READ_RESPONSE_RESULT_TYPES,
  };

  // Used for UMA. Append-only.
  enum WriteResponseResult {
    WRITE_OK,
    WRITE_HEADERS_ERROR,
    WRITE_DATA_ERROR,
    NUM_WRITE_RESPONSE_RESULT_TYPES,
  };

  // Used for UMA. Append-only.
  // This class is used to indicate which event is fired/finished. Most events
  // have only one request that starts the event and one response that finishes
  // the event, but the fetch event has two responses, so there are two types of
  // EventType to break down the measurement into two: FETCH and
  // FETCH_WAITUNTIL. Moreover, FETCH is separated into the four: MAIN_FRAME,
  // SUB_FRAME, SHARED_WORKER and SUB_RESOURCE for more detailed UMA.
  enum class EventType {
    ACTIVATE = 0,
    INSTALL = 1,
    // FETCH = 2,  // Obsolete
    SYNC = 3,
    NOTIFICATION_CLICK = 4,
    PUSH = 5,
    // GEOFENCING = 6,  // Obsolete
    // SERVICE_PORT_CONNECT = 7,  // Obsolete
    MESSAGE = 8,
    NOTIFICATION_CLOSE = 9,
    FETCH_MAIN_FRAME = 10,
    FETCH_SUB_FRAME = 11,
    FETCH_SHARED_WORKER = 12,
    FETCH_SUB_RESOURCE = 13,
    UNKNOWN = 14,  // Used when event type is not known.
    // FOREIGN_FETCH = 15,  // Obsolete
    FETCH_WAITUNTIL = 16,
    // FOREIGN_FETCH_WAITUNTIL = 17,  // Obsolete
    // NAVIGATION_HINT_LINK_MOUSE_DOWN = 18,  // Obsolete
    // NAVIGATION_HINT_LINK_TAP_UNCONFIRMED = 19,  // Obsolete
    // NAVIGATION_HINT_LINK_TAP_DOWN = 20,  // Obsolete
    // Used when external consumers want to add a request to
    // ServiceWorkerVersion to keep it alive.
    EXTERNAL_REQUEST = 21,
    PAYMENT_REQUEST = 22,
    BACKGROUND_FETCH_ABORT = 23,
    BACKGROUND_FETCH_CLICK = 24,
    BACKGROUND_FETCH_FAIL = 25,
    // BACKGROUND_FETCHED = 26,  // Obsolete
    NAVIGATION_HINT = 27,
    CAN_MAKE_PAYMENT = 28,
    ABORT_PAYMENT = 29,
    COOKIE_CHANGE = 30,
    // LONG_RUNNING_MESSAGE = 31, // Obsolete
    BACKGROUND_FETCH_SUCCESS = 32,
    PERIODIC_SYNC = 33,
    CONTENT_DELETE = 34,
    PUSH_SUBSCRIPTION_CHANGE = 35,
    FETCH_FENCED_FRAME = 36,
    BYPASS_MAIN_RESOURCE = 37,
    SKIP_EMPTY_FETCH_HANDLER = 38,
    BYPASS_ONLY_IF_SERVICE_WORKER_NOT_STARTED = 39,
    WARM_UP = 40,
    STATIC_ROUTER = 41,
    // Add new events to record here.
    kMaxValue = STATIC_ROUTER,
  };

  // Not used for UMA.
  enum class StartSituation {
    // Failed to allocate a process.
    UNKNOWN,
    // The service worker started up during browser startup.
    DURING_STARTUP,
    // The service worker started up in a new process.
    NEW_PROCESS,
    // The service worker started up in an existing unready process. (Ex: The
    // process was created for the navigation but the IPC connection is not
    // established yet.)
    EXISTING_UNREADY_PROCESS,
    // The service worker started up in an existing ready process.
    EXISTING_READY_PROCESS
  };

  // Used for UMA. Append only.
  // Describes the outcome of a time measurement taken between processes.
  enum class CrossProcessTimeDelta {
    NORMAL,
    NEGATIVE,
    INACCURATE_CLOCK,
    // Add new types here.
    kMaxValue = INACCURATE_CLOCK,
  };

  // These are prefixed with "local" or "remote" to indicate whether the browser
  // process or renderer process recorded the timing (browser is local).
  struct StartTimes {
    // The browser started the service worker startup sequence.
    base::TimeTicks local_start;

    // The browser sent the start worker IPC to the renderer.
    base::TimeTicks local_start_worker_sent;

    // The renderer received the start worker IPC.
    base::TimeTicks remote_start_worker_received;

    // The renderer started script evaluation on the worker thread.
    base::TimeTicks remote_script_evaluation_start;

    // The renderer finished script evaluation on the worker thread.
    base::TimeTicks remote_script_evaluation_end;

    // The browser received the worker started IPC.
    base::TimeTicks local_end;
  };

  ServiceWorkerMetrics() = delete;
  ServiceWorkerMetrics(const ServiceWorkerMetrics&) = delete;
  ServiceWorkerMetrics& operator=(const ServiceWorkerMetrics&) = delete;

  // Converts an event type to a string. Used for tracing.
  static const char* EventTypeToString(EventType event_type);

  // Converts a start situation to a string. Used for tracing.
  static const char* StartSituationToString(StartSituation start_situation);

  // Counts the result of reading a service worker script from storage.
  static void CountReadResponseResult(ReadResponseResult result);
  // Counts the result of writing a service worker script to storage.
  static void CountWriteResponseResult(WriteResponseResult result);

  // Records the result of trying to start an installed worker.
  static void RecordStartInstalledWorkerStatus(
      blink::ServiceWorkerStatusCode status,
      EventType purpose);

  // Records the running status of the worker to receive a task.
  // Usually recorded for the fetch handler.
  static void RecordRunAfterStartWorkerStatus(
      blink::EmbeddedWorkerStatus running_status,
      EventType purpose);

  // Records the time taken to successfully start a worker. |is_installed|
  // indicates whether the version has been installed.
  //
  // TODO(crbug.com/40582160): Replace this with RecordStartWorkerTiming().
  static void RecordStartWorkerTime(base::TimeDelta time,
                                    bool is_installed,
                                    StartSituation start_situation,
                                    EventType purpose);

  static void RecordActivateEventStatus(blink::ServiceWorkerStatusCode status,
                                        bool is_shutdown);
  static void RecordInstallEventStatus(blink::ServiceWorkerStatusCode status,
                                       uint32_t fetch_count);

  // Records the amount of time spent handling an event.
  static void RecordEventDuration(EventType event,
                                  base::TimeDelta time,
                                  bool was_handled,
                                  uint32_t fetch_count);

  // Records the result of dispatching a fetch event to a service worker.
  static void RecordFetchEventStatus(bool is_main_resource,
                                     blink::ServiceWorkerStatusCode status);

  CONTENT_EXPORT static void RecordStartWorkerTiming(const StartTimes& times,
                                                     StartSituation situation);
  static void RecordStartWorkerTimingClockConsistency(
      CrossProcessTimeDelta type);

  // Records the size of Service-Worker-Navigation-Preload header when the
  // navigation preload request is to be sent.
  static void RecordNavigationPreloadRequestHeaderSize(size_t size);

  static void RecordSkipServiceWorkerOnNavigation(bool skip_service_worker);

  static void RecordFindRegistrationForClientUrlTime(base::TimeDelta time);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_
