// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_metrics.h"

#include <limits>
#include <string>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"

namespace content {

namespace {

const char* StartSituationToSuffix(
    ServiceWorkerMetrics::StartSituation situation) {
  // Don't change these returned strings. They are written (in hashed form) into
  // logs.
  switch (situation) {
    case ServiceWorkerMetrics::StartSituation::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION() << static_cast<int>(situation);
  return ".Unknown";
}

// TODO(falken): Remove this when the associated UMA are removed.
const char* StartSituationToDeprecatedSuffix(
    ServiceWorkerMetrics::StartSituation situation) {
  // Don't change this returned string. It is written (in hashed form) into
  // logs.
  switch (situation) {
    case ServiceWorkerMetrics::StartSituation::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION() << static_cast<int>(situation);
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
    case ServiceWorkerMetrics::EventType::FETCH_WAITUNTIL:
      return "_FETCH_WAITUNTIL";
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
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_SUCCESS:
      return "_BACKGROUND_FETCH_SUCCESS";
    case ServiceWorkerMetrics::EventType::PERIODIC_SYNC:
      return "_PERIODIC_SYNC";
    case ServiceWorkerMetrics::EventType::CONTENT_DELETE:
      return "_CONTENT_DELETE";
    case ServiceWorkerMetrics::EventType::PUSH_SUBSCRIPTION_CHANGE:
      return "_PUSH_SUBSCRIPTION_CHANGE";
    case ServiceWorkerMetrics::EventType::FETCH_FENCED_FRAME:
      return "_FETCH_FENCED_FRAME";
    case ServiceWorkerMetrics::EventType::BYPASS_MAIN_RESOURCE:
      return "_BYPASS_MAIN_RESOURCE";
    case ServiceWorkerMetrics::EventType::SKIP_EMPTY_FETCH_HANDLER:
      return "_SKIP_EMPTY_FETCH_HANDLER";
    case ServiceWorkerMetrics::EventType::
        BYPASS_ONLY_IF_SERVICE_WORKER_NOT_STARTED:
      return "_BYPASS_ONLY_IF_SERVICE_WORKER_NOT_STARTED";
    case ServiceWorkerMetrics::EventType::WARM_UP:
      return "_WARM_UP";
    case ServiceWorkerMetrics::EventType::STATIC_ROUTER:
      return "_STATIC_ROUTING";
  }
  return "_UNKNOWN";
}

}  // namespace

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
    case EventType::FETCH_WAITUNTIL:
      return "Fetch WaitUntil";
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
    case EventType::BACKGROUND_FETCH_SUCCESS:
      return "Background Fetch Success";
    case EventType::PERIODIC_SYNC:
      return "Periodic Sync";
    case EventType::CONTENT_DELETE:
      return "Content Delete";
    case EventType::PUSH_SUBSCRIPTION_CHANGE:
      return "Push Subscription Change";
    case EventType::FETCH_FENCED_FRAME:
      return "Fetch Fenced Frame";
    case ServiceWorkerMetrics::EventType::BYPASS_MAIN_RESOURCE:
      return "_BYPASS_MAIN_RESOURCE";
    case ServiceWorkerMetrics::EventType::SKIP_EMPTY_FETCH_HANDLER:
      return "Skip Empty Fetch Handler";
    case ServiceWorkerMetrics::EventType::
        BYPASS_ONLY_IF_SERVICE_WORKER_NOT_STARTED:
      return "Bypass Only If ServiceWorker Is Not Started";
    case ServiceWorkerMetrics::EventType::WARM_UP:
      return "Warm Up";
    case ServiceWorkerMetrics::EventType::STATIC_ROUTER:
      return "Static Routing";
  }
  NOTREACHED_IN_MIGRATION()
      << "Got unexpected event type: " << static_cast<int>(event_type);
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
  }
  NOTREACHED_IN_MIGRATION() << "Got unexpected start situation: "
                            << static_cast<int>(start_situation);
  return "error";
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

void ServiceWorkerMetrics::RecordStartInstalledWorkerStatus(
    blink::ServiceWorkerStatusCode status,
    EventType purpose) {
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

void ServiceWorkerMetrics::RecordRunAfterStartWorkerStatus(
    blink::EmbeddedWorkerStatus running_status,
    EventType purpose) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.MaybeStartWorker.RunningStatus",
                            running_status);
  base::UmaHistogramEnumeration(
      base::StrCat({"ServiceWorker.MaybeStartWorker.RunningStatusByPurpose",
                    EventTypeToSuffix(purpose)}),
      running_status);
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.MaybeStartWorker.Purpose", purpose);
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
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {"ServiceWorker.StartWorker.Time_Any", EventTypeToSuffix(purpose)}),
        time);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartNewWorker.Time", time);
  }
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
    blink::ServiceWorkerStatusCode status,
    uint32_t fetch_count) {
  base::UmaHistogramEnumeration("ServiceWorker.InstallEvent.All.Status",
                                status);
  base::UmaHistogramCounts1000("ServiceWorker.InstallEvent.All.FetchCount",
                               fetch_count);
  if (fetch_count > 0) {
    base::UmaHistogramEnumeration("ServiceWorker.InstallEvent.WithFetch.Status",
                                  status);
  }
}

