// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_OFFLINE_CAPABILITY_CHECKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_OFFLINE_CAPABILITY_CHECKER_H_

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/public/browser/service_worker_context.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_response_callback.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom.h"

class GURL;

namespace content {

class ServiceWorkerRegistration;
class ServiceWorkerRegistry;
class ServiceWorkerVersion;

// Utility class used to check a service worker's offline capability.
// Tracking bug is crbug.com/965802.
//
// |this| must outlive |callback_|.
class ServiceWorkerOfflineCapabilityChecker {
 public:
  explicit ServiceWorkerOfflineCapabilityChecker(const GURL& url,
                                                 const blink::StorageKey& key);
  ~ServiceWorkerOfflineCapabilityChecker();

  ServiceWorkerOfflineCapabilityChecker(
      const ServiceWorkerOfflineCapabilityChecker&) = delete;
  ServiceWorkerOfflineCapabilityChecker& operator=(
      const ServiceWorkerOfflineCapabilityChecker&) = delete;

  ServiceWorkerOfflineCapabilityChecker(
      const ServiceWorkerOfflineCapabilityChecker&&) = delete;
  ServiceWorkerOfflineCapabilityChecker& operator=(
      const ServiceWorkerOfflineCapabilityChecker&&) = delete;

  // It's the caller's responsibility to make sure that |this| outlives
  // |callback|.
  void Start(ServiceWorkerRegistry* registry,
             ServiceWorkerContext::CheckOfflineCapabilityCallback callback);

 private:
  void DidFindRegistration(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void OnFetchResult(
      blink::ServiceWorkerStatusCode actual_status,
      ServiceWorkerFetchDispatcher::FetchEventResult actual_result,
      blink::mojom::FetchAPIResponsePtr actual_response,
      blink::mojom::ServiceWorkerStreamHandlePtr /* stream */,
      blink::mojom::ServiceWorkerFetchEventTimingPtr /* timing */,
      scoped_refptr<ServiceWorkerVersion> worker);

  const GURL url_;
  const blink::StorageKey key_;
  ServiceWorkerContext::CheckOfflineCapabilityCallback callback_;
  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_OFFLINE_CAPABILITY_CHECKER_H_
