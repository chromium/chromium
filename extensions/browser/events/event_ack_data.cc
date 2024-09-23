// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/event_ack_data.h"

#include <string>
#include <utility>

#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/uuid.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_external_request_result.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace extensions {

namespace {

// static
// Emit metrics helpful in determining causes of `unacked_events_` that are not
// acked within the timeout.
void EmitLateAckedEventTaskMetrics(const EventAckData::EventInfo& event_info) {
  base::UmaHistogramEnumeration(
      "Extensions.Events.ServiceWorkerDispatchFailed.Event",
      event_info.histogram_value, events::ENUM_BOUNDARY);

  base::UmaHistogramBoolean(
      "Extensions.Events.ServiceWorkerDispatchFailed.StartExternalRequestOk",
      event_info.start_ok);
  if (!event_info.start_ok) {
    base::UmaHistogramEnumeration(
        "Extensions.Events.ServiceWorkerDispatchFailed."
        "StartExternalRequestResult",
        event_info.external_request_result);
  }

  // TODO(crbug.com/40909770): Implement service worker running status as a late
  // acked event metric when it can be more accurately determined. For example,
  // it could be useful to determine if the late acked events are for already
  // shut down workers and therefore wouldn't be "late".
}

}  // namespace

EventAckData::EventInfo::EventInfo(
    const base::Uuid& request_uuid,
    int render_process_id,
    bool start_ok,
    content::ServiceWorkerExternalRequestResult external_request_result,
    base::TimeTicks dispatch_start_time,
    EventDispatchSource dispatch_source,
    bool lazy_background_active_on_dispatch,
    const events::HistogramValue histogram_value)
    : request_uuid(request_uuid),
      render_process_id(render_process_id),
      start_ok(start_ok),
      external_request_result(external_request_result),
      dispatch_start_time(dispatch_start_time),
      dispatch_source(dispatch_source),
      lazy_background_active_on_dispatch(lazy_background_active_on_dispatch),
      histogram_value(histogram_value) {}

EventAckData::EventInfo::EventInfo(EventInfo&& other) = default;

EventAckData::EventAckData() = default;

EventAckData::~EventAckData() = default;

void EventAckData::IncrementInflightEvent(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id,
    base::TimeTicks dispatch_start_time,
    EventDispatchSource dispatch_source,
    bool lazy_background_active_on_dispatch,
    events::HistogramValue histogram_value) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Uuid request_uuid = base::Uuid::GenerateRandomV4();
  bool start_ok = true;

  content::ServiceWorkerExternalRequestResult external_request_result =
      context->StartingExternalRequest(
          version_id,
          content::ServiceWorkerExternalRequestTimeoutType::kDefault,
          request_uuid);
  base::UmaHistogramEnumeration(
      "Extensions.ServiceWorkerBackground.StartingExternalRequest_Result",
      external_request_result);
  if (external_request_result !=
      content::ServiceWorkerExternalRequestResult::kOk) {
    LOG(ERROR) << "StartExternalRequest failed: "
               << static_cast<int>(external_request_result);
    start_ok = false;
  }

  auto insert_result = unacked_events_.try_emplace(
      event_id,
      EventInfo{request_uuid, render_process_id, start_ok,
                external_request_result, dispatch_start_time, dispatch_source,
                lazy_background_active_on_dispatch, histogram_value});
  DCHECK(insert_result.second) << "EventAckData: Duplicate event_id.";

  if (dispatch_source == EventDispatchSource::kDispatchEventToProcess) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&EventAckData::EmitLateAckedEventTask,
                       weak_factory_.GetWeakPtr(), event_id),
        kEventAckMetricTimeLimit);
  }
}

void EventAckData::EmitLateAckedEventTask(int event_id) {
  // If the event is still present then we haven't received the ack yet in
  // `EventAckData::DecrementInflightEvent()`.
  if (auto* value = base::FindOrNull(unacked_events_, event_id)) {
    base::UmaHistogramBoolean(
        "Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker3",
        false);
    EmitLateAckedEventTaskMetrics(*value);
  }
}

