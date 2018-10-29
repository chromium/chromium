// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/match_requests_task.h"

#include "base/barrier_closure.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "services/network/public/cpp/cors/cors.h"

namespace content {

namespace background_fetch {

MatchRequestsTask::MatchRequestsTask(
    DatabaseTaskHost* host,
    BackgroundFetchRegistrationId registration_id,
    std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
    SettledFetchesCallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      match_params_(std::move(match_params)),
      callback_(std::move(callback)),
      weak_factory_(this) {}

MatchRequestsTask::~MatchRequestsTask() = default;

void MatchRequestsTask::Start() {
  cache_manager()->OpenCache(registration_id_.origin(),
                             CacheStorageOwner::kBackgroundFetch,
                             registration_id_.unique_id() /* cache_name */,
                             base::BindOnce(&MatchRequestsTask::DidOpenCache,
                                            weak_factory_.GetWeakPtr()));
}

void MatchRequestsTask::DidOpenCache(CacheStorageCacheHandle handle,
                                     blink::mojom::CacheStorageError error) {
  if (error != blink::mojom::CacheStorageError::kSuccess) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  handle_ = std::move(handle);
  DCHECK(handle_.value());

  std::unique_ptr<ServiceWorkerFetchRequest> request;
  if (match_params_->FilterByRequest()) {
    request = std::make_unique<ServiceWorkerFetchRequest>(
        match_params_->request_to_match());
  }

  handle_.value()->GetAllMatchedEntries(
      std::move(request), match_params_->cloned_cache_query_params(),
      base::BindOnce(&MatchRequestsTask::DidGetAllMatchedEntries,
                     weak_factory_.GetWeakPtr()));
}

void MatchRequestsTask::DidGetAllMatchedEntries(
    blink::mojom::CacheStorageError error,
    std::vector<CacheStorageCache::CacheEntry> entries) {
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

  size_t size = match_params_->match_all() ? entries.size() : 1u;
  settled_fetches_.reserve(size);

  for (size_t i = 0; i < size; i++) {
    auto& entry = entries[i];
    BackgroundFetchSettledFetch settled_fetch;
    settled_fetch.request = std::move(*entry.first);

    if (entry.second && entry.second->url_list.empty()) {
      // We didn't process this empty response, so we should expose it
      // as a nullptr.
      settled_fetch.response = nullptr;
    } else {
      settled_fetch.response = std::move(entry.second);
    }

    settled_fetches_.push_back(std::move(settled_fetch));
  }

  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
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
};

}  // namespace background_fetch

}  // namespace content
