// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_metadata_task.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {
namespace background_fetch {

GetMetadataTask::GetMetadataTask(DatabaseTaskHost* host,
                                 int64_t service_worker_registration_id,
                                 const blink::StorageKey& storage_key,
                                 const std::string& developer_id,
                                 GetMetadataCallback callback)
    : DatabaseTask(host),
      service_worker_registration_id_(service_worker_registration_id),
      storage_key_(storage_key),
      developer_id_(developer_id),
      callback_(std::move(callback)) {}

GetMetadataTask::~GetMetadataTask() = default;

void GetMetadataTask::Start() {
  service_worker_context()->GetRegistrationUserData(
      service_worker_registration_id_,
      {ActiveRegistrationUniqueIdKey(developer_id_)},
      base::BindOnce(&GetMetadataTask::DidGetUniqueId,
                     weak_factory_.GetWeakPtr()));
}

void GetMetadataTask::DidGetUniqueId(const std::vector<std::string>& data,
                                     blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
      FinishWithError(blink::mojom::BackgroundFetchError::INVALID_ID);
      return;
    case DatabaseStatus::kOk:
      DCHECK_EQ(1u, data.size());
      service_worker_context()->GetRegistrationUserData(
          service_worker_registration_id_, {RegistrationKey(data[0])},
          base::BindOnce(&GetMetadataTask::DidGetMetadata,
                         weak_factory_.GetWeakPtr()));
      return;
    case DatabaseStatus::kFailed:
      FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      return;
  }
}

void GetMetadataTask::DidGetMetadata(const std::vector<std::string>& data,
                                     blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
      // The database is corrupt as there's no registration data despite there
      // being an active developer_id pointing to it.
      FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      return;
    case DatabaseStatus::kOk:
      DCHECK_EQ(1u, data.size());
      ProcessMetadata(data[0]);
      return;
    case DatabaseStatus::kFailed:
      FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      return;
  }
}

void GetMetadataTask::ProcessMetadata(const std::string& metadata) {
  metadata_proto_ = std::make_unique<proto::BackgroundFetchMetadata>();
  if (!metadata_proto_->ParseFromString(metadata)) {
    FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
    return;
  }

  const auto& registration_proto = metadata_proto_->registration();
  auto meta_storage_key = GetMetadataStorageKey(*metadata_proto_);
  if (registration_proto.developer_id() != developer_id_ ||
      meta_storage_key != storage_key_) {
    FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
    return;
  }

  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
}

void GetMetadataTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  // We want to return a nullptr instead of an empty proto in case of an error.
  if (error != blink::mojom::BackgroundFetchError::NONE)
    metadata_proto_.reset();

  std::move(callback_).Run(error, std::move(metadata_proto_));
  Finished();
}

}  // namespace background_fetch
}  // namespace content
