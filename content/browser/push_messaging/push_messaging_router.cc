// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/push_messaging/push_messaging_router.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"

namespace content {

namespace {

using ServiceWorkerStartCallback = base::OnceCallback<void(
    scoped_refptr<ServiceWorkerVersion>,
    scoped_refptr<DevToolsBackgroundServicesContextImpl>,
    blink::ServiceWorkerStatusCode)>;

void RunPushEventCallback(
    PushMessagingRouter::PushEventCallback deliver_message_callback,
    blink::mojom::PushEventStatus push_event_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // PostTask() to ensure the callback is called asynchronously.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(deliver_message_callback), push_event_status));
}

// Given the |service_worker_registration|, this method finds and finishes the
// |callback| by finding the |service_worker_version|.
void DidFindServiceWorkerRegistration(
    ServiceWorkerMetrics::EventType event_type,
    scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context,
    ServiceWorkerStartCallback callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (event_type == ServiceWorkerMetrics::EventType::PUSH) {
    UMA_HISTOGRAM_ENUMERATION("PushMessaging.DeliveryStatus.FindServiceWorker",
                              service_worker_status);
  }
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(nullptr /* service_worker_version */,
                            nullptr /* devtools_context */,
                            service_worker_status);
    return;
  }
  ServiceWorkerVersion* version = service_worker_registration->active_version();
  DCHECK(version);

  version->RunAfterStartWorker(
      event_type,
      base::BindOnce(std::move(callback), base::WrapRefCounted(version),
                     std::move(devtools_context)));
}

// Finds the |service_worker_registration|.
void FindServiceWorkerRegistration(
    ServiceWorkerMetrics::EventType event_type,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context,
    const url::Origin& origin,
    int64_t service_worker_registration_id,
    ServiceWorkerStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Try to acquire the registration from storage. If it's already live we'll
  // receive it right away. If not, it will be revived from storage.
  service_worker_context->FindReadyRegistrationForId(
      service_worker_registration_id, blink::StorageKey(origin),
      base::BindOnce(&DidFindServiceWorkerRegistration, event_type,
                     std::move(devtools_context), std::move(callback)));
}

// According to the |event_type| this method will start finding the
// |service_worker_version| and |devtools_context| for the event. Must be called
// on the UI thread.
void StartServiceWorkerForDispatch(ServiceWorkerMetrics::EventType event_type,
                                   BrowserContext* browser_context,
                                   const GURL& origin,
                                   int64_t service_worker_registration_id,
                                   ServiceWorkerStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StoragePartition* partition =
      browser_context->GetStoragePartitionForUrl(origin);
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  auto devtools_context =
      base::WrapRefCounted<DevToolsBackgroundServicesContextImpl>(
          static_cast<DevToolsBackgroundServicesContextImpl*>(
              service_worker_context->storage_partition()
                  ->GetDevToolsBackgroundServicesContext()));

  FindServiceWorkerRegistration(
      event_type, std::move(service_worker_context),
      std::move(devtools_context), url::Origin::Create(origin),
      service_worker_registration_id, std::move(callback));
}

}  // namespace

// static
void PushMessagingRouter::DeliverMessage(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& message_id,
    absl::optional<std::string> payload,
    PushEventCallback deliver_message_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartServiceWorkerForDispatch(
      ServiceWorkerMetrics::EventType::PUSH, browser_context, origin,
      service_worker_registration_id,
      base::BindOnce(&PushMessagingRouter::DeliverMessageToWorker, message_id,
                     std::move(payload), std::move(deliver_message_callback)));
}

