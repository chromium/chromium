// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_metrics.h"

#include <limits>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/url_request/url_request.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

namespace content {

namespace {

const char* StartSituationToSuffix(
    ServiceWorkerMetrics::StartSituation situation) {
  // Don't change these returned strings. They are written (in hashed form) into
  // logs.
  switch (situation) {
    case ServiceWorkerMetrics::StartSituation::UNKNOWN:
      NOTREACHED();
      return ".Unknown";
    case ServiceWorkerMetrics::StartSituation::DURING_STARTUP:
      return ".DuringStartup";
    case ServiceWorkerMetrics::StartSituation::NEW_PROCESS:
      return ".NewProcess";
    case ServiceWorkerMetrics::StartSituation::EXISTING_UNREADY_PROCESS:
      return ".ExistingUnreadyProcess";
    case ServiceWorkerMetrics::StartSituation::EXISTING_READY_PROCESS:
      return ".ExistingReadyProcess";
  }
  NOTREACHED() << static_cast<int>(situation);
  return ".Unknown";
}

// TODO(falken): Remove this when the associated UMA are removed.
const char* StartSituationToDeprecatedSuffix(
    ServiceWorkerMetrics::StartSituation situation) {
  // Don't change this returned string. It is written (in hashed form) into
  // logs.
  switch (situation) {
    case ServiceWorkerMetrics::StartSituation::UNKNOWN:
      NOTREACHED();
      return "_Unknown";
    case ServiceWorkerMetrics::StartSituation::DURING_STARTUP:
      return "_DuringStartup";
    case ServiceWorkerMetrics::StartSituation::NEW_PROCESS:
      return "_NewProcess";
    case ServiceWorkerMetrics::StartSituation::EXISTING_UNREADY_PROCESS:
      return "_ExistingUnreadyProcess";
    case ServiceWorkerMetrics::StartSituation::EXISTING_READY_PROCESS:
      return "_ExistingReadyProcess";
  }
  NOTREACHED() << static_cast<int>(situation);
  return "_Unknown";
}

const char* EventTypeToSuffix(ServiceWorkerMetrics::EventType event_type) {
  // Don't change these returned strings. They are written (in hashed form) into
  // logs.
  switch (event_type) {
    case ServiceWorkerMetrics::EventType::ACTIVATE:
      return "_ACTIVATE";
    case ServiceWorkerMetrics::EventType::INSTALL:
      return "_INSTALL";
    case ServiceWorkerMetrics::EventType::SYNC:
      return "_SYNC";
    case ServiceWorkerMetrics::EventType::NOTIFICATION_CLICK:
      return "_NOTIFICATION_CLICK";
    case ServiceWorkerMetrics::EventType::PUSH:
      return "_PUSH";
    case ServiceWorkerMetrics::EventType::MESSAGE:
      return "_MESSAGE";
    case ServiceWorkerMetrics::EventType::NOTIFICATION_CLOSE:
      return "_NOTIFICATION_CLOSE";
    case ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME:
      return "_FETCH_MAIN_FRAME";
    case ServiceWorkerMetrics::EventType::FETCH_SUB_FRAME:
      return "_FETCH_SUB_FRAME";
    case ServiceWorkerMetrics::EventType::FETCH_SHARED_WORKER:
      return "_FETCH_SHARED_WORKER";
    case ServiceWorkerMetrics::EventType::FETCH_SUB_RESOURCE:
      return "_FETCH_SUB_RESOURCE";
    case ServiceWorkerMetrics::EventType::UNKNOWN:
      return "_UNKNOWN";
    case ServiceWorkerMetrics::EventType::FOREIGN_FETCH:
      return "_FOREIGN_FETCH";
    case ServiceWorkerMetrics::EventType::FETCH_WAITUNTIL:
      return "_FETCH_WAITUNTIL";
    case ServiceWorkerMetrics::EventType::FOREIGN_FETCH_WAITUNTIL:
      return "_FOREIGN_FETCH_WAITUNTIL";
    case ServiceWorkerMetrics::EventType::EXTERNAL_REQUEST:
      return "_EXTERNAL_REQUEST";
    case ServiceWorkerMetrics::EventType::PAYMENT_REQUEST:
      return "_PAYMENT_REQUEST";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_ABORT:
      return "_BACKGROUND_FETCH_ABORT";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_CLICK:
      return "_BACKGROUND_FETCH_CLICK";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_FAIL:
      return "_BACKGROUND_FETCH_FAIL";
    case ServiceWorkerMetrics::EventType::NAVIGATION_HINT:
      return "_NAVIGATION_HINT";
    case ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT:
      return "_CAN_MAKE_PAYMENT";
    case ServiceWorkerMetrics::EventType::ABORT_PAYMENT:
      return "_ABORT_PAYMENT";
    case ServiceWorkerMetrics::EventType::COOKIE_CHANGE:
      return "_COOKIE_CHANGE";
    case ServiceWorkerMetrics::EventType::LONG_RUNNING_MESSAGE:
      return "_LONG_RUNNING_MESSAGE";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_SUCCESS:
      return "_BACKGROUND_FETCH_SUCCESS";
  }
  return "_UNKNOWN";
}

ServiceWorkerMetrics::WorkerPreparationType GetWorkerPreparationType(
    EmbeddedWorkerStatus initial_worker_status,
    ServiceWorkerMetrics::StartSituation start_situation) {
  using Situation = ServiceWorkerMetrics::StartSituation;
  using Preparation = ServiceWorkerMetrics::WorkerPreparationType;
  switch (initial_worker_status) {
    case EmbeddedWorkerStatus::STOPPED: {
      switch (start_situation) {
        case Situation::DURING_STARTUP:
          return Preparation::START_DURING_STARTUP;
        case Situation::NEW_PROCESS:
          return Preparation::START_IN_NEW_PROCESS;
        case Situation::EXISTING_UNREADY_PROCESS:
          return Preparation::START_IN_EXISTING_UNREADY_PROCESS;
        case Situation::EXISTING_READY_PROCESS:
          return Preparation::START_IN_EXISTING_READY_PROCESS;
        case Situation::UNKNOWN:
          break;
      }
      break;
    }
    case EmbeddedWorkerStatus::STARTING:
      return Preparation::STARTING;
    case EmbeddedWorkerStatus::RUNNING:
      return Preparation::RUNNING;
    case EmbeddedWorkerStatus::STOPPING:
      return Preparation::STOPPING;
  }
  NOTREACHED() << static_cast<int>(initial_worker_status);
  return Preparation::UNKNOWN;
}

const char* GetWorkerPreparationSuffix(
    ServiceWorkerMetrics::WorkerPreparationType status) {
  using Preparation = ServiceWorkerMetrics::WorkerPreparationType;
  switch (status) {
    case Preparation::UNKNOWN:
      break;
    case Preparation::START_DURING_STARTUP:
      return "_StartWorkerDuringStartup";
    case Preparation::START_IN_NEW_PROCESS:
      return "_StartWorkerNewProcess";
    case Preparation::START_IN_EXISTING_UNREADY_PROCESS:
      return "_StartWorkerExistingUnreadyProcess";
    case Preparation::START_IN_EXISTING_READY_PROCESS:
      return "_StartWorkerExistingReadyProcess";
    case Preparation::STARTING:
      return "_StartingWorker";
    case Preparation::RUNNING:
      return "_RunningWorker";
    case Preparation::STOPPING:
      return "_StoppingWorker";
  }
  NOTREACHED();
  return "_UNKNOWN";
}

void RecordURLMetricOnUI(const std::string& metric_name, const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetContentClient()->browser()->RecordURLMetric(metric_name, url);
}

}  // namespace

using ScopedEventRecorder = ServiceWorkerMetrics::ScopedEventRecorder;

ScopedEventRecorder::ScopedEventRecorder() = default;

ScopedEventRecorder::~ScopedEventRecorder() {
  UMA_HISTOGRAM_BOOLEAN("ServiceWorker.StartHintPrecision",
                        frame_fetch_event_fired_);
}

void ScopedEventRecorder::RecordEventHandledStatus(
    ServiceWorkerMetrics::EventType event) {
  if (event == EventType::FETCH_MAIN_FRAME ||
      event == EventType::FETCH_SUB_FRAME) {
    frame_fetch_event_fired_ = true;
  }
}

const char* ServiceWorkerMetrics::EventTypeToString(EventType event_type) {
  switch (event_type) {
    case EventType::ACTIVATE:
      return "Activate";
    case EventType::INSTALL:
      return "Install";
    case EventType::SYNC:
      return "Sync";
    case EventType::NOTIFICATION_CLICK:
      return "Notification Click";
    case EventType::NOTIFICATION_CLOSE:
      return "Notification Close";
    case EventType::PUSH:
      return "Push";
    case EventType::MESSAGE:
      return "Message";
    case EventType::FETCH_MAIN_FRAME:
      return "Fetch Main Frame";
    case EventType::FETCH_SUB_FRAME:
      return "Fetch Sub Frame";
    case EventType::FETCH_SHARED_WORKER:
      return "Fetch Shared Worker";
    case EventType::FETCH_SUB_RESOURCE:
      return "Fetch Subresource";
    case EventType::UNKNOWN:
      return "Unknown";
    case EventType::FOREIGN_FETCH:
      return "Foreign Fetch";
    case EventType::FETCH_WAITUNTIL:
      return "Fetch WaitUntil";
    case EventType::FOREIGN_FETCH_WAITUNTIL:
      return "Foreign Fetch WaitUntil";
    case EventType::EXTERNAL_REQUEST:
      return "External Request";
    case EventType::PAYMENT_REQUEST:
      return "Payment Request";
    case EventType::BACKGROUND_FETCH_ABORT:
      return "Background Fetch Abort";
    case EventType::BACKGROUND_FETCH_CLICK:
      return "Background Fetch Click";
    case EventType::BACKGROUND_FETCH_FAIL:
      return "Background Fetch Fail";
    case EventType::NAVIGATION_HINT:
      return "Navigation Hint";
    case EventType::CAN_MAKE_PAYMENT:
      return "Can Make Payment";
    case EventType::ABORT_PAYMENT:
      return "Abort Payment";
    case EventType::COOKIE_CHANGE:
      return "Cookie Change";
    case EventType::LONG_RUNNING_MESSAGE:
      return "Long Running Message";
    case EventType::BACKGROUND_FETCH_SUCCESS:
      return "Background Fetch Success";
  }
  NOTREACHED() << "Got unexpected event type: " << static_cast<int>(event_type);
  return "error";
}

const char* ServiceWorkerMetrics::StartSituationToString(
    StartSituation start_situation) {
  switch (start_situation) {
    case StartSituation::UNKNOWN:
      return "Unknown";
    case StartSituation::DURING_STARTUP:
      return "During startup";
    case StartSituation::NEW_PROCESS:
      return "New process";
    case StartSituation::EXISTING_UNREADY_PROCESS:
      return "Existing unready process";
    case StartSituation::EXISTING_READY_PROCESS:
      return "Existing ready process";
      break;
  }
  NOTREACHED() << "Got unexpected start situation: "
               << static_cast<int>(start_situation);
  return "error";
}

ServiceWorkerMetrics::Site ServiceWorkerMetrics::SiteFromURL(const GURL& url) {
  // TODO(falken): Plumb through ContentBrowserClient::GetMetricSuffixForURL or
  // figure out a way to remove ServiceWorkerMetrics::Site entirely instead of
  // hardcoding sites in //content.

  // This inaccurately matches google.example.com, see the TODO above.
  static const char google_like_scope_prefix[] = "https://www.google.";
  static const char ntp_scope_path[] = "/_/chrome/";
  if (base::StartsWith(url.spec(), google_like_scope_prefix,
                       base::CompareCase::INSENSITIVE_ASCII) &&
      base::StartsWith(url.path(), ntp_scope_path,
                       base::CompareCase::SENSITIVE)) {
    return ServiceWorkerMetrics::Site::NEW_TAB_PAGE;
  }

  const base::StringPiece host = url.host_piece();
  if (host == "plus.google.com")
    return ServiceWorkerMetrics::Site::PLUS;
  if (host == "inbox.google.com")
    return ServiceWorkerMetrics::Site::INBOX;
  if (host == "docs.google.com")
    return ServiceWorkerMetrics::Site::DOCS;
  if (host == "drive.google.com") {
    // TODO(falken): This should not be DOCS but historically we logged them
    // together.
    return ServiceWorkerMetrics::Site::DOCS;
  }
  return ServiceWorkerMetrics::Site::OTHER;
}

bool ServiceWorkerMetrics::ShouldExcludeSiteFromHistogram(Site site) {
  return site == ServiceWorkerMetrics::Site::NEW_TAB_PAGE;
}

void ServiceWorkerMetrics::CountInitDiskCacheResult(bool result) {
  UMA_HISTOGRAM_BOOLEAN("ServiceWorker.DiskCache.InitResult", result);
}

void ServiceWorkerMetrics::CountReadResponseResult(
    ServiceWorkerMetrics::ReadResponseResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.DiskCache.ReadResponseResult",
                            result, NUM_READ_RESPONSE_RESULT_TYPES);
}

