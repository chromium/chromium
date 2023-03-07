// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_request_blob_task.h"

#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
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

  OpenCache(registration_id_, trace_id,
            base::BindOnce(&GetRequestBlobTask::DidOpenCache,
                           weak_factory_.GetWeakPtr(), trace_id));
}

void GetRequestBlobTask::DidOpenCache(int64_t trace_id,
                                      blink::mojom::CacheStorageError error) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "GetRequestBlobTask::DidOpenCache",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (error != blink::mojom::CacheStorageError::kSuccess) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  auto request =
      BackgroundFetchSettledFetch::CloneRequest(request_info_->fetch_request());
  request->url = MakeCacheUrlUnique(request->url, registration_id_.unique_id(),
                                    request_info_->request_index());

  auto match_options = blink::mojom::CacheQueryOptions::New();
  cache_storage_cache_remote()->Keys(
      std::move(request), std::move(match_options), trace_id,
      base::BindOnce(&GetRequestBlobTask::DidMatchRequest,
                     weak_factory_.GetWeakPtr(), trace_id));
}

void GetRequestBlobTask::DidMatchRequest(
    int64_t trace_id,
    blink::mojom::CacheKeysResultPtr result) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "GetRequestBlobTask::DidMatchRequest",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN);

  if (result->is_status() || result->get_keys().size() == 0) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  auto& keys = result->get_keys();
  DCHECK_EQ(keys.size(), 1u);
  DCHECK(keys[0]->blob);

  blob_ = std::move(keys[0]->blob);
  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
}

void GetRequestBlobTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  std::move(callback_).Run(error, std::move(blob_));
  Finished();
}

}  // namespace background_fetch
}  // namespace content
