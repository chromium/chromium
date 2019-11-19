// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/mark_request_complete_task.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/guid.h"
#include "base/task/post_task.h"
#include "content/browser/background_fetch/background_fetch_cross_origin_filter.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/background_fetch/storage/get_metadata_task.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/fetch/fetch_api_request_proto.h"
#include "content/public/browser/browser_task_traits.h"
#include "services/network/public/cpp/cors/cors.h"
#include "storage/browser/blob/blob_impl.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"

namespace content {
namespace background_fetch {

namespace {

blink::mojom::SerializedBlobPtr MakeBlob(
    scoped_refptr<BackgroundFetchRequestInfo> info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::unique_ptr<storage::BlobDataHandle> response_blob_handle =
      info->TakeResponseBlobDataHandleOnIO();

  if (!response_blob_handle)
    return nullptr;

  auto blob = blink::mojom::SerializedBlob::New();
  blob->uuid = response_blob_handle->uuid();
  blob->size = response_blob_handle->size();

  storage::BlobImpl::Create(std::move(response_blob_handle),
                            blob->blob.InitWithNewPipeAndPassReceiver());
  return blob;
}

// Returns whether the response contained in the Background Fetch |request| is
// considered OK. See https://fetch.spec.whatwg.org/#ok-status aka a successful
// 2xx status per https://tools.ietf.org/html/rfc7231#section-6.3.
bool IsOK(const BackgroundFetchRequestInfo& request) {
  int status = request.GetResponseCode();
  return network::cors::IsOkStatus(status);
}

}  // namespace

MarkRequestCompleteTask::MarkRequestCompleteTask(
    DatabaseTaskHost* host,
    const BackgroundFetchRegistrationId& registration_id,
    scoped_refptr<BackgroundFetchRequestInfo> request_info,
    MarkRequestCompleteCallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      request_info_(std::move(request_info)),
      callback_(std::move(callback)) {}

MarkRequestCompleteTask::~MarkRequestCompleteTask() = default;

void MarkRequestCompleteTask::Start() {
  DCHECK(blob_storage_context());
  request_info_->CreateResponseBlobDataHandle(blob_storage_context());

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2u, base::BindOnce(&MarkRequestCompleteTask::FinishWithError,
                         weak_factory_.GetWeakPtr(),
                         blink::mojom::BackgroundFetchError::NONE));

  StoreResponse(barrier_closure);
  UpdateMetadata(barrier_closure);
}

void MarkRequestCompleteTask::StoreResponse(base::OnceClosure done_closure) {
  response_ = blink::mojom::FetchAPIResponse::New();
  response_->url_list = request_info_->GetURLChain();
  response_->response_type = network::mojom::FetchResponseType::kDefault;
  response_->response_time = request_info_->GetResponseTime();

  if (request_info_->GetURLChain().empty()) {
    // The URL chain was not provided, so this is a failed response.
    DCHECK(!request_info_->IsResultSuccess());
    failure_reason_ = proto::BackgroundFetchRegistration::FETCH_ERROR;
    CreateAndStoreCompletedRequest(std::move(done_closure));
    return;
  }

  // TODO(crbug.com/884672): Move cross origin checks to when the response
  // headers are available.
  BackgroundFetchCrossOriginFilter filter(registration_id_.origin(),
                                          *request_info_);
  if (!filter.CanPopulateBody()) {
    failure_reason_ = proto::BackgroundFetchRegistration::FETCH_ERROR;
    // No point writing the response to the cache since it won't be exposed.
    CreateAndStoreCompletedRequest(std::move(done_closure));
    return;
  }

  // Include the status code, status text and the response's body as a blob
  // when this is allowed by the CORS protocol.
  response_->status_code = request_info_->GetResponseCode();
  response_->status_text = request_info_->GetResponseText();
  response_->headers.insert(request_info_->GetResponseHeaders().begin(),
                            request_info_->GetResponseHeaders().end());

  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    base::PostTaskAndReplyWithResult(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&MakeBlob, request_info_),
        base::BindOnce(&MarkRequestCompleteTask::DidMakeBlob,
                       weak_factory_.GetWeakPtr(), std::move(done_closure)));
  } else {
    DidMakeBlob(std::move(done_closure), MakeBlob(request_info_));
  }
}

