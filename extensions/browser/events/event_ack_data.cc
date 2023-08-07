// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/event_ack_data.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/uuid.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_external_request_result.h"
#include "extensions/browser/event_router.h"

namespace extensions {

EventAckData::EventAckData() = default;

EventAckData::~EventAckData() = default;

void EventAckData::IncrementInflightEvent(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id,
    base::TimeTicks dispatch_start_time,
    EventDispatchSource dispatch_source) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Uuid request_uuid = base::Uuid::GenerateRandomV4();
  bool start_ok = true;

  content::ServiceWorkerExternalRequestResult result =
      context->StartingExternalRequest(
          version_id,
          content::ServiceWorkerExternalRequestTimeoutType::kDefault,
          request_uuid);
  if (result != content::ServiceWorkerExternalRequestResult::kOk) {
    LOG(ERROR) << "StartExternalRequest failed: " << static_cast<int>(result);
    start_ok = false;
  }

  // TODO(lazyboy): Clean up |unacked_events_| if RenderProcessHost died before
  // it got a chance to ack |event_id|. This shouldn't happen in common cases.
  auto insert_result = unacked_events_.try_emplace(
      event_id, EventInfo{request_uuid, render_process_id, start_ok,
                          dispatch_start_time, dispatch_source});
  DCHECK(insert_result.second) << "EventAckData: Duplicate event_id.";
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

  // Only emit events that use the EventRouter::DispatchEventToProcess() event
  // routing flow since EventRouter::DispatchEventToSender() uses a different
  // flow that doesn't include dispatch start and service worker start time.
  if (event_info.dispatch_source ==
      EventDispatchSource::kDispatchEventToProcess) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
        /*time=*/base::TimeTicks::Now() - event_info.dispatch_start_time,
        /*minimum=*/base::Microseconds(1), /*maximum=*/base::Minutes(5),
        /*bucket_count=*/100);

    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Extensions.Events.DispatchToAckLongTime.ExtensionServiceWorker2",
        /*time=*/base::TimeTicks::Now() - event_info.dispatch_start_time,
        /*minimum=*/base::Seconds(1), /*maximum=*/base::Days(1),
        /*bucket_count=*/100);
  }

  base::Uuid request_uuid = std::move(event_info.request_uuid);
  bool start_ok = event_info.start_ok;
  unacked_events_.erase(request_info_iter);

  content::ServiceWorkerExternalRequestResult result =
      context->FinishedExternalRequest(version_id, request_uuid);
  // If the worker was already stopped or StartExternalRequest didn't succeed,
  // the FinishedExternalRequest will legitimately fail.
  if (worker_stopped || !start_ok)
    return;

  if (result != content::ServiceWorkerExternalRequestResult::kOk) {
    LOG(ERROR) << "FinishExternalRequest failed: " << static_cast<int>(result);
    std::move(failure_callback).Run();
  }
}

}  // namespace extensions