void ServiceWorkerMetrics::CountWriteResponseResult(
    ServiceWorkerMetrics::WriteResponseResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.DiskCache.WriteResponseResult",
                            result, NUM_WRITE_RESPONSE_RESULT_TYPES);
}

void ServiceWorkerMetrics::CountOpenDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.OpenResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::CountReadDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.ReadResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::CountWriteDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.WriteResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::RecordDestroyDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.DestroyDatabaseResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::RecordPurgeResourceResult(int net_error) {
  base::UmaHistogramSparse("ServiceWorker.Storage.PurgeResourceResult",
                           std::abs(net_error));
}

void ServiceWorkerMetrics::RecordDeleteAndStartOverResult(
    DeleteAndStartOverResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Storage.DeleteAndStartOverResult",
                            result, NUM_DELETE_AND_START_OVER_RESULT_TYPES);
}

void ServiceWorkerMetrics::CountControlledPageLoad(Site site,
                                                   const GURL& url,
                                                   bool is_main_frame_load) {
  DCHECK_NE(site, Site::OTHER);
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.PageLoad", site);
  if (is_main_frame_load) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.MainFramePageLoad", site);
  }
  if (ShouldExcludeSiteFromHistogram(site))
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&RecordURLMetricOnUI, "ServiceWorker.ControlledPageUrl",
                     url));
}

