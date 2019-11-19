// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/match_requests_task.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "services/network/public/cpp/cors/cors.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace content {
namespace background_fetch {

MatchRequestsTask::MatchRequestsTask(
    DatabaseTaskHost* host,
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
    SettledFetchesCallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      match_params_(std::move(match_params)),
      callback_(std::move(callback)) {}

MatchRequestsTask::~MatchRequestsTask() = default;

void MatchRequestsTask::Start() {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "MatchRequestsTask::Start",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);
  CacheStorageHandle cache_storage = GetOrOpenCacheStorage(registration_id_);
  cache_storage.value()->OpenCache(
      /* cache_name= */ registration_id_.unique_id(), trace_id,
      base::BindOnce(&MatchRequestsTask::DidOpenCache,
                     weak_factory_.GetWeakPtr(), trace_id));
}

void MatchRequestsTask::DidOpenCache(int64_t trace_id,
                                     CacheStorageCacheHandle handle,
                                     blink::mojom::CacheStorageError error) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "MatchRequestsTask::DidOpenCache",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (error != blink::mojom::CacheStorageError::kSuccess) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  handle_ = std::move(handle);
  DCHECK(handle_.value());

  blink::mojom::FetchAPIRequestPtr request;
  if (match_params_->FilterByRequest()) {
    request = BackgroundFetchSettledFetch::CloneRequest(
        match_params_->request_to_match());
  } else {
    request = blink::mojom::FetchAPIRequest::New();
  }

  auto query_options = match_params_->cloned_cache_query_options();
  if (!query_options)
    query_options = blink::mojom::CacheQueryOptions::New();

  // Ignore the search params since we added query params to make the URL
  // unique.
  query_options->ignore_search = true;

  // Ignore the method since Cache Storage assumes the request being matched
  // against is a GET.
  query_options->ignore_method = true;

  handle_.value()->GetAllMatchedEntries(
      std::move(request), std::move(query_options), trace_id,
      base::BindOnce(&MatchRequestsTask::DidGetAllMatchedEntries,
                     weak_factory_.GetWeakPtr(), trace_id));
}

void MatchRequestsTask::DidGetAllMatchedEntries(
    int64_t trace_id,
    blink::mojom::CacheStorageError error,
    std::vector<CacheStorageCache::CacheEntry> entries) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "MatchRequestsTask::DidGetAllMatchedEntries",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN);

  if (error != blink::mojom::CacheStorageError::kSuccess) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  // If we tried to match without filtering, there should always be entries.
  if (entries.empty()) {
    if (!match_params_->FilterByRequest())
      SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    else
      FinishWithError(blink::mojom::BackgroundFetchError::NONE);
    return;
  }

  for (auto& entry : entries) {
    auto settled_fetch = blink::mojom::BackgroundFetchSettledFetch::New();
    settled_fetch->request = std::move(entry.first);
    settled_fetch->request->url = RemoveUniqueParamFromCacheURL(
        settled_fetch->request->url, registration_id_.unique_id());

    if (!ShouldMatchRequest(settled_fetch->request))
      continue;

    if (entry.second && entry.second->url_list.empty()) {
      // We didn't process this empty response, so we should expose it
      // as a nullptr.
      settled_fetch->response = nullptr;
    } else {
      settled_fetch->response = std::move(entry.second);
    }

    settled_fetches_.push_back(std::move(settled_fetch));
    if (!match_params_->match_all())
      break;
  }

  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
}

bool MatchRequestsTask::ShouldMatchRequest(
    const blink::mojom::FetchAPIRequestPtr& request) {
  // We were supposed to match everything anyway.
  if (!match_params_->FilterByRequest())
    return true;

  // Ignore the request if the methods don't match.
  if ((!match_params_->cache_query_options() ||
       !match_params_->cache_query_options()->ignore_method) &&
      request->method != match_params_->request_to_match()->method) {
    return false;
  }

  // Ignore the request if the queries don't match.
  if ((!match_params_->cache_query_options() ||
       !match_params_->cache_query_options()->ignore_search) &&
      request->url.query() != match_params_->request_to_match()->url.query()) {
    return false;
  }

  return true;
}

void MatchRequestsTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  if (HasStorageError())
    error = blink::mojom::BackgroundFetchError::STORAGE_ERROR;
  ReportStorageError();

  std::move(callback_).Run(error, std::move(settled_fetches_));
  Finished();  // Destroys |this|.
}

std::string MatchRequestsTask::HistogramName() const {
  return "MatchRequestsTask";
}

}  // namespace background_fetch
}  // namespace content
