// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_service_worker_message_filter.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_external_request_result.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/events/event_ack_data.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/service_worker_task_queue.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/mojom/frame.mojom.h"

namespace extensions {

namespace {

class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  ShutdownNotifierFactory(const ShutdownNotifierFactory&) = delete;
  ShutdownNotifierFactory& operator=(const ShutdownNotifierFactory&) = delete;

  static ShutdownNotifierFactory* GetInstance() {
    return base::Singleton<ShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "ExtensionServiceWorkerMessageFilter") {
    DependsOn(ExtensionRegistryFactory::GetInstance());
    DependsOn(EventRouterFactory::GetInstance());
    DependsOn(ProcessManagerFactory::GetInstance());
  }
  ~ShutdownNotifierFactory() override = default;
};

}  // namespace

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
  shutdown_notifier_subscription_ =
      ShutdownNotifierFactory::GetInstance()->Get(context)->Subscribe(
          base::BindRepeating(
              &ExtensionServiceWorkerMessageFilter::ShutdownOnUIThread,
              base::Unretained(this)));
}

void ExtensionServiceWorkerMessageFilter::ShutdownOnUIThread() {
  browser_context_ = nullptr;
  shutdown_notifier_subscription_ = {};
}

void ExtensionServiceWorkerMessageFilter::OnDestruct() const {
  content::BrowserThread::DeleteOnUIThread::Destruct(this);
}

void ExtensionServiceWorkerMessageFilter::EnsureShutdownNotifierFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
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
      message.type() == ExtensionHostMsg_DidStopServiceWorkerContext::ID ||
      message.type() == ExtensionHostMsg_WorkerResponseAck::ID) {
    *thread = content::BrowserThread::UI;
  }

  if (message.type() == ExtensionHostMsg_IncrementServiceWorkerActivity::ID ||
      message.type() == ExtensionHostMsg_DecrementServiceWorkerActivity::ID) {
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
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_WorkerResponseAck, OnResponseWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionServiceWorkerMessageFilter::OnRequestWorker(
    const mojom::RequestParams& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!browser_context_)
    return;
  dispatcher_->DispatchForServiceWorker(params, render_process_id_);
}

void ExtensionServiceWorkerMessageFilter::OnResponseWorker(
    int request_id,
    int64_t service_worker_version_id) {
  if (!browser_context_)
    return;
  dispatcher_->ProcessServiceWorkerResponse(request_id,
                                            service_worker_version_id);
}

void ExtensionServiceWorkerMessageFilter::OnIncrementServiceWorkerActivity(
    int64_t service_worker_version_id,
    const std::string& request_uuid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!browser_context_)
    return;
  active_request_uuids_.insert(request_uuid);
  // The worker might have already stopped before we got here, so the increment
  // below might fail legitimately. Therefore, we do not send bad_message to the
  // worker even if it fails.
  service_worker_context_->StartingExternalRequest(
      service_worker_version_id,
      content::ServiceWorkerExternalRequestTimeoutType::kDefault, request_uuid);
}

void ExtensionServiceWorkerMessageFilter::OnDecrementServiceWorkerActivity(
    int64_t service_worker_version_id,
    const std::string& request_uuid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!browser_context_)
    return;
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
  if (!browser_context_)
    return;
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
  if (!browser_context_)
    return;

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  DCHECK(registry);
  if (!registry->enabled_extensions().GetByID(extension_id)) {
    // This can happen if the extension is unloaded at this point. Just
    // checking the extension process (as below) is insufficient because
    // tearing down processes is async and happens after extension unload.
    return;
  }

  if (!ProcessMap::Get(browser_context_)
           ->Contains(extension_id, render_process_id_)) {
    // We check the process in addition to the registry to guard against
    // situations in which an extension may still be enabled, but no longer
    // running in a given process.
    return;
  }

  ServiceWorkerTaskQueue::Get(browser_context_)
      ->DidInitializeServiceWorkerContext(render_process_id_, extension_id,
                                          service_worker_version_id, thread_id);
}

void ExtensionServiceWorkerMessageFilter::OnDidStartServiceWorkerContext(
    const ExtensionId& extension_id,
    ActivationSequence activation_sequence,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!browser_context_)
    return;
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
                                     activation_sequence, service_worker_scope,
                                     service_worker_version_id, thread_id);
}

void ExtensionServiceWorkerMessageFilter::OnDidStopServiceWorkerContext(
    const ExtensionId& extension_id,
    ActivationSequence activation_sequence,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!browser_context_)
    return;
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
                                    activation_sequence, service_worker_scope,
                                    service_worker_version_id, thread_id);
}

void ExtensionServiceWorkerMessageFilter::DidFailDecrementInflightEvent() {
  bad_message::ReceivedBadMessage(this, bad_message::ESWMF_BAD_EVENT_ACK);
}

}  // namespace extensions