void ServiceWorkerMetrics::RecordStartWorkerStatus(
    blink::ServiceWorkerStatusCode status,
    EventType purpose,
    bool is_installed) {
  if (!is_installed) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartNewWorker.Status", status);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.Status", status);
  base::UmaHistogramEnumeration(
      base::StrCat({"ServiceWorker.StartWorker.StatusByPurpose",
                    EventTypeToSuffix(purpose)}),
      status);
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.Purpose", purpose);
  if (status == blink::ServiceWorkerStatusCode::kErrorTimeout) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.Timeout.StartPurpose",
                              purpose);
  }
}

void ServiceWorkerMetrics::RecordInstalledScriptsSenderStatus(
    ServiceWorkerInstalledScriptReader::FinishedReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "ServiceWorker.StartWorker.InstalledScriptsSender.FinishedReason", reason,
      ServiceWorkerInstalledScriptReader::FinishedReason::kMaxValue);
}

void ServiceWorkerMetrics::RecordStartWorkerTime(base::TimeDelta time,
                                                 bool is_installed,
                                                 StartSituation start_situation,
                                                 EventType purpose) {
  if (is_installed) {
    UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartWorker.Time", time);
    base::UmaHistogramMediumTimes(
        base::StrCat({"ServiceWorker.StartWorker.Time",
                      StartSituationToDeprecatedSuffix(start_situation)}),
        time);
    base::UmaHistogramMediumTimes(
        base::StrCat({"ServiceWorker.StartWorker.Time",
                      StartSituationToDeprecatedSuffix(start_situation),
                      EventTypeToSuffix(purpose)}),
        time);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartNewWorker.Time", time);
  }
}

