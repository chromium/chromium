// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/event_ack_data.h"

#include "base/callback.h"
#include "base/guid.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"

namespace extensions {

namespace {
// Information about an unacked event.
//   std::string - GUID of the Service Worker's external request for the event.
//   int - RenderProcessHost id.
using EventInfo = std::pair<std::string, int>;
}  // namespace

// Class that holds map of unacked event information keyed by event id, accessed
// on IO thread.
// TODO(lazyboy): Could this potentially be owned exclusively (and deleted) on
// the IO thread, thus not requiring RefCounted?
class EventAckData::IOEventInfo
    : public base::RefCountedThreadSafe<IOEventInfo> {
 public:
  IOEventInfo() = default;

  // Map of event information keyed by event_id.
  std::map<int, EventInfo> event_map;

 private:
  friend class base::RefCountedThreadSafe<IOEventInfo>;
  ~IOEventInfo() = default;

  DISALLOW_COPY_AND_ASSIGN(IOEventInfo);
};

// static
void EventAckData::StartExternalRequestOnIO(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id,
    scoped_refptr<IOEventInfo> unacked_events) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::string request_uuid = base::GenerateGUID();

  if (!context->StartingExternalRequest(version_id, request_uuid)) {
    LOG(ERROR) << "StartExternalRequest failed";
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
void EventAckData::FinishExternalRequestOnIO(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id,
    scoped_refptr<IOEventInfo> unacked_events,
    base::OnceClosure failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::map<int, EventInfo>& unacked_events_map = unacked_events->event_map;
  auto request_info_iter = unacked_events_map.find(event_id);
  if (request_info_iter == unacked_events_map.end() ||
      request_info_iter->second.second != render_process_id) {
    std::move(failure_callback).Run();
    return;
  }

  std::string request_uuid = std::move(request_info_iter->second.first);
  unacked_events_map.erase(request_info_iter);

  if (!context->FinishedExternalRequest(version_id, request_uuid))
    std::move(failure_callback).Run();
}

EventAckData::EventAckData()
    : unacked_events_(base::MakeRefCounted<IOEventInfo>()),
      weak_factory_(this) {}
EventAckData::~EventAckData() = default;

void EventAckData::IncrementInflightEvent(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::ServiceWorkerContext::RunTask(
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::IO}),
      FROM_HERE, context,
      base::BindOnce(&EventAckData::StartExternalRequestOnIO, context,
                     render_process_id, version_id, event_id, unacked_events_));
}

void EventAckData::DecrementInflightEvent(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id,
    base::OnceClosure failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::ServiceWorkerContext::RunTask(
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::IO}),
      FROM_HERE, context,
      base::BindOnce(&EventAckData::FinishExternalRequestOnIO, context,
                     render_process_id, version_id, event_id, unacked_events_,
                     std::move(failure_callback)));
}

}  // namespace extensions
