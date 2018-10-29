// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_

#include <stddef.h>
#include <map>
#include <set>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_context_request_handler.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/browser/service_worker/service_worker_installed_script_reader.h"
#include "content/common/service_worker/embedded_worker.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/service_worker_context.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace content {

enum class EmbeddedWorkerStatus;

class ServiceWorkerMetrics {
 public:
  // Used for UMA. Append-only.
  enum class MainResourceRequestDestination {
    // The request was routed to the service worker. Fetch event dispatching
    // possibly succeeded or failed.
    // ServiceWorker.FetchEvent.MainResource.Status was logged with the result
    // of the dispatch.
    kServiceWorker = 0,

    // The request was routed to network for the specified reason.
    kNetworkBecauseNoActiveVersion = 1,
    kNetworkBecauseNoActiveVersionAfterContinuing = 2,
    kNetworkBecauseNoContext = 3,
    kNetworkBecauseNoFetchEventHandler = 4,
    kNetworkBecauseNoProvider = 5,
    kNetworkBecauseNoProviderAfterContinuing = 6,
    kNetworkBecauseNoRegistration = 7,
    kNetworkBecauseNotAllowed = 8,
    kNetworkBecauseNotSecure = 9,

    // The loader couldn't dispatch the fetch event because there was no active
    // worker.
    kErrorNoActiveWorkerFromDelegate = 10,
    // The loader couldn't dispatch the fetch event because the request body
    // failed.
    kErrorRequestBodyFailed = 11,

    // The request was being routed to the service worker, but the handler was
    // destroyed before the result of the fetch event dispatch was received.
    kAbortedWhileDispatchingFetchEvent = 12,
    // The handler was destroyed without dispatching a fetch event to the
    // service
    // worker.
    kAbortedWithoutDispatchingFetchEvent = 13,

    // The request was not routed because the job was destroyed.
    kJobWasDestroyed = 14,

    kMaxValue = 14,
  };

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
  enum DeleteAndStartOverResult {
    DELETE_OK,
    DELETE_DATABASE_ERROR,
    DELETE_DISK_CACHE_ERROR,
    NUM_DELETE_AND_START_OVER_RESULT_TYPES,
  };

  // Used for UMA. Append-only.
  enum URLRequestJobResult {
    // The service worker fell back to network.
    REQUEST_JOB_FALLBACK_RESPONSE = 0,

    // The service worker fell back to network and CORS check is needed.
    REQUEST_JOB_FALLBACK_FOR_CORS = 1,

    // The service worker responded with headers only (no body).
    REQUEST_JOB_HEADERS_ONLY_RESPONSE = 2,

    // The service worker responded with a stream body.
    REQUEST_JOB_STREAM_RESPONSE = 3,

    // The service worker responded with a blob body.
    REQUEST_JOB_BLOB_RESPONSE = 4,

    // The renderer responded with network error (see
    // RecordStatusZeroResponseError() for error reasons).
    REQUEST_JOB_ERROR_RESPONSE_STATUS_ZERO = 5,

    // The renderer returned a response blob that could not be read.
    REQUEST_JOB_ERROR_BAD_BLOB = 6,

    // The provider host for the request was destroyed before the request
    // could start.
    REQUEST_JOB_ERROR_NO_PROVIDER_HOST = 7,

    // The service worker assigned to the request could not be found, when
    // the request tried to start.
    REQUEST_JOB_ERROR_NO_ACTIVE_VERSION = 8,

    // Obsolete.
    // REQUEST_JOB_ERROR_NO_REQUEST = 9,

    // An error occurred attempting to dispatch the event to the service worker.
    REQUEST_JOB_ERROR_FETCH_EVENT_DISPATCH = 10,

    // An error occurred while reading the blob response.
    REQUEST_JOB_ERROR_BLOB_READ = 11,

    // The connection to the stream response was destroyed before all the data
    // was read.
    REQUEST_JOB_ERROR_STREAM_ABORTED = 12,

    // The request job destructed before it finished.
    REQUEST_JOB_ERROR_KILLED = 13,

    // The request job destructed before it finished. It was reading
    // a blob response.
    REQUEST_JOB_ERROR_KILLED_WITH_BLOB = 14,

    // The request job was destructed before it finished. It was reading
    // a stream response.
    REQUEST_JOB_ERROR_KILLED_WITH_STREAM = 15,

    // Obsolete.
    // REQUEST_JOB_ERROR_DESTROYED = 16,
    // REQUEST_JOB_ERROR_DESTROYED_WITH_BLOB = 17,
    // REQUEST_JOB_ERROR_DESTROYED_WITH_STREAM = 18,

    // The request job delegate behaved incorrectly.
    REQUEST_JOB_ERROR_BAD_DELEGATE = 19,

    // The browser failed to construct the request body.
    REQUEST_JOB_ERROR_REQUEST_BODY_BLOB_FAILED = 20,

    NUM_REQUEST_JOB_RESULT_TYPES,
  };

