// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/start_next_pending_request_task.h"

#include "base/bind.h"
#include "base/guid.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/fetch/fetch_api_request_proto.h"

namespace content {
namespace background_fetch {

StartNextPendingRequestTask::StartNextPendingRequestTask(
    DatabaseTaskHost* host,
    const BackgroundFetchRegistrationId& registration_id,
    NextRequestCallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      callback_(std::move(callback)) {
  DCHECK(!registration_id_.is_null());
}

StartNextPendingRequestTask::~StartNextPendingRequestTask() = default;

void StartNextPendingRequestTask::Start() {
  GetPendingRequests();
}

void StartNextPendingRequestTask::GetPendingRequests() {
  service_worker_context()->GetRegistrationUserDataByKeyPrefix(
      registration_id_.service_worker_registration_id(),
      PendingRequestKeyPrefix(registration_id_.unique_id()),
      base::BindOnce(&StartNextPendingRequestTask::DidGetPendingRequests,
                     weak_factory_.GetWeakPtr()));
}

void StartNextPendingRequestTask::DidGetPendingRequests(
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
    case DatabaseStatus::kFailed:
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
    case DatabaseStatus::kOk:
      if (data.empty()) {
        // There are no pending requests.
        FinishWithError(blink::mojom::BackgroundFetchError::NONE);
        return;
      }
  }

  if (!pending_request_.ParseFromString(data.front())) {
    // Service Worker database has been corrupted. Abandon fetches.
    AbandonFetches(registration_id_.service_worker_registration_id());
    SetStorageErrorAndFinish(
        BackgroundFetchStorageError::kServiceWorkerStorageError);
    return;
  }

  // Create an active request.
  proto::BackgroundFetchActiveRequest active_request;

  active_request_.set_download_guid(base::GenerateGUID());
  active_request_.set_unique_id(pending_request_.unique_id());
  active_request_.set_request_index(pending_request_.request_index());
  // Transfer ownership of the request to avoid a potentially expensive copy.
  active_request_.set_allocated_serialized_request(
      pending_request_.release_serialized_request());
  active_request_.set_request_body_size(pending_request_.request_body_size());

  service_worker_context()->StoreRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      registration_id_.origin().GetURL(),
      {{ActiveRequestKey(active_request_.unique_id(),
                         active_request_.request_index()),
        active_request_.SerializeAsString()}},
      base::BindRepeating(&StartNextPendingRequestTask::DidStoreActiveRequest,
                          weak_factory_.GetWeakPtr()));
}

void StartNextPendingRequestTask::DidStoreActiveRequest(
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      break;
    case DatabaseStatus::kFailed:
    case DatabaseStatus::kNotFound:
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
  }

  next_request_ = base::MakeRefCounted<BackgroundFetchRequestInfo>(
      active_request_.request_index(),
      DeserializeFetchRequestFromString(active_request_.serialized_request()),
      active_request_.request_body_size());
  next_request_->SetDownloadGuid(active_request_.download_guid());

  // Delete the pending request.
  service_worker_context()->ClearRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      {PendingRequestKey(pending_request_.unique_id(),
                         pending_request_.request_index())},
      base::BindOnce(&StartNextPendingRequestTask::DidDeletePendingRequest,
                     weak_factory_.GetWeakPtr()));
}

void StartNextPendingRequestTask::DidDeletePendingRequest(
    blink::ServiceWorkerStatusCode status) {
  if (ToDatabaseStatus(status) != DatabaseStatus::kOk) {
    SetStorageErrorAndFinish(
        BackgroundFetchStorageError::kServiceWorkerStorageError);
  } else {
    FinishWithError(blink::mojom::BackgroundFetchError::NONE);
  }
}

void StartNextPendingRequestTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  ReportStorageError();

  std::move(callback_).Run(error, std::move(next_request_));

  Finished();  // Destroys |this|.
}

std::string StartNextPendingRequestTask::HistogramName() const {
  return "StartNextPendingRequestTask";
}

}  // namespace background_fetch
}  // namespace content