void ServiceWorkerMetrics::RecordActivatedWorkerPreparationForMainFrame(
    base::TimeDelta time,
    EmbeddedWorkerStatus initial_worker_status,
    StartSituation start_situation,
    bool did_navigation_preload,
    const GURL& url) {
  // Record the worker preparation type.
  WorkerPreparationType preparation =
      GetWorkerPreparationType(initial_worker_status, start_situation);
  UMA_HISTOGRAM_ENUMERATION(
      "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Type", preparation);
  std::string suffix =
      GetContentClient()->browser()->GetMetricSuffixForURL(url);
  if (!suffix.empty()) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"ServiceWorker.ActivatedWorkerPreparationForMainFrame.Type.",
             suffix}),
        preparation);
  }

  if (did_navigation_preload) {
    // TODO(falken): Consider removing this UMA if it turns out the same as
    // ServiceWorker.NavPreload.WorkerPreparationType. That UMA is logged at
    // the same time as the other NavPreload metrics (which requires both the
    // worker to start and the nav preload response to arrive successfuly), so
    // they are more safely compared together.
    UMA_HISTOGRAM_ENUMERATION(
        "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Type_"
        "NavigationPreloadEnabled",
        preparation);
  }

  // Don't record .Time if S13nServiceWorker is enabled.
  // https://crbug.com/852664
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  // Record the preparation time.
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Time", time);

  // Record the preparation time using the worker preparation suffix.
  base::UmaHistogramMediumTimes(
      base::StrCat({"ServiceWorker.ActivatedWorkerPreparationForMainFrame.Time",
                    GetWorkerPreparationSuffix(preparation)}),
      time);

  // Record the preparation time using the navigation preload suffix.
  if (did_navigation_preload) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Time_"
        "NavigationPreloadEnabled",
        time);
    // We're mostly interested in when the worker needed to start up.
    if (initial_worker_status != EmbeddedWorkerStatus::RUNNING) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Time_"
          "WorkerStartOccurred_NavigationPreloadEnabled",
          time);
    }
  }
}

