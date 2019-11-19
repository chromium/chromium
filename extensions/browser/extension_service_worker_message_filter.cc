// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_service_worker_message_filter.h"

#include "base/bind.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_external_request_result.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/events/event_ack_data.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker_task_queue.h"
#include "extensions/common/extension_messages.h"

namespace extensions {

ExtensionServiceWorkerMessageFilter::ExtensionServiceWorkerMessageFilter(
    int render_process_id,
    content::BrowserContext* context,
    content::ServiceWorkerContext* service_worker_context)
    : content::BrowserMessageFilter(ExtensionWorkerMsgStart),
      browser_context_(context),
      render_process_id_(render_process_id),
      service_worker_context_(service_worker_context),
      dispatcher_(new ExtensionFunctionDispatcher(context)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ExtensionServiceWorkerMessageFilter::~ExtensionServiceWorkerMessageFilter() {}

void ExtensionServiceWorkerMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
  if (message.type() == ExtensionHostMsg_RequestWorker::ID ||
      message.type() == ExtensionHostMsg_EventAckWorker::ID ||
      message.type() ==
          ExtensionHostMsg_DidInitializeServiceWorkerContext::ID ||
      message.type() == ExtensionHostMsg_DidStartServiceWorkerContext::ID ||
      message.type() == ExtensionHostMsg_DidStopServiceWorkerContext::ID) {
    *thread = content::BrowserThread::UI;
  }

  if (content::ServiceWorkerContext::IsServiceWorkerOnUIEnabled() &&
      (message.type() == ExtensionHostMsg_IncrementServiceWorkerActivity::ID ||
       message.type() == ExtensionHostMsg_DecrementServiceWorkerActivity::ID)) {
    *thread = content::BrowserThread::UI;
  }
}

bool ExtensionServiceWorkerMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionServiceWorkerMessageFilter, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RequestWorker, OnRequestWorker)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_IncrementServiceWorkerActivity,
                        OnIncrementServiceWorkerActivity)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_DecrementServiceWorkerActivity,
                        OnDecrementServiceWorkerActivity)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_EventAckWorker, OnEventAckWorker)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_DidInitializeServiceWorkerContext,
                        OnDidInitializeServiceWorkerContext)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_DidStartServiceWorkerContext,
                        OnDidStartServiceWorkerContext)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_DidStopServiceWorkerContext,
                        OnDidStopServiceWorkerContext)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionServiceWorkerMessageFilter::OnRequestWorker(
    const ExtensionHostMsg_Request_Params& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dispatcher_->Dispatch(params, nullptr, render_process_id_);
}

void ExtensionServiceWorkerMessageFilter::OnIncrementServiceWorkerActivity(
    int64_t service_worker_version_id,
    const std::string& request_uuid) {
  DCHECK_CURRENTLY_ON(content::ServiceWorkerContext::GetCoreThreadId());
  active_request_uuids_.insert(request_uuid);
  // The worker might have already stopped before we got here, so the increment
  // below might fail legitimately. Therefore, we do not send bad_message to the
  // worker even if it fails.
  service_worker_context_->StartingExternalRequest(service_worker_version_id,
                                                   request_uuid);
}

void ExtensionServiceWorkerMessageFilter::OnDecrementServiceWorkerActivity(
    int64_t service_worker_version_id,
    const std::string& request_uuid) {
  DCHECK_CURRENTLY_ON(content::ServiceWorkerContext::GetCoreThreadId());
  content::ServiceWorkerExternalRequestResult result =
      service_worker_context_->FinishedExternalRequest(
          service_worker_version_id, request_uuid);
  if (result != content::ServiceWorkerExternalRequestResult::kOk) {
    LOG(ERROR) << "ServiceWorkerContext::FinishedExternalRequest failed: "
               << static_cast<int>(result);
  }

  bool erased = active_request_uuids_.erase(request_uuid) == 1;
  // The worker may have already stopped before we got here, so only report
  // a bad message if we didn't have an increment for the UUID.
  if (!erased) {
    bad_message::ReceivedBadMessage(
        this, bad_message::ESWMF_INVALID_DECREMENT_ACTIVITY);
  }
}

void ExtensionServiceWorkerMessageFilter::OnEventAckWorker(
    const ExtensionId& extension_id,
    int64_t service_worker_version_id,
    int worker_thread_id,
    int event_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const bool worker_stopped =
      !ProcessManager::Get(browser_context_)
           ->HasServiceWorker({extension_id, render_process_id_,
                               service_worker_version_id, worker_thread_id});
  EventRouter::Get(browser_context_)
      ->event_ack_data()
      ->DecrementInflightEvent(
          service_worker_context_, render_process_id_,
          service_worker_version_id, event_id, worker_stopped,
          base::BindOnce(&ExtensionServiceWorkerMessageFilter::
                             DidFailDecrementInflightEvent,
                         this));
}

void ExtensionServiceWorkerMessageFilter::OnDidInitializeServiceWorkerContext(
    const ExtensionId& extension_id,
    int64_t service_worker_version_id,
    int thread_id) {
  if (!ProcessMap::Get(browser_context_)
           ->Contains(extension_id, render_process_id_)) {
    // We can legitimately get here if the extension was already unloaded.
    return;
  }
  ServiceWorkerTaskQueue::Get(browser_context_)
      ->DidInitializeServiceWorkerContext(render_process_id_, extension_id,
                                          service_worker_version_id, thread_id);
}

void ExtensionServiceWorkerMessageFilter::OnDidStartServiceWorkerContext(
    const ExtensionId& extension_id,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(kMainThreadId, thread_id);
  if (!ProcessMap::Get(browser_context_)
           ->Contains(extension_id, render_process_id_)) {
    // We can legitimately get here if the extension was already unloaded.
    return;
  }
  CHECK(service_worker_scope.SchemeIs(kExtensionScheme) &&
        extension_id == service_worker_scope.host_piece());

  ServiceWorkerTaskQueue::Get(browser_context_)
      ->DidStartServiceWorkerContext(render_process_id_, extension_id,
                                     service_worker_scope,
                                     service_worker_version_id, thread_id);
}

void ExtensionServiceWorkerMessageFilter::OnDidStopServiceWorkerContext(
    const ExtensionId& extension_id,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(kMainThreadId, thread_id);
  if (!ProcessMap::Get(browser_context_)
           ->Contains(extension_id, render_process_id_)) {
    // We can legitimately get here if the extension was already unloaded.
    return;
  }
  CHECK(service_worker_scope.SchemeIs(kExtensionScheme) &&
        extension_id == service_worker_scope.host_piece());

  ServiceWorkerTaskQueue::Get(browser_context_)
      ->DidStopServiceWorkerContext(render_process_id_, extension_id,
                                    service_worker_scope,
                                    service_worker_version_id, thread_id);
}

void ExtensionServiceWorkerMessageFilter::DidFailDecrementInflightEvent() {
  bad_message::ReceivedBadMessage(this, bad_message::ESWMF_BAD_EVENT_ACK);
}

}  // namespace extensions