  // Used for UMA. Append-only.
  enum class StopStatus {
    NORMAL,
    DETACH_BY_REGISTRY,
    TIMEOUT,
    // Add new types here.
    kMaxValue = TIMEOUT,
  };

  // Used for UMA. Append-only.
  // This class is used to indicate which event is fired/finished. Most events
  // have only one request that starts the event and one response that finishes
  // the event, but the fetch and the foreign fetch event have two responses, so
  // there are two types of EventType to break down the measurement into two:
  // FETCH/FOREIGN_FETCH and FETCH_WAITUNTIL/FOREIGN_FETCH_WAITUNTIL.
  // Moreover, FETCH is separated into the four: MAIN_FRAME, SUB_FRAME,
  // SHARED_WORKER and SUB_RESOURCE for more detailed UMA.
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
    FOREIGN_FETCH = 15,
    FETCH_WAITUNTIL = 16,
    FOREIGN_FETCH_WAITUNTIL = 17,
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
    LONG_RUNNING_MESSAGE = 31,
    BACKGROUND_FETCH_SUCCESS = 32,
    // Add new events to record here.
    kMaxValue = BACKGROUND_FETCH_SUCCESS,
  };

  // Used for UMA. Append only.
  enum class Site {
    OTHER,  // Obsolete for UMA. Use WITH_FETCH_HANDLER or
            // WITHOUT_FETCH_HANDLER.
    NEW_TAB_PAGE,
    WITH_FETCH_HANDLER,
    WITHOUT_FETCH_HANDLER,
    PLUS,
    INBOX,
    DOCS,
    kMaxValue = DOCS,
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
    // process was created for the navigation by PlzNavigate but the IPC
    // connection is not established yet.)
    EXISTING_UNREADY_PROCESS,
    // The service worker started up in an existing ready process.
    EXISTING_READY_PROCESS
  };

  // Used for UMA. Append only.
  // This enum describes how an activated worker was found and prepared (i.e.,
  // reached the RUNNING status) in order to dispatch a fetch event to.
  enum class WorkerPreparationType {
    UNKNOWN = 0,
    // The worker was already starting up. We waited for it to finish.
    STARTING = 1,
    // The worker was already running.
    RUNNING = 2,
    // The worker was stopping. We waited for it to stop, and then started it
    // up.
    STOPPING = 3,
    // The worker was in the stopped state. We started it up, and startup
    // required a new process to be created.
    START_IN_NEW_PROCESS = 4,
    // Deprecated 07/2017; replaced by START_IN_EXISTING_UNREADY_PROCESS and
    // START_IN_EXISTING_READY_PROCESS.
    //   START_IN_EXISTING_PROCESS = 5,
    // The worker was in the stopped state. We started it up, and this occurred
    // during browser startup.
    START_DURING_STARTUP = 6,
    // The worker was in the stopped state. We started it up, and it used an
    // existing unready process.
    START_IN_EXISTING_UNREADY_PROCESS = 7,
    // The worker was in the stopped state. We started it up, and it used an
    // existing ready process.
    START_IN_EXISTING_READY_PROCESS = 8,
    // Add new types here.
    kMaxValue = START_IN_EXISTING_READY_PROCESS,
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

  // Records worker activities. Currently this only records
  // StartHintPrecision histogram.
  class ScopedEventRecorder {
   public:
    ScopedEventRecorder();
    ~ScopedEventRecorder();

    void RecordEventHandledStatus(EventType event);

   private:
    bool frame_fetch_event_fired_ = false;

    DISALLOW_COPY_AND_ASSIGN(ScopedEventRecorder);
  };

  // Converts an event type to a string. Used for tracing.
  static const char* EventTypeToString(EventType event_type);

  // Converts a start situation to a string. Used for tracing.
  static const char* StartSituationToString(StartSituation start_situation);

  // If the |url| is not a special site, returns Site::OTHER.
  static Site SiteFromURL(const GURL& url);

  // Excludes NTP scope from UMA for now as it tends to dominate the stats and
  // makes the results largely skewed. Some metrics don't follow this policy
  // and hence don't call this function.
  static bool ShouldExcludeSiteFromHistogram(Site site);

  // Used for ServiceWorkerDiskCache.
  static void CountInitDiskCacheResult(bool result);
  static void CountReadResponseResult(ReadResponseResult result);
  static void CountWriteResponseResult(WriteResponseResult result);

  // Used for ServiceWorkerDatabase.
  static void CountOpenDatabaseResult(ServiceWorkerDatabase::Status status);
  static void CountReadDatabaseResult(ServiceWorkerDatabase::Status status);
  static void CountWriteDatabaseResult(ServiceWorkerDatabase::Status status);
  static void RecordDestroyDatabaseResult(ServiceWorkerDatabase::Status status);

  // Used for ServiceWorkerStorage.
  static void RecordPurgeResourceResult(int net_error);
  static void RecordDeleteAndStartOverResult(DeleteAndStartOverResult result);

  // Counts the number of page loads controlled by a Service Worker.
  static void CountControlledPageLoad(Site site,
                                      const GURL& url,
                                      bool is_main_frame_load);

  // Records the result of trying to start a worker. |is_installed| indicates
  // whether the version has been installed.
  static void RecordStartWorkerStatus(blink::ServiceWorkerStatusCode status,
                                      EventType purpose,
                                      bool is_installed);

  // Records the result of sending installed scripts to the renderer.
  static void RecordInstalledScriptsSenderStatus(
      ServiceWorkerInstalledScriptReader::FinishedReason reason);

  // Records the time taken to successfully start a worker. |is_installed|
  // indicates whether the version has been installed.
  //
  // TODO(crbug.com/855952): Replace this with RecordStartWorkerTiming().
  static void RecordStartWorkerTime(base::TimeDelta time,
                                    bool is_installed,
                                    StartSituation start_situation,
                                    EventType purpose);

  // Records metrics for the preparation of an activated Service Worker for a
  // main frame navigation.
  CONTENT_EXPORT static void RecordActivatedWorkerPreparationForMainFrame(
      base::TimeDelta time,
      EmbeddedWorkerStatus initial_worker_status,
      StartSituation start_situation,
      bool did_navigation_preload,
      const GURL& url);

  // Records the result of trying to stop a worker.
  static void RecordWorkerStopped(StopStatus status);

  // Records the time taken to successfully stop a worker.
  static void RecordStopWorkerTime(base::TimeDelta time);

  static void RecordActivateEventStatus(blink::ServiceWorkerStatusCode status,
                                        bool is_shutdown);
  static void RecordInstallEventStatus(blink::ServiceWorkerStatusCode status);

  // Records how often a dispatched event times out.
  static void RecordEventTimeout(EventType event);

  // Records the amount of time spent handling an event.
  static void RecordEventDuration(EventType event,
                                  base::TimeDelta time,
                                  bool was_handled);

  // Records the time taken between sending an event IPC from the browser
  // process to a Service Worker and executing the event handler in the Service
  // Worker.
  static void RecordEventDispatchingDelay(EventType event,
                                          base::TimeDelta time);

  // Records the result of dispatching a fetch event to a service worker.
  static void RecordFetchEventStatus(bool is_main_resource,
                                     blink::ServiceWorkerStatusCode status);

  // Records result of a ServiceWorkerURLRequestJob that was forwarded to
  // the service worker.
  static void RecordURLRequestJobResult(bool is_main_resource,
                                        URLRequestJobResult result);

  // Records the error code provided when the renderer returns a response with
  // status zero to a fetch request.
  static void RecordStatusZeroResponseError(
      bool is_main_resource,
      blink::mojom::ServiceWorkerResponseError error);

  // Records the mode of request that was fallbacked to the network.
  static void RecordFallbackedRequestMode(
      network::mojom::FetchRequestMode mode);

  static void RecordProcessCreated(bool is_new_process);

  CONTENT_EXPORT static void RecordStartWorkerTiming(const StartTimes& times,
                                                     StartSituation situation);
  static void RecordStartWorkerTimingClockConsistency(
      CrossProcessTimeDelta type);

  // Records the result of a start attempt that occurred after the worker had
  // failed |failure_count| consecutive times.
  static void RecordStartStatusAfterFailure(
      int failure_count,
      blink::ServiceWorkerStatusCode status);

  // Records the size of Service-Worker-Navigation-Preload header when the
  // navigation preload request is to be sent.
  static void RecordNavigationPreloadRequestHeaderSize(size_t size);

  // Records timings for the navigation preload response and how
  // it compares to starting the worker.
  // |worker_start| is the time it took to prepare an activated and running
  // worker to receive the fetch event. |initial_worker_status| and
  // |start_situation| describe the preparation needed.
  // |response_start| is the time it took until the navigation preload response
  // started.
  // |resource_type| must be RESOURCE_TYPE_MAIN_FRAME or
  // RESOURCE_TYPE_SUB_FRAME.
  CONTENT_EXPORT static void RecordNavigationPreloadResponse(
      base::TimeDelta worker_start,
      base::TimeDelta response_start,
      EmbeddedWorkerStatus initial_worker_status,
      StartSituation start_situation,
      ResourceType resource_type);

  // Records the result of trying to handle a request for a service worker
  // script.
  static void RecordContextRequestHandlerStatus(
      ServiceWorkerContextRequestHandler::CreateJobStatus status,
      bool is_installed,
      bool is_main_script);

  static void RecordRuntime(base::TimeDelta time);

  // Records the result of starting service worker for a navigation hint.
  static void RecordStartServiceWorkerForNavigationHintResult(
      StartServiceWorkerForNavigationHintResult result);

  // Records the number of origins with a registered service worker.
  static void RecordRegisteredOriginCount(size_t origin_count);

  static void RecordMainResourceRequestDestination(
      MainResourceRequestDestination destination);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ServiceWorkerMetrics);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_