// static
void PushMessagingRouter::DeliverMessageToWorker(
    const std::string& message_id,
    absl::optional<std::string> payload,
    PushEventCallback deliver_message_callback,
    scoped_refptr<ServiceWorkerVersion> service_worker,
    scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Service worker registration was not found, run callback immediately
  if (!service_worker) {
    DCHECK_NE(blink::ServiceWorkerStatusCode::kOk, status);
    RunPushEventCallback(
        std::move(deliver_message_callback),
        status == blink::ServiceWorkerStatusCode::kErrorNotFound
            ? blink::mojom::PushEventStatus::NO_SERVICE_WORKER
            : blink::mojom::PushEventStatus::SERVICE_WORKER_ERROR);
    return;
  }

  // RunAfterStartWorker was not successful, end message delivery and log error
  // in devtools_context before running RunPushEventCallback
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    DeliverMessageEnd(std::move(service_worker), std::move(devtools_context),
                      message_id, std::move(deliver_message_callback), status);
    return;
  }

  int request_id = service_worker->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::PUSH,
      base::BindOnce(&PushMessagingRouter::DeliverMessageEnd, service_worker,
                     devtools_context, message_id,
                     std::move(deliver_message_callback)),
      base::Seconds(blink::mojom::kPushEventTimeoutSeconds),
      ServiceWorkerVersion::KILL_ON_TIMEOUT);

  service_worker->endpoint()->DispatchPushEvent(
      payload, service_worker->CreateSimpleEventCallback(request_id));

  if (devtools_context->IsRecording(
          DevToolsBackgroundService::kPushMessaging)) {
    std::map<std::string, std::string> event_metadata;
    if (payload)
      event_metadata["Payload"] = *payload;
    devtools_context->LogBackgroundServiceEvent(
        service_worker->registration_id(), service_worker->key(),
        DevToolsBackgroundService::kPushMessaging, "Push event dispatched",
        message_id, event_metadata);
  }
}

// static
void PushMessagingRouter::DeliverMessageEnd(
    scoped_refptr<ServiceWorkerVersion> service_worker,
    scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context,
    const std::string& message_id,
    PushEventCallback deliver_message_callback,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.DeliveryStatus.ServiceWorkerEvent",
                            service_worker_status);
  blink::mojom::PushEventStatus push_event_status =
      blink::mojom::PushEventStatus::SERVICE_WORKER_ERROR;
  std::string status_description;
  switch (service_worker_status) {
    case blink::ServiceWorkerStatusCode::kOk:
      push_event_status = blink::mojom::PushEventStatus::SUCCESS;
      status_description = "Success";
      break;
    case blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected:
      push_event_status =
          blink::mojom::PushEventStatus::EVENT_WAITUNTIL_REJECTED;
      status_description = "waitUntil Rejected";
      break;
    case blink::ServiceWorkerStatusCode::kErrorTimeout:
      push_event_status = blink::mojom::PushEventStatus::TIMEOUT;
      status_description = "Timeout";
      break;
    case blink::ServiceWorkerStatusCode::kErrorFailed:
    case blink::ServiceWorkerStatusCode::kErrorAbort:
    case blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorProcessNotFound:
    case blink::ServiceWorkerStatusCode::kErrorNotFound:
    case blink::ServiceWorkerStatusCode::kErrorIpcFailed:
    case blink::ServiceWorkerStatusCode::kErrorScriptEvaluateFailed:
    case blink::ServiceWorkerStatusCode::kErrorDiskCache:
    case blink::ServiceWorkerStatusCode::kErrorRedundant:
    case blink::ServiceWorkerStatusCode::kErrorDisallowed:
      push_event_status = blink::mojom::PushEventStatus::SERVICE_WORKER_ERROR;
      break;
    case blink::ServiceWorkerStatusCode::kErrorExists:
    case blink::ServiceWorkerStatusCode::kErrorInstallWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorActivateWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorNetwork:
    case blink::ServiceWorkerStatusCode::kErrorSecurity:
    case blink::ServiceWorkerStatusCode::kErrorState:
    case blink::ServiceWorkerStatusCode::kErrorInvalidArguments:
    case blink::ServiceWorkerStatusCode::kErrorStorageDisconnected:
      NOTREACHED() << "Got unexpected error code: "
                   << static_cast<uint32_t>(service_worker_status) << " "
                   << blink::ServiceWorkerStatusToString(service_worker_status);
      push_event_status = blink::mojom::PushEventStatus::SERVICE_WORKER_ERROR;
      break;
  }
  RunPushEventCallback(std::move(deliver_message_callback), push_event_status);

  if (devtools_context->IsRecording(
          DevToolsBackgroundService::kPushMessaging) &&
      push_event_status !=
          blink::mojom::PushEventStatus::SERVICE_WORKER_ERROR) {
    devtools_context->LogBackgroundServiceEvent(
        service_worker->registration_id(), service_worker->key(),
        DevToolsBackgroundService::kPushMessaging, "Push event completed",
        message_id, {{"Status", status_description}});
  }
}