// static
void EventAckData::EmitDispatchTimeMetrics(EventInfo& event_info) {
  // Only emit events that use the EventRouter::DispatchEventToProcess() event
  // routing flow since EventRouter::DispatchEventToSender() uses a different
  // flow that doesn't include dispatch start and service worker start time.
  if (event_info.dispatch_source ==
      EventDispatchSource::kDispatchEventToProcess) {
    base::UmaHistogramCustomMicrosecondsTimes(
        "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
        /*sample=*/base::TimeTicks::Now() - event_info.dispatch_start_time,
        /*min=*/base::Microseconds(1), /*max=*/base::Minutes(5),
        /*buckets=*/100);
    const char* active_metric_name =
        event_info.lazy_background_active_on_dispatch
            ? "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2."
              "Active3"
            : "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2."
              "Inactive3";
    base::UmaHistogramCustomMicrosecondsTimes(
        active_metric_name,
        /*sample=*/base::TimeTicks::Now() - event_info.dispatch_start_time,
        /*min=*/base::Microseconds(1), /*max=*/base::Minutes(5),
        /*buckets=*/100);

    base::UmaHistogramCustomTimes(
        "Extensions.Events.DispatchToAckLongTime.ExtensionServiceWorker2",
        /*sample=*/base::TimeTicks::Now() - event_info.dispatch_start_time,
        /*min=*/base::Seconds(1), /*max=*/base::Days(1),
        /*buckets=*/100);

    // Emit only if we're within the expected event ack time limit. We'll take
    // care of the emit for a late ack via a delayed task.
    bool late_ack = (base::TimeTicks::Now() - event_info.dispatch_start_time) >
                    kEventAckMetricTimeLimit;
    if (!late_ack) {
      base::UmaHistogramBoolean(
          "Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker3",
          true);
    }
  }
}

void EventAckData::DecrementInflightEvent(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id,
    bool worker_stopped,
    base::OnceClosure failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto request_info_iter = unacked_events_.find(event_id);
  if (request_info_iter == unacked_events_.end() ||
      request_info_iter->second.render_process_id != render_process_id) {
    std::move(failure_callback).Run();
    return;
  }

  EventInfo& event_info = request_info_iter->second;

  EmitDispatchTimeMetrics(event_info);

  base::Uuid request_uuid = std::move(event_info.request_uuid);
  bool start_ok = event_info.start_ok;
  unacked_events_.erase(request_info_iter);

  content::ServiceWorkerExternalRequestResult result =
      context->FinishedExternalRequest(version_id, request_uuid);
  base::UmaHistogramEnumeration(
      "Extensions.ServiceWorkerBackground.FinishedExternalRequest_Result",
      result);
  // If the worker was already stopped or StartExternalRequest didn't succeed,
  // the FinishedExternalRequest will legitimately fail.
  if (worker_stopped || !start_ok) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Extensions.ServiceWorkerBackground.FinishedExternalRequest_Result_"
      "PostReturn",
      result);

  switch (result) {
    case content::ServiceWorkerExternalRequestResult::kOk:
    // Metrics have shown us that it is possible that a worker may not be found
    // or not running at this point.
    case content::ServiceWorkerExternalRequestResult::kWorkerNotFound:
    case content::ServiceWorkerExternalRequestResult::kWorkerNotRunning:
    // Null context can happen in the rare case if ServiceWorkerContextCore is
    // torn down when EventRouter + BrowserContext are still alive and an
    // event happens to be acked here.
    case content::ServiceWorkerExternalRequestResult::kNullContext:
      // TODO(crbug.com/41494056): Perform more graceful shutdown when
      // ServiceWorkerContextCore is torn down.

    // kBadRequestId can expectedly happen if a new instance of a worker starts
    // while an ack for the previous worker is in-flight to the browser. We then
    // receive the ack and ServiceWorkerContext can't find the
    // external/in-flight request because the previous worker's
    // `ServiceWorkerVersion` has been replaced by the new worker's
    // `ServiceWorkerVersion`. The new version then does not have a record of
    // the external/in-flight request and returns kBadRequestId.
    case content::ServiceWorkerExternalRequestResult::kBadRequestId:
      // TODO(crbug.com/40072982): Reliably detect when the above occurs and
      // continue to not kill the renderer. But if the event is not for an old
      // instance of the worker then consider CHECK()-ing since this could
      // indicate a bug in the tracking of external requests in the browser.
      break;
  }
}

void EventAckData::ClearUnackedEventsForRenderProcess(int render_process_id) {
  std::erase_if(unacked_events_, [render_process_id](const auto& entry) {
    return entry.second.render_process_id == render_process_id;
  });
}

EventAckData::EventInfo* EventAckData::GetUnackedEventForTesting(int event_id) {
  return base::FindOrNull(unacked_events_, event_id);
}

}  // namespace extensions
