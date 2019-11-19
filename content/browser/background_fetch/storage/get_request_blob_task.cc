// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_request_blob_task.h"
#include "base/bind.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"

namespace content {
namespace background_fetch {

GetRequestBlobTask::GetRequestBlobTask(
    DatabaseTaskHost* host,
    const BackgroundFetchRegistrationId& registration_id,
    const scoped_refptr<BackgroundFetchRequestInfo>& request_info,
    GetRequestBlobCallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      request_info_(request_info),
      callback_(std::move(callback)) {}

GetRequestBlobTask::~GetRequestBlobTask() = default;

void GetRequestBlobTask::Start() {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "GetRequestBlobTask::Start",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);
  CacheStorageHandle cache_storage = GetOrOpenCacheStorage(registration_id_);
  cache_storage.value()->OpenCache(
      /* cache_name= */ registration_id_.unique_id(), trace_id,
      base::BindOnce(&GetRequestBlobTask::DidOpenCache,
                     weak_factory_.GetWeakPtr(), trace_id));
}

void GetRequestBlobTask::DidOpenCache(int64_t trace_id,
                                      CacheStorageCacheHandle handle,
                                      blink::mojom::CacheStorageError error) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "GetRequestBlobTask::DidOpenCache",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (error != blink::mojom::CacheStorageError::kSuccess) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  DCHECK(handle.value());
  auto request =
      BackgroundFetchSettledFetch::CloneRequest(request_info_->fetch_request());
  request->url = MakeCacheUrlUnique(request->url, registration_id_.unique_id(),
                                    request_info_->request_index());

  handle.value()->GetAllMatchedEntries(
      std::move(request), /* match_options= */ nullptr, trace_id,
      base::BindOnce(&GetRequestBlobTask::DidMatchRequest,
                     weak_factory_.GetWeakPtr(), handle.Clone(), trace_id));
}

void GetRequestBlobTask::DidMatchRequest(
    CacheStorageCacheHandle handle,
    int64_t trace_id,
    blink::mojom::CacheStorageError error,
    std::vector<CacheStorageCache::CacheEntry> entries) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "GetRequestBlobTask::DidMatchRequest",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN);

  if (error != blink::mojom::CacheStorageError::kSuccess || entries.empty()) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  DCHECK_EQ(entries.size(), 1u);
  DCHECK(entries[0].first->blob);

  blob_ = std::move(entries[0].first->blob);
  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
}

void GetRequestBlobTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  ReportStorageError();

  std::move(callback_).Run(error, std::move(blob_));
  Finished();
}

std::string GetRequestBlobTask::HistogramName() const {
  return "GetRequestBlobTask";
}

}  // namespace background_fetch
}  // namespace content
