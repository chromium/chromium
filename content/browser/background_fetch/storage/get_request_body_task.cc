// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_request_body_task.h"

#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

namespace content {
namespace background_fetch {

GetRequestBodyTask::GetRequestBodyTask(
    DatabaseTaskHost* host,
    const BackgroundFetchRegistrationId& registration_id,
    const scoped_refptr<BackgroundFetchRequestInfo>& request_info,
    GetRequestBodyCallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      request_info_(request_info),
      callback_(std::move(callback)) {}

GetRequestBodyTask::~GetRequestBodyTask() = default;

void GetRequestBodyTask::Start() {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT("CacheStorage", "GetRequestBodyTask::Start",
              perfetto::Flow::Global(trace_id));

  OpenCache(registration_id_, trace_id,
            base::BindOnce(&GetRequestBodyTask::DidOpenCache,
                           weak_factory_.GetWeakPtr(), trace_id));
}

void GetRequestBodyTask::DidOpenCache(int64_t trace_id,
                                      blink::mojom::CacheStorageError error) {
  TRACE_EVENT("CacheStorage", "GetRequestBodyTask::DidOpenCache",
              perfetto::Flow::Global(trace_id));
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
      base::BindOnce(&GetRequestBodyTask::DidMatchRequest,
                     weak_factory_.GetWeakPtr(), trace_id));
}

void GetRequestBodyTask::DidMatchRequest(
    int64_t trace_id,
    blink::mojom::CacheStorageCache::KeysResult result) {
  TRACE_EVENT("CacheStorage", "GetRequestBodyTask::DidMatchRequest",
              perfetto::TerminatingFlow::Global(trace_id));

  if (!result.has_value() || result.value().size() != 1u) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  blink::mojom::FetchAPIRequestPtr& request = result.value()[0];
  if (!request->body) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  std::vector<network::DataElement>* elements =
      request->body->elements_mutable();
  if (elements->size() != 1u ||
      (*elements)[0].type() != network::DataElement::Tag::kDataPipe) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  data_pipe_getter_ =
      (*elements)[0].As<network::DataElementDataPipe>().ReleaseDataPipeGetter();
  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
}

void GetRequestBodyTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  std::move(callback_).Run(error, std::move(data_pipe_getter_));
  Finished();
}

}  // namespace background_fetch
}  // namespace content
