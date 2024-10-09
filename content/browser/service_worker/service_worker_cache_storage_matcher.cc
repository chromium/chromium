// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_storage_matcher.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_installed_scripts_sender.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_response_callback.mojom.h"

namespace content {

ServiceWorkerCacheStorageMatcher::ServiceWorkerCacheStorageMatcher(
    std::optional<std::string> cache_name,
    blink::mojom::FetchAPIRequestPtr request,
    scoped_refptr<ServiceWorkerVersion> version,
    ServiceWorkerFetchDispatcher::FetchCallback fetch_callback)
    : cache_name_(std::move(cache_name)),
      request_(std::move(request)),
      version_(std::move(version)),
      fetch_callback_(std::move(fetch_callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!request_->blob);
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerCacheStorageMatcher::ServiceWorkerCacheStorageMatcher",
      TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_OUT);
}

ServiceWorkerCacheStorageMatcher::~ServiceWorkerCacheStorageMatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerCacheStorageMatcher::~ServiceWorkerCacheStorageMatcher",
      TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_IN);
}

void ServiceWorkerCacheStorageMatcher::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerCacheStorageMatcher::Run",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  CHECK(cache_lookup_start_.is_null());
  cache_lookup_start_ = base::TimeTicks::Now();
  // If `GetMainScriptResponse` is not set, it need to be set from the
  // installed script.  Or, calling the fallback function may fail.
  if (!version_->GetMainScriptResponse()) {
    CHECK(ServiceWorkerVersion::IsInstalled(version_->status()));
    CHECK(!installed_scripts_sender_);
    installed_scripts_sender_ =
        std::make_unique<ServiceWorkerInstalledScriptsSender>(version_.get());
    installed_scripts_sender_->Start();
  }
  // Gets receiver.
  if (!version_->context()) {
    FailFallback();
    return;
  }
  auto* storage_partition = version_->context()->wrapper()->storage_partition();
  if (!storage_partition) {
    FailFallback();
    return;
  }
  auto* control = storage_partition->GetCacheStorageControl();
  if (!control) {
    FailFallback();
    return;
  }
  // Since this is offloading the cache storage API access in ServiceWorker,
  // we need to follow COEP and DIP used there.
  // The reason why COEP is enforced to the cache storage API can be seen in:
  // crbug.com/991428.
  const network::CrossOriginEmbedderPolicy* coep =
      version_->cross_origin_embedder_policy();
  const network::DocumentIsolationPolicy* dip =
      version_->document_isolation_policy();
  if (!coep || !dip) {
    FailFallback();
    return;
  }
  control->AddReceiver(
      *coep, version_->embedded_worker()->GetCoepReporter(), *dip,
      storage::BucketLocator::ForDefaultBucket(version_->key()),
      storage::mojom::CacheStorageOwner::kCacheAPI,
      remote_.BindNewPipeAndPassReceiver());

  // Calls caches.match.
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  auto options = blink::mojom::MultiCacheQueryOptions::New();
  options->query_options = blink::mojom::CacheQueryOptions::New();
  if (cache_name_) {
    options->cache_name = base::UTF8ToUTF16(*cache_name_);
  }
  remote_->Match(std::move(request_), std::move(options),
                 /*in_related_fetch_event=*/false,
                 /*in_range_fetch_event=*/false, trace_id,
                 base::BindOnce(&ServiceWorkerCacheStorageMatcher::DidMatch,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ServiceWorkerCacheStorageMatcher::DidMatch(
    blink::mojom::MatchResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerCacheStorageMatcher::DidMatch",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  cache_lookup_duration_ = base::TimeTicks::Now() - cache_lookup_start_;
  base::UmaHistogramTimes(
      "ServiceWorker.StaticRouter.MainResource.CacheLookupDuration",
      cache_lookup_duration_);

  auto timing = blink::mojom::ServiceWorkerFetchEventTiming::New();
  switch (result->which()) {
    case blink::mojom::MatchResult::Tag::kStatus:  // error fallback.
      base::UmaHistogramEnumeration(
          "ServiceWorker.StaticRouter.MainResource.CacheStorageError",
          result->get_status());
      RunCallback(
          blink::ServiceWorkerStatusCode::kOk,
          ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback,
          blink::mojom::FetchAPIResponse::New(), nullptr, std::move(timing));
      break;
    case blink::mojom::MatchResult::Tag::kResponse:  // we got fetch response.
      if (result->get_response()->parsed_headers) {
        // We intend to reset the parsed header. Or, invalid parsed headers
        // should be set.
        //
        // According to content/browser/cache_storage/cache_storage_cache.cc,
        // the field looks not set up with the meaningful value.
        // Also, the Cache Storage API code looks not using the parsed_header
        // according to third_party/blink/renderer/core/fetch/response.cc.
        // (It can be tracked from
        // third_party/blink/renderer/modules/cache_storage/cache_storage.cc)
        result->get_response()->parsed_headers.reset();
      }
      RunCallback(blink::ServiceWorkerStatusCode::kOk,
                  ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
                  std::move(result->get_response()), nullptr,
                  std::move(timing));
      break;
    case blink::mojom::MatchResult::Tag::kEagerResponse:
      // EagerResponse, which should be used only if `in_related_fetch_event`
      // is set.
      NOTREACHED();
  }
}

void ServiceWorkerCacheStorageMatcher::FailFallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerCacheStorageMatcher::FailFallback",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // `Run` method will be called in
  // `ServiceWorkerMainResourceLoader::StartRequest`.
  // Its `fallback_callback_` works after returning fom the `StartRequest`
  // function, and PostTask is used there.  Since `fetch_callback_` directly
  // calls `fallback_callback_`, we need to follow that.
  //
  // You can find the detailed explanation in
  // `ServiceWorkerMainResourceLoader::StartRequest`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerCacheStorageMatcher::RunCallback,
          weak_ptr_factory_.GetWeakPtr(),
          blink::ServiceWorkerStatusCode::kErrorFailed,
          ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback,
          blink::mojom::FetchAPIResponse::New(), nullptr,
          blink::mojom::ServiceWorkerFetchEventTiming::New()));
  return;
}

void ServiceWorkerCacheStorageMatcher::RunCallback(
    blink::ServiceWorkerStatusCode status,
    ServiceWorkerFetchDispatcher::FetchEventResult fetch_result,
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerCacheStorageMatcher::RunCallback",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_IN);
  // Fetch dispatcher can be completed at this point due to a failure of
  // starting up a worker. In that case, let's simply ignore it.
  if (!fetch_callback_) {
    return;
  }

  // Wait until `installed_scripts_sender_` updates the main script response.
  if (installed_scripts_sender_ &&
      installed_scripts_sender_->last_finished_reason() ==
          ServiceWorkerInstalledScriptReader::FinishedReason::kNotFinished) {
    installed_scripts_sender_->SetFinishCallback(base::BindOnce(
        &ServiceWorkerCacheStorageMatcher::RunCallback,
        weak_ptr_factory_.GetWeakPtr(), status, fetch_result,
        std::move(response), std::move(body_as_stream), std::move(timing)));
    return;
  }

  std::move(fetch_callback_)
      .Run(status, fetch_result, std::move(response), std::move(body_as_stream),
           std::move(timing), version_);
}

}  // namespace content
