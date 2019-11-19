// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/event_ack_data.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_external_request_result.h"

namespace extensions {

namespace {
// Information about an unacked event.
//   std::string - GUID of the Service Worker's external request for the event.
//   int - RenderProcessHost id.
using EventInfo = std::pair<std::string, int>;
}  // namespace

// Class that holds map of unacked event information keyed by event id, accessed
// on service worker core thread.
// TODO(crbug.com/824858): This shouldn't be needed after service worker core
// thread moves to the UI thread.
// TODO(lazyboy): Could this potentially be owned exclusively (and deleted) on
// the core thread, thus not requiring RefCounted?
class EventAckData::CoreThreadEventInfo
    : public base::RefCountedThreadSafe<CoreThreadEventInfo> {
 public:
  CoreThreadEventInfo() = default;

  // Map of event information keyed by event_id.
  std::map<int, EventInfo> event_map;

 private:
  friend class base::RefCountedThreadSafe<CoreThreadEventInfo>;
  ~CoreThreadEventInfo() = default;

  DISALLOW_COPY_AND_ASSIGN(CoreThreadEventInfo);
};

// static
void EventAckData::StartExternalRequestOnCoreThread(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id,
    scoped_refptr<CoreThreadEventInfo> unacked_events) {
  DCHECK_CURRENTLY_ON(content::ServiceWorkerContext::GetCoreThreadId());

  std::string request_uuid = base::GenerateGUID();

  content::ServiceWorkerExternalRequestResult result =
      context->StartingExternalRequest(version_id, request_uuid);
  if (result != content::ServiceWorkerExternalRequestResult::kOk) {
    LOG(ERROR) << "StartExternalRequest failed: " << static_cast<int>(result);
    return;
  }

  // TODO(lazyboy): Clean up |unacked_events_| if RenderProcessHost died before
  // it got a chance to ack |event_id|. This shouldn't happen in common cases.
  std::map<int, EventInfo>& unacked_events_map = unacked_events->event_map;
  auto insert_result = unacked_events_map.insert(std::make_pair(
      event_id, std::make_pair(request_uuid, render_process_id)));
  DCHECK(insert_result.second) << "EventAckData: Duplicate event_id.";
}

// static
void EventAckData::FinishExternalRequestOnCoreThread(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id,
    bool worker_stopped,
    scoped_refptr<CoreThreadEventInfo> unacked_events,
    base::OnceClosure failure_callback) {
  DCHECK_CURRENTLY_ON(content::ServiceWorkerContext::GetCoreThreadId());

  std::map<int, EventInfo>& unacked_events_map = unacked_events->event_map;
  auto request_info_iter = unacked_events_map.find(event_id);
  if (request_info_iter == unacked_events_map.end() ||
      request_info_iter->second.second != render_process_id) {
    std::move(failure_callback).Run();
    return;
  }

  std::string request_uuid = std::move(request_info_iter->second.first);
  unacked_events_map.erase(request_info_iter);

  content::ServiceWorkerExternalRequestResult result =
      context->FinishedExternalRequest(version_id, request_uuid);
  // If the worker was already stopped, the FinishedExternalRequest will
  // legitimately fail.
  if (worker_stopped)
    return;

  if (result != content::ServiceWorkerExternalRequestResult::kOk) {
    LOG(ERROR) << "FinishExternalRequest failed: " << static_cast<int>(result);
    std::move(failure_callback).Run();
  }
}

EventAckData::EventAckData()
    : unacked_events_(base::MakeRefCounted<CoreThreadEventInfo>()) {}
EventAckData::~EventAckData() = default;

void EventAckData::IncrementInflightEvent(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (content::ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    StartExternalRequestOnCoreThread(context, render_process_id, version_id,
                                     event_id, unacked_events_);
  } else {
    content::ServiceWorkerContext::RunTask(
        base::CreateSingleThreadTaskRunner({content::BrowserThread::IO}),
        FROM_HERE, context,
        base::BindOnce(&EventAckData::StartExternalRequestOnCoreThread, context,
                       render_process_id, version_id, event_id,
                       unacked_events_));
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

  if (content::ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    FinishExternalRequestOnCoreThread(context, render_process_id, version_id,
                                      event_id, worker_stopped, unacked_events_,
                                      std::move(failure_callback));
  } else {
    content::ServiceWorkerContext::RunTask(
        base::CreateSingleThreadTaskRunner({content::BrowserThread::IO}),
        FROM_HERE, context,
        base::BindOnce(&EventAckData::FinishExternalRequestOnCoreThread,
                       context, render_process_id, version_id, event_id,
                       worker_stopped, unacked_events_,
                       std::move(failure_callback)));
  }
}

}  // namespace extensions