void ServiceWorkerMetrics::RecordEventDuration(EventType event,
                                               base::TimeDelta time,
                                               bool was_handled,
                                               uint32_t fetch_count) {
  switch (event) {
    case EventType::ACTIVATE:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.ActivateEvent.Time", time);
      break;
    case EventType::INSTALL:
      base::UmaHistogramMediumTimes("ServiceWorker.InstallEvent.All.Time",
                                    time);
      if (fetch_count) {
        base::UmaHistogramMediumTimes(
            "ServiceWorker.InstallEvent.WithFetch.Time", time);
      }
      break;
    case EventType::MESSAGE:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.ExtendableMessageEvent.Time",
                                 time);
      break;
    case EventType::FETCH_MAIN_FRAME:
    case EventType::FETCH_SUB_FRAME:
    case EventType::FETCH_SHARED_WORKER:
    case EventType::FETCH_SUB_RESOURCE:
    case EventType::FETCH_FENCED_FRAME:
      if (was_handled) {
        UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.FetchEvent.HasResponse.Time",
                                   time);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.FetchEvent.Fallback.Time",
                                   time);
      }
      break;
    case EventType::PAYMENT_REQUEST:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.PaymentRequestEvent.Time",
                                 time);
      break;
    case EventType::CAN_MAKE_PAYMENT:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.CanMakePaymentEvent.Time",
                                 time);
      break;
    case EventType::ABORT_PAYMENT:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.AbortPaymentEvent.Time", time);
      break;
    case EventType::PERIODIC_SYNC:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "ServiceWorker.PeriodicBackgroundSyncEvent.Time", time);
      break;
    case EventType::SYNC:
    case EventType::NOTIFICATION_CLICK:
    case EventType::PUSH:
    case EventType::NOTIFICATION_CLOSE:
    case EventType::FETCH_WAITUNTIL:
    case EventType::EXTERNAL_REQUEST:
    case EventType::BACKGROUND_FETCH_ABORT:
    case EventType::BACKGROUND_FETCH_CLICK:
    case EventType::BACKGROUND_FETCH_FAIL:
    case EventType::COOKIE_CHANGE:
    case EventType::BACKGROUND_FETCH_SUCCESS:
    case EventType::CONTENT_DELETE:
    case EventType::PUSH_SUBSCRIPTION_CHANGE:
    case EventType::WARM_UP:
      // Do nothing: the warm up should not be sent as an event.
      break;
    case EventType::NAVIGATION_HINT:
    // The navigation hint should not be sent as an event.
    case EventType::BYPASS_MAIN_RESOURCE:
    // The bypass main resource should not be sent as an event.
    case EventType::SKIP_EMPTY_FETCH_HANDLER:
    // The skip empty fetch handler should not be sent as an event.
    case EventType::BYPASS_ONLY_IF_SERVICE_WORKER_NOT_STARTED:
    // The bypass_only_if_service_worker_not_started should not be sent as an
    // event.
    case EventType::STATIC_ROUTER:
    // Static Routing should not be sent as an event.
    case EventType::UNKNOWN:
      NOTREACHED_IN_MIGRATION() << "Invalid event type";
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

void ServiceWorkerMetrics::RecordSkipServiceWorkerOnNavigation(
    bool skip_service_worker) {
  static bool is_first_call = true;
  if (is_first_call) {
    is_first_call = false;
    if (!GetContentClient()->browser()->IsBrowserStartupComplete()) {
      base::UmaHistogramBoolean(
          "ServiceWorker.OnBrowserStartup.SkipServiceWorkerOnFirstNavigation",
          skip_service_worker);
    }
  } else {
    if (GetContentClient()->browser()->IsBrowserStartupComplete()) {
      base::UmaHistogramBoolean(
          "ServiceWorker.SkipCallingFindRegistrationForClientUrl",
          skip_service_worker);
    }
  }
}

void ServiceWorkerMetrics::RecordFindRegistrationForClientUrlTime(
    base::TimeDelta time) {
  static bool is_first_call = true;
  if (is_first_call) {
    is_first_call = false;
    if (!GetContentClient()->browser()->IsBrowserStartupComplete()) {
      base::UmaHistogramMediumTimes(
          "ServiceWorker.OnBrowserStartup.FirstFindRegistrationForClientUrl."
          "Time",
          time);
    }
  } else {
    if (GetContentClient()->browser()->IsBrowserStartupComplete()) {
      base::UmaHistogramMediumTimes(
          "ServiceWorker.FindRegistrationForClientUrl.Time", time);
    }
  }
}

}  // namespace content