void ServiceWorkerMetrics::RecordWorkerStopped(StopStatus status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.WorkerStopped", status);
}

void ServiceWorkerMetrics::RecordStopWorkerTime(base::TimeDelta time) {
  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StopWorker.Time", time);
}

void ServiceWorkerMetrics::RecordActivateEventStatus(
    blink::ServiceWorkerStatusCode status,
    bool is_shutdown) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.ActivateEventStatus", status);
  if (is_shutdown) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.ActivateEventStatus_InShutdown",
                              status);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.ActivateEventStatus_NotInShutdown",
                              status);
  }
}

void ServiceWorkerMetrics::RecordInstallEventStatus(
    blink::ServiceWorkerStatusCode status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.InstallEventStatus", status);
}

void ServiceWorkerMetrics::RecordEventDispatchingDelay(EventType event_type,
                                                       base::TimeDelta time) {
  static constexpr char kName[] = "ServiceWorker.EventDispatchingDelay";
  UMA_HISTOGRAM_TIMES(kName, time);
  base::UmaHistogramTimes(base::StrCat({kName, EventTypeToSuffix(event_type)}),
                          time);
}

void ServiceWorkerMetrics::RecordEventTimeout(EventType event) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.RequestTimeouts.Count", event);
}

void ServiceWorkerMetrics::RecordEventDuration(EventType event,
                                               base::TimeDelta time,
                                               bool was_handled) {
  switch (event) {
    case EventType::ACTIVATE:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.ActivateEvent.Time", time);
      break;
    case EventType::INSTALL:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.InstallEvent.Time", time);
      break;
    case EventType::FETCH_MAIN_FRAME:
    case EventType::FETCH_SUB_FRAME:
    case EventType::FETCH_SHARED_WORKER:
    case EventType::FETCH_SUB_RESOURCE:
      if (was_handled) {
        UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.FetchEvent.HasResponse.Time",
                                   time);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.FetchEvent.Fallback.Time",
                                   time);
      }
      break;
    case EventType::FETCH_WAITUNTIL:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.FetchEvent.WaitUntil.Time",
                                 time);
      break;
    case EventType::FOREIGN_FETCH:
      if (was_handled) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "ServiceWorker.ForeignFetchEvent.HasResponse.Time", time);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "ServiceWorker.ForeignFetchEvent.Fallback.Time", time);
      }
      break;
    case EventType::FOREIGN_FETCH_WAITUNTIL:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "ServiceWorker.ForeignFetchEvent.WaitUntil.Time", time);
      break;
    case EventType::SYNC:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.BackgroundSyncEvent.Time",
                                 time);
      break;
    case EventType::NOTIFICATION_CLICK:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.NotificationClickEvent.Time",
                                 time);
      break;
    case EventType::NOTIFICATION_CLOSE:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.NotificationCloseEvent.Time",
                                 time);
      break;
    case EventType::PUSH:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.PushEvent.Time", time);
      break;
    case EventType::MESSAGE:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.ExtendableMessageEvent.Time",
                                 time);
      break;
    case EventType::EXTERNAL_REQUEST:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.ExternalRequest.Time", time);
      break;
    case EventType::PAYMENT_REQUEST:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.PaymentRequestEvent.Time",
                                 time);
      break;
    case EventType::BACKGROUND_FETCH_ABORT:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.BackgroundFetchAbortEvent.Time",
                                 time);
      break;
    case EventType::BACKGROUND_FETCH_CLICK:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.BackgroundFetchClickEvent.Time",
                                 time);
      break;
    case EventType::BACKGROUND_FETCH_FAIL:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.BackgroundFetchFailEvent.Time",
                                 time);
      break;
    case EventType::BACKGROUND_FETCH_SUCCESS:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "ServiceWorker.BackgroundFetchSuccessEvent.Time", time);
      break;
    case EventType::CAN_MAKE_PAYMENT:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.CanMakePaymentEvent.Time",
                                 time);
      break;
    case EventType::ABORT_PAYMENT:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.AbortPaymentEvent.Time", time);
      break;
    case EventType::COOKIE_CHANGE:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.CookieChangeEvent.Time", time);
      break;
    case EventType::LONG_RUNNING_MESSAGE:
      // Since this event is expected to last indefinitely we don't need to log
      // how long they actually last.
      break;

    case EventType::NAVIGATION_HINT:
    // The navigation hint should not be sent as an event.
    case EventType::UNKNOWN:
      NOTREACHED() << "Invalid event type";
      break;
  }
}