void MarkRequestCompleteTask::DidMakeBlob(
    base::OnceClosure done_closure,
    blink::mojom::SerializedBlobPtr blob) {
  response_->blob = std::move(blob);
  if (!IsOK(*request_info_))
    failure_reason_ = proto::BackgroundFetchRegistration::BAD_STATUS;

  // We need to check if there is enough quota before writing the response to
  // the cache.
  if (request_info_->GetResponseSize()) {
    IsQuotaAvailable(
        registration_id_.origin(), request_info_->GetResponseSize(),
        base::BindOnce(&MarkRequestCompleteTask::DidGetIsQuotaAvailable,
                       weak_factory_.GetWeakPtr(), std::move(done_closure)));
  } else {
    // Assume there is enough quota.
    DidGetIsQuotaAvailable(std::move(done_closure), /* is_available= */ true);
  }
}

void MarkRequestCompleteTask::DidGetIsQuotaAvailable(
    base::OnceClosure done_closure,
    bool is_available) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "MarkRequestCompleteTask::DidGetIsQuotaAvailable",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  if (!is_available) {
    FinishWithError(blink::mojom::BackgroundFetchError::QUOTA_EXCEEDED);
    return;
  }

  CacheStorageHandle cache_storage = GetOrOpenCacheStorage(registration_id_);
  cache_storage.value()->OpenCache(
      /* cache_name= */ registration_id_.unique_id(), trace_id,
      base::BindOnce(&MarkRequestCompleteTask::DidOpenCache,
                     weak_factory_.GetWeakPtr(), std::move(done_closure),
                     trace_id));
}

void MarkRequestCompleteTask::DidOpenCache(
    base::OnceClosure done_closure,
    int64_t trace_id,
    CacheStorageCacheHandle handle,
    blink::mojom::CacheStorageError error) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "MarkRequestCompleteTask::DidOpenCache",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (error != blink::mojom::CacheStorageError::kSuccess) {
    SetStorageError(BackgroundFetchStorageError::kCacheStorageError);
    CreateAndStoreCompletedRequest(std::move(done_closure));
    return;
  }

  DCHECK(handle.value());

  blink::mojom::FetchAPIRequestPtr request =
      BackgroundFetchSettledFetch::CloneRequest(
          request_info_->fetch_request_ptr());

  request->url = MakeCacheUrlUnique(request->url, registration_id_.unique_id(),
                                    request_info_->request_index());

  // TODO(crbug.com/774054): The request blob stored in the cache is being
  // overwritten here, it should be written back.
  handle.value()->Put(
      std::move(request), BackgroundFetchSettledFetch::CloneResponse(response_),
      trace_id,
      base::BindOnce(&MarkRequestCompleteTask::DidWriteToCache,
                     weak_factory_.GetWeakPtr(), std::move(handle),
                     std::move(done_closure)));
}

void MarkRequestCompleteTask::DidWriteToCache(
    CacheStorageCacheHandle handle,
    base::OnceClosure done_closure,
    blink::mojom::CacheStorageError error) {
  if (error != blink::mojom::CacheStorageError::kSuccess)
    SetStorageError(BackgroundFetchStorageError::kCacheStorageError);
  CreateAndStoreCompletedRequest(std::move(done_closure));
}

