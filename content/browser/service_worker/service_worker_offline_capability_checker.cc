// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_offline_capability_checker.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/uuid.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/fetch/fetch_request_type_converters.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom.h"
#include "url/gurl.h"

namespace content {

ServiceWorkerOfflineCapabilityChecker::ServiceWorkerOfflineCapabilityChecker(
    const GURL& url,
    const blink::StorageKey& key)
    : url_(url), key_(key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerOfflineCapabilityChecker::
    ~ServiceWorkerOfflineCapabilityChecker() = default;

void ServiceWorkerOfflineCapabilityChecker::Start(
    ServiceWorkerRegistry* registry,
    ServiceWorkerContext::CheckOfflineCapabilityCallback callback) {
  callback_ = std::move(callback);
  registry->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation, url_, key_,
      base::BindOnce(
          &ServiceWorkerOfflineCapabilityChecker::DidFindRegistration,
          // We can use base::Unretained(this) because |this| is expected
          // to be alive until the |callback_| is called.
          base::Unretained(this)));
}

void ServiceWorkerOfflineCapabilityChecker::DidFindRegistration(
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback_).Run(OfflineCapability::kUnsupported,
                             blink::mojom::kInvalidServiceWorkerRegistrationId);
    return;
  }

  if (registration->is_uninstalling() || registration->is_uninstalled()) {
    std::move(callback_).Run(OfflineCapability::kUnsupported,
                             blink::mojom::kInvalidServiceWorkerRegistrationId);
    return;
  }

  scoped_refptr<ServiceWorkerVersion> preferred_version =
      registration->active_version();
  if (!preferred_version) {
    preferred_version = registration->waiting_version();
  }
  if (!preferred_version) {
    std::move(callback_).Run(OfflineCapability::kUnsupported,
                             blink::mojom::kInvalidServiceWorkerRegistrationId);
    return;
  }

  ServiceWorkerVersion::FetchHandlerExistence existence =
      preferred_version->fetch_handler_existence();

  DCHECK_NE(existence, ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN);

  if (existence != ServiceWorkerVersion::FetchHandlerExistence::EXISTS) {
    std::move(callback_).Run(OfflineCapability::kUnsupported,
                             preferred_version->registration_id());
    return;
  }

  if (preferred_version->status() != ServiceWorkerVersion::Status::ACTIVATING &&
      preferred_version->status() != ServiceWorkerVersion::Status::ACTIVATED) {
    // ServiceWorkerFetchDispatcher assumes that the version's status is
    // ACTIVATING or ACTIVATED. If the version's status is other one, we return
    // kUnsupported, without waiting its status becoming ACTIVATING because that
    // is not always guaranteed.
    // TODO(hayato): We can do a bit better, such as 1) trigger the activation
    // and wait, or 2) return a value to indicate the service worker is
    // installed but not yet activated.
    std::move(callback_).Run(OfflineCapability::kUnsupported,
                             preferred_version->registration_id());
    return;
  }

  network::ResourceRequest resource_request;
  resource_request.url = url_;
  resource_request.method = "GET";
  resource_request.mode = network::mojom::RequestMode::kNavigate;
  resource_request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  resource_request.destination = network::mojom::RequestDestination::kDocument;

  // Store the weak reference to ServiceWorkerContextCore before
  // |preferred_version| moves.
  base::WeakPtr<ServiceWorkerContextCore> context =
      preferred_version->context();
  if (!context) {
    std::move(callback_).Run(OfflineCapability::kUnsupported,
                             preferred_version->registration_id());
    return;
  }

  fetch_dispatcher_ = std::make_unique<ServiceWorkerFetchDispatcher>(
      blink::mojom::FetchAPIRequest::From(resource_request),
      resource_request.destination,
      base::Uuid::GenerateRandomV4().AsLowercaseString() /* client_id */,
      std::move(preferred_version), base::DoNothing() /* prepare callback */,
      base::BindOnce(&ServiceWorkerOfflineCapabilityChecker::OnFetchResult,
                     base::Unretained(this)),
      /*is_offline_capability_check=*/true);

  fetch_dispatcher_->MaybeStartNavigationPreload(
      resource_request, context->wrapper(), /*frame_tree_node_id=*/-1);

  fetch_dispatcher_->Run();
}

void ServiceWorkerOfflineCapabilityChecker::OnFetchResult(
    blink::ServiceWorkerStatusCode status,
    ServiceWorkerFetchDispatcher::FetchEventResult result,
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr /* stream */,
    blink::mojom::ServiceWorkerFetchEventTimingPtr /* timing */,
    scoped_refptr<ServiceWorkerVersion> version) {
  // The sites are considered as "offline capable" when the response finished
  // successfully and returns successful responses (200–299) or redirects
  // (300–399). Also considered as "offline capable" when the timeout happens.
  if ((status == blink::ServiceWorkerStatusCode::kOk &&
       result == ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse &&
       (200 <= response->status_code && response->status_code <= 399)) ||
      status == blink::ServiceWorkerStatusCode::kErrorTimeout) {
    std::move(callback_).Run(OfflineCapability::kSupported,
                             version->registration_id());
  } else {
    // TODO(hayato): At present, we return kUnsupported even if the detection
    // failed due to internal errors except timeout (disk fialures, etc). In the
    // future, we might want to return another enum value so that the callside
    // can know whether internal errors happened or not.
    std::move(callback_).Run(OfflineCapability::kUnsupported,
                             version->registration_id());
  }
}

}  // namespace content