void ServiceWorkerMetrics::RecordFetchEventStatus(
    bool is_main_resource,
    blink::ServiceWorkerStatusCode status) {
  if (is_main_resource) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.FetchEvent.MainResource.Status",
                              status);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.FetchEvent.Subresource.Status",
                              status);
  }
}

void ServiceWorkerMetrics::RecordURLRequestJobResult(
    bool is_main_resource,
    URLRequestJobResult result) {
  if (is_main_resource) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.URLRequestJob.MainResource.Result",
                              result, NUM_REQUEST_JOB_RESULT_TYPES);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.URLRequestJob.Subresource.Result",
                              result, NUM_REQUEST_JOB_RESULT_TYPES);
  }
}

void ServiceWorkerMetrics::RecordStatusZeroResponseError(
    bool is_main_resource,
    blink::mojom::ServiceWorkerResponseError error) {
  if (is_main_resource) {
    UMA_HISTOGRAM_ENUMERATION(
        "ServiceWorker.URLRequestJob.MainResource.StatusZeroError", error);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "ServiceWorker.URLRequestJob.Subresource.StatusZeroError", error);
  }
}

void ServiceWorkerMetrics::RecordFallbackedRequestMode(
    network::mojom::FetchRequestMode mode) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.URLRequestJob.FallbackedRequestMode",
                            mode);
}

void ServiceWorkerMetrics::RecordProcessCreated(bool is_new_process) {
  UMA_HISTOGRAM_BOOLEAN("EmbeddedWorkerInstance.ProcessCreated",
                        is_new_process);
}