// static
void PushMessagingRouter::FireSubscriptionChangeEvent(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    blink::mojom::PushSubscriptionPtr new_subscription,
    blink::mojom::PushSubscriptionPtr old_subscription,
    PushEventCallback subscription_change_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(features::kPushSubscriptionChangeEvent));

  StartServiceWorkerForDispatch(
      ServiceWorkerMetrics::EventType::PUSH_SUBSCRIPTION_CHANGE,
      browser_context, origin, service_worker_registration_id,
      base::BindOnce(&PushMessagingRouter::FireSubscriptionChangeEventToWorker,
                     std::move(new_subscription), std::move(old_subscription),
                     std::move(subscription_change_callback)));
}

// static
void PushMessagingRouter::FireSubscriptionChangeEventToWorker(
    blink::mojom::PushSubscriptionPtr new_subscription,
    blink::mojom::PushSubscriptionPtr old_subscription,
    PushEventCallback subscription_change_callback,
    scoped_refptr<ServiceWorkerVersion> service_worker,
    scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(features::kPushSubscriptionChangeEvent));

  if (!service_worker) {
    DCHECK_NE(blink::ServiceWorkerStatusCode::kOk, status);
    RunPushEventCallback(
        std::move(subscription_change_callback),
        status == blink::ServiceWorkerStatusCode::kErrorNotFound
            ? blink::mojom::PushEventStatus::NO_SERVICE_WORKER
            : blink::mojom::PushEventStatus::SERVICE_WORKER_ERROR);
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    FireSubscriptionChangeEventEnd(std::move(service_worker),
                                   std::move(subscription_change_callback),
                                   status);
    return;
  }

  int request_id = service_worker->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::PUSH_SUBSCRIPTION_CHANGE,
      base::BindOnce(&PushMessagingRouter::FireSubscriptionChangeEventEnd,
                     service_worker, std::move(subscription_change_callback)),
      base::Seconds(blink::mojom::kPushEventTimeoutSeconds),
      ServiceWorkerVersion::KILL_ON_TIMEOUT);

  service_worker->endpoint()->DispatchPushSubscriptionChangeEvent(
      std::move(old_subscription), std::move(new_subscription),
      service_worker->CreateSimpleEventCallback(request_id));
}

// static
void PushMessagingRouter::FireSubscriptionChangeEventEnd(
    scoped_refptr<ServiceWorkerVersion> service_worker,
    PushEventCallback subscription_change_callback,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::mojom::PushEventStatus push_event_status =
      blink::mojom::PushEventStatus::SERVICE_WORKER_ERROR;
  switch (service_worker_status) {
    case blink::ServiceWorkerStatusCode::kOk:
      push_event_status = blink::mojom::PushEventStatus::SUCCESS;
      break;
    case blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected:
      push_event_status =
          blink::mojom::PushEventStatus::EVENT_WAITUNTIL_REJECTED;
      break;
    case blink::ServiceWorkerStatusCode::kErrorTimeout:
      push_event_status = blink::mojom::PushEventStatus::TIMEOUT;
      break;
    default:
      break;
  }
  RunPushEventCallback(std::move(subscription_change_callback),
                       push_event_status);
}

}  // namespace content
