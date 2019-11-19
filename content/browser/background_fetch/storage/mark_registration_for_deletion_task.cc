// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/mark_registration_for_deletion_task.h"

#include <utility>

#include "base/bind.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"

namespace content {
namespace background_fetch {

MarkRegistrationForDeletionTask::MarkRegistrationForDeletionTask(
    DatabaseTaskHost* host,
    const BackgroundFetchRegistrationId& registration_id,
    bool check_for_failure,
    MarkRegistrationForDeletionCallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      check_for_failure_(check_for_failure),
      callback_(std::move(callback)) {}

MarkRegistrationForDeletionTask::~MarkRegistrationForDeletionTask() = default;

void MarkRegistrationForDeletionTask::Start() {
  // Look up if there is already an active |unique_id| entry for this
  // |developer_id|.
  service_worker_context()->GetRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      {ActiveRegistrationUniqueIdKey(registration_id_.developer_id()),
       RegistrationKey(registration_id_.unique_id())},
      base::BindOnce(&MarkRegistrationForDeletionTask::DidGetActiveUniqueId,
                     weak_factory_.GetWeakPtr()));
}

void MarkRegistrationForDeletionTask::DidGetActiveUniqueId(
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      break;
    case DatabaseStatus::kNotFound:
      FinishWithError(blink::mojom::BackgroundFetchError::INVALID_ID);
      return;
    case DatabaseStatus::kFailed:
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
  }

  DCHECK_EQ(2u, data.size());

  // If the |unique_id| does not match, then the registration identified by
  // |registration_id_.unique_id()| was already deactivated.
  if (data[0] != registration_id_.unique_id()) {
    FinishWithError(blink::mojom::BackgroundFetchError::INVALID_ID);
    return;
  }

  proto::BackgroundFetchMetadata metadata_proto;
  if (metadata_proto.ParseFromString(data[1])) {
    // Mark registration as no longer active.
    service_worker_context()->ClearRegistrationUserData(
        registration_id_.service_worker_registration_id(),
        {ActiveRegistrationUniqueIdKey(registration_id_.developer_id())},
        base::BindOnce(&MarkRegistrationForDeletionTask::DidDeactivate,
                       weak_factory_.GetWeakPtr()));
  } else {
    // Service worker database has been corrupted. Abandon fetches.
    SetStorageErrorAndFinish(
        BackgroundFetchStorageError::kServiceWorkerStorageError);
    return;
  }
}

void MarkRegistrationForDeletionTask::DidDeactivate(
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
    case DatabaseStatus::kNotFound:
      break;
    case DatabaseStatus::kFailed:
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
  }

  // If CleanupTask runs after this, it shouldn't clean up the
  // |unique_id| as there may still be JavaScript references to it.
  ref_counted_unique_ids().emplace(registration_id_.unique_id());

  if (check_for_failure_) {
    // Check if there is an error in the responses to report.
    service_worker_context()->GetRegistrationUserDataByKeyPrefix(
        registration_id_.service_worker_registration_id(),
        {CompletedRequestKeyPrefix(registration_id_.unique_id())},
        base::BindOnce(
            &MarkRegistrationForDeletionTask::DidGetCompletedRequests,
            weak_factory_.GetWeakPtr()));
  } else {
    FinishWithError(blink::mojom::BackgroundFetchError::NONE);
  }
}

void MarkRegistrationForDeletionTask::DidGetCompletedRequests(
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
    case DatabaseStatus::kNotFound:
      break;
    case DatabaseStatus::kFailed:
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
  }

  for (const std::string& serialized_completed_request : data) {
    proto::BackgroundFetchCompletedRequest completed_request;
    if (!completed_request.ParseFromString(serialized_completed_request)) {
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
    }

    if (completed_request.failure_reason() !=
        proto::BackgroundFetchRegistration::NONE) {
      bool did_convert = MojoFailureReasonFromRegistrationProto(
          completed_request.failure_reason(), &failure_reason_);
      if (!did_convert) {
        SetStorageErrorAndFinish(
            BackgroundFetchStorageError::kServiceWorkerStorageError);
        return;
      }
      break;
    }
  }

  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
}

void MarkRegistrationForDeletionTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  ReportStorageError();
  if (HasStorageError())
    AbandonFetches(registration_id_.service_worker_registration_id());
  std::move(callback_).Run(error, failure_reason_);
  Finished();  // Destroys |this|.
}

std::string MarkRegistrationForDeletionTask::HistogramName() const {
  return "MarkRegistrationForDeletionTask";
}

}  // namespace background_fetch
}  // namespace content
