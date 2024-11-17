// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_MATCHER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_MATCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_response_callback.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom.h"

namespace content {

class ServiceWorkerInstalledScriptsSender;

// ServiceWorkerCacheStorageMatcher is a helper class to have a fetch API
// response that matches the given fetch API request from the cache storage
// API. It is used with the ServiceWorker static routing API to provide the
// cache source support.
// See: https://github.com/WICG/service-worker-static-routing-api
class CONTENT_EXPORT ServiceWorkerCacheStorageMatcher {
 public:
  ServiceWorkerCacheStorageMatcher(
      std::optional<std::string> cache_name,
      blink::mojom::FetchAPIRequestPtr request,
      scoped_refptr<ServiceWorkerVersion> version,
      ServiceWorkerFetchDispatcher::FetchCallback fetch_callback);

  ServiceWorkerCacheStorageMatcher(const ServiceWorkerCacheStorageMatcher&) =
      delete;
  ServiceWorkerCacheStorageMatcher& operator=(
      const ServiceWorkerCacheStorageMatcher&) = delete;

  ~ServiceWorkerCacheStorageMatcher();

  void Run();

  base::TimeTicks cache_lookup_start() { return cache_lookup_start_; }
  base::TimeDelta cache_lookup_duration() { return cache_lookup_duration_; }

 private:
  void FailFallback();
  void DidMatch(blink::mojom::MatchResultPtr result);
  void RunCallback(blink::ServiceWorkerStatusCode status,
                   ServiceWorkerFetchDispatcher::FetchEventResult fetch_result,
                   blink::mojom::FetchAPIResponsePtr response,
                   blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
                   blink::mojom::ServiceWorkerFetchEventTimingPtr timing);

  std::optional<std::string> cache_name_;
  blink::mojom::FetchAPIRequestPtr request_;
  scoped_refptr<ServiceWorkerVersion> version_;
  ServiceWorkerFetchDispatcher::FetchCallback fetch_callback_;

  mojo::Remote<blink::mojom::CacheStorage> remote_;
  base::TimeTicks cache_lookup_start_;
  base::TimeDelta cache_lookup_duration_;

  std::unique_ptr<ServiceWorkerInstalledScriptsSender>
      installed_scripts_sender_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ServiceWorkerCacheStorageMatcher> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_MATCHER_H_
