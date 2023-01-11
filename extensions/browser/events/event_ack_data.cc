// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/event_ack_data.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/guid.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_external_request_result.h"

namespace extensions {

namespace {
// Information about an unacked event.
struct EventInfo {
  // GUID of the Service Worker's external request for the event.
  std::string request_uuid;
  // RenderProcessHost id.
  int render_process_id;
  // Whether or not StartExternalRequest succeeded.
  bool start_ok;
};
}  // namespace

// Class that holds map of unacked event information keyed by event id, accessed
// on the UI thread.
// TODO(crbug.com/824858): This shouldn't be needed since ServiceWorkerContext
// moved to the UI thread.
// TODO(lazyboy): Could this potentially be owned exclusively (and deleted) on
// the UI thread, thus not requiring RefCounted?
class EventAckData::CoreThreadEventInfo
    : public base::RefCountedThreadSafe<CoreThreadEventInfo> {
 public:
  CoreThreadEventInfo() = default;

  CoreThreadEventInfo(const CoreThreadEventInfo&) = delete;
  CoreThreadEventInfo& operator=(const CoreThreadEventInfo&) = delete;

  // Map of event information keyed by event_id.
  std::map<int, EventInfo> event_map;

 private:
  friend class base::RefCountedThreadSafe<CoreThreadEventInfo>;
  ~CoreThreadEventInfo() = default;
};

EventAckData::EventAckData()
    : unacked_events_(base::MakeRefCounted<CoreThreadEventInfo>()) {}
EventAckData::~EventAckData() = default;

void EventAckData::IncrementInflightEvent(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string request_uuid = base::GenerateGUID();
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
  std::map<int, EventInfo>& unacked_events_map = unacked_events_->event_map;
  auto insert_result = unacked_events_map.insert(std::make_pair(
      event_id, EventInfo{request_uuid, render_process_id, start_ok}));
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

  std::map<int, EventInfo>& unacked_events_map = unacked_events_->event_map;
  auto request_info_iter = unacked_events_map.find(event_id);
  if (request_info_iter == unacked_events_map.end() ||
      request_info_iter->second.render_process_id != render_process_id) {
    std::move(failure_callback).Run();
    return;
  }

  std::string request_uuid = std::move(request_info_iter->second.request_uuid);
  bool start_ok = request_info_iter->second.start_ok;
  unacked_events_map.erase(request_info_iter);

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