void ServiceWorkerMetrics::RecordStartWorkerTiming(const StartTimes& times,
                                                   StartSituation situation) {
  // Bail if the timings across processes weren't consistent.
  if (!base::TimeTicks::IsHighResolution() ||
      !base::TimeTicks::IsConsistentAcrossProcesses()) {
    RecordStartWorkerTimingClockConsistency(
        CrossProcessTimeDelta::INACCURATE_CLOCK);
    return;
  }
  if (times.remote_start_worker_received < times.local_start_worker_sent ||
      times.local_end < times.remote_script_evaluation_end) {
    RecordStartWorkerTimingClockConsistency(CrossProcessTimeDelta::NEGATIVE);
    return;
  }
  RecordStartWorkerTimingClockConsistency(CrossProcessTimeDelta::NORMAL);

  // Total duration.
  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartTiming.Duration",
                             times.local_end - times.local_start);
  base::UmaHistogramMediumTimes(
      base::StrCat({"ServiceWorker.StartTiming.Duration",
                    StartSituationToSuffix(situation)}),
      times.local_end - times.local_start);

  // SentStartWorker milestone.
  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartTiming.StartToSentStartWorker",
                             times.local_start_worker_sent - times.local_start);

  // ReceivedStartWorker milestone.
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.StartTiming.StartToReceivedStartWorker",
      times.remote_start_worker_received - times.local_start);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.StartTiming.SentStartWorkerToReceivedStartWorker",
      times.remote_start_worker_received - times.local_start_worker_sent);

  // ScriptEvaluationStart milestone.
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.StartTiming.StartToScriptEvaluationStart",
      times.remote_script_evaluation_start - times.local_start);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.StartTiming.ReceivedStartWorkerToScriptEvaluationStart",
      times.remote_script_evaluation_start -
          times.remote_start_worker_received);

  // ScriptEvaluationEnd milestone.
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.StartTiming.StartToScriptEvaluationEnd",
      times.remote_script_evaluation_end - times.local_start);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.StartTiming.ScriptEvaluationStartToScriptEvaluationEnd",
      times.remote_script_evaluation_end -
          times.remote_script_evaluation_start);

  // End milestone.
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.StartTiming.ScriptEvaluationEndToEnd",
      times.local_end - times.remote_script_evaluation_end);
}

void ServiceWorkerMetrics::RecordStartWorkerTimingClockConsistency(
    CrossProcessTimeDelta type) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartTiming.ClockConsistency", type);
}

void ServiceWorkerMetrics::RecordStartStatusAfterFailure(
    int failure_count,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_GT(failure_count, 0);

  if (status == blink::ServiceWorkerStatusCode::kOk) {
    UMA_HISTOGRAM_COUNTS_1000("ServiceWorker.StartWorker.FailureStreakEnded",
                              failure_count);
  } else if (failure_count < std::numeric_limits<int>::max()) {
    UMA_HISTOGRAM_COUNTS_1000("ServiceWorker.StartWorker.FailureStreak",
                              failure_count + 1);
  }

  if (failure_count == 1) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.AfterFailureStreak_1",
                              status);
  } else if (failure_count == 2) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.AfterFailureStreak_2",
                              status);
  } else if (failure_count == 3) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.AfterFailureStreak_3",
                              status);
  }
}

void ServiceWorkerMetrics::RecordNavigationPreloadRequestHeaderSize(
    size_t size) {
  UMA_HISTOGRAM_COUNTS_100000("ServiceWorker.NavigationPreload.HeaderSize",
                              size);
}