void MarkRequestCompleteTask::CreateAndStoreCompletedRequest(
    base::OnceClosure done_closure) {
  completed_request_.set_unique_id(registration_id_.unique_id());
  completed_request_.set_request_index(request_info_->request_index());
  completed_request_.set_serialized_request(
      SerializeFetchRequestToString(*(request_info_->fetch_request())));
  completed_request_.set_download_guid(request_info_->download_guid());
  completed_request_.set_failure_reason(failure_reason_);

  service_worker_context()->StoreRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      registration_id_.origin().GetURL(),
      {{CompletedRequestKey(completed_request_.unique_id(),
                            completed_request_.request_index()),
        completed_request_.SerializeAsString()}},
      base::BindOnce(&MarkRequestCompleteTask::DidStoreCompletedRequest,
                     weak_factory_.GetWeakPtr(), std::move(done_closure)));
}

void MarkRequestCompleteTask::DidStoreCompletedRequest(
    base::OnceClosure done_closure,
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      break;
    case DatabaseStatus::kFailed:
    case DatabaseStatus::kNotFound:
      SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
      std::move(done_closure).Run();
      return;
  }

  // Notify observers that the request is complete.
  for (auto& observer : data_manager()->observers()) {
    observer.OnRequestCompleted(
        registration_id_.unique_id(),
        BackgroundFetchSettledFetch::CloneRequest(
            request_info_->fetch_request_ptr()),
        BackgroundFetchSettledFetch::CloneResponse(response_));
  }

  // Delete the active request.
  service_worker_context()->ClearRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      {ActiveRequestKey(completed_request_.unique_id(),
                        completed_request_.request_index())},
      base::BindOnce(&MarkRequestCompleteTask::DidDeleteActiveRequest,
                     weak_factory_.GetWeakPtr(), std::move(done_closure)));
}

void MarkRequestCompleteTask::DidDeleteActiveRequest(
    base::OnceClosure done_closure,
    blink::ServiceWorkerStatusCode status) {
  if (ToDatabaseStatus(status) != DatabaseStatus::kOk)
    SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
  std::move(done_closure).Run();
}

void MarkRequestCompleteTask::UpdateMetadata(base::OnceClosure done_closure) {
  if (!request_info_->IsResultSuccess() || !request_info_->GetResponseSize()) {
    std::move(done_closure).Run();
    return;
  }

  AddSubTask(std::make_unique<GetMetadataTask>(
      this, registration_id_.service_worker_registration_id(),
      registration_id_.origin(), registration_id_.developer_id(),
      base::BindOnce(&MarkRequestCompleteTask::DidGetMetadata,
                     weak_factory_.GetWeakPtr(), std::move(done_closure))));
}

void MarkRequestCompleteTask::DidGetMetadata(
    base::OnceClosure done_closure,
    blink::mojom::BackgroundFetchError error,
    std::unique_ptr<proto::BackgroundFetchMetadata> metadata) {
  if (!metadata || error != blink::mojom::BackgroundFetchError::NONE) {
    SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
    std::move(done_closure).Run();
    return;
  }

  metadata->mutable_registration()->set_downloaded(
      metadata->registration().downloaded() + request_info_->GetResponseSize());
  metadata->mutable_registration()->set_uploaded(
      metadata->registration().uploaded() + request_info_->request_body_size());

  service_worker_context()->StoreRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      registration_id_.origin().GetURL(),
      {{RegistrationKey(registration_id_.unique_id()),
        metadata->SerializeAsString()}},
      base::BindOnce(&MarkRequestCompleteTask::DidStoreMetadata,
                     weak_factory_.GetWeakPtr(), std::move(done_closure)));
}

void MarkRequestCompleteTask::DidStoreMetadata(
    base::OnceClosure done_closure,
    blink::ServiceWorkerStatusCode status) {
  if (ToDatabaseStatus(status) != DatabaseStatus::kOk)
    SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
  std::move(done_closure).Run();
}

void MarkRequestCompleteTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  if (HasStorageError())
    error = blink::mojom::BackgroundFetchError::STORAGE_ERROR;
  ReportStorageError();

  std::move(callback_).Run(error);
  Finished();
}

std::string MarkRequestCompleteTask::HistogramName() const {
  return "MarkRequestCompleteTask";
}

}  // namespace background_fetch
}  // namespace content