void ServiceWorkerMetrics::RecordNavigationPreloadResponse(
    base::TimeDelta worker_start,
    base::TimeDelta response_start,
    EmbeddedWorkerStatus initial_worker_status,
    StartSituation start_situation,
    ResourceType resource_type) {
  DCHECK_GE(worker_start.ToInternalValue(), 0);
  DCHECK_GE(response_start.ToInternalValue(), 0);
  DCHECK(resource_type == RESOURCE_TYPE_MAIN_FRAME ||
         resource_type == RESOURCE_TYPE_SUB_FRAME);
  const bool is_main_frame = (resource_type == RESOURCE_TYPE_MAIN_FRAME);
  // TODO(falken): Log sub-frame navigations also.
  if (!is_main_frame) {
    return;
  }
  const bool nav_preload_finished_first = response_start < worker_start;
  const base::TimeDelta concurrent_time =
      nav_preload_finished_first ? response_start : worker_start;
  base::TimeDelta worker_wait_time;
  if (nav_preload_finished_first) {
    worker_wait_time = worker_start - response_start;
  }
  const bool worker_start_occurred =
      initial_worker_status != EmbeddedWorkerStatus::RUNNING;
  const WorkerPreparationType preparation =
      GetWorkerPreparationType(initial_worker_status, start_situation);

  UMA_HISTOGRAM_ENUMERATION(
      "ServiceWorker.NavPreload.WorkerPreparationType_MainFrame", preparation);
  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.NavPreload.ResponseTime_MainFrame",
                             response_start);
  UMA_HISTOGRAM_BOOLEAN("ServiceWorker.NavPreload.FinishedFirst_MainFrame",
                        nav_preload_finished_first);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.NavPreload.ConcurrentTime_MainFrame", concurrent_time);
  if (nav_preload_finished_first) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame", worker_wait_time);
  }

  if (worker_start_occurred) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "ServiceWorker.NavPreload.ResponseTime_MainFrame_"
        "WorkerStartOccurred",
        response_start);
    UMA_HISTOGRAM_BOOLEAN(
        "ServiceWorker.NavPreload.FinishedFirst_MainFrame_"
        "WorkerStartOccurred",
        nav_preload_finished_first);
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "ServiceWorker.NavPreload.ConcurrentTime_MainFrame_"
        "WorkerStartOccurred",
        concurrent_time);
    if (nav_preload_finished_first) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame_"
          "WorkerStartOccurred",
          worker_wait_time);
    }
  }
}

void ServiceWorkerMetrics::RecordContextRequestHandlerStatus(
    ServiceWorkerContextRequestHandler::CreateJobStatus status,
    bool is_installed,
    bool is_main_script) {
  if (is_installed) {
    if (is_main_script) {
      UMA_HISTOGRAM_ENUMERATION(
          "ServiceWorker.ContextRequestHandlerStatus.InstalledWorker."
          "MainScript",
          status);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "ServiceWorker.ContextRequestHandlerStatus.InstalledWorker."
          "ImportedScript",
          status);
    }
  } else {
    if (is_main_script) {
      UMA_HISTOGRAM_ENUMERATION(
          "ServiceWorker.ContextRequestHandlerStatus.NewWorker.MainScript",
          status);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "ServiceWorker.ContextRequestHandlerStatus.NewWorker."
          "ImportedScript",
          status);
    }
  }
}

void ServiceWorkerMetrics::RecordRuntime(base::TimeDelta time) {
  // Start at 1 second since we expect service worker to last at least this
  // long: the update timer and idle timeout timer run on the order of seconds.
  constexpr base::TimeDelta kMin = base::TimeDelta::FromSeconds(1);
  // End at 1 day since service workers can conceivably run as long as the the
  // browser is open; we have to cap somewhere.
  constexpr base::TimeDelta kMax = base::TimeDelta::FromDays(1);
  // Set the bucket count to 50 since that is the recommended value for all
  // histograms.
  const int kBucketCount = 50;

  UMA_HISTOGRAM_CUSTOM_TIMES("ServiceWorker.Runtime", time, kMin, kMax,
                             kBucketCount);
}

void ServiceWorkerMetrics::RecordStartServiceWorkerForNavigationHintResult(
    StartServiceWorkerForNavigationHintResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartForNavigationHint.Result",
                            result);
}

void ServiceWorkerMetrics::RecordRegisteredOriginCount(size_t origin_count) {
  UMA_HISTOGRAM_COUNTS_1M("ServiceWorker.RegisteredOriginCount", origin_count);
}

void ServiceWorkerMetrics::RecordMainResourceRequestDestination(
    MainResourceRequestDestination destination) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.MainResourceRequestDestination",
                            destination);
}

}  // namespace content
