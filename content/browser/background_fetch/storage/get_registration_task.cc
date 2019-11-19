// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_registration_task.h"

#include "base/bind.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/background_fetch/storage/get_metadata_task.h"

namespace content {
namespace background_fetch {

GetRegistrationTask::GetRegistrationTask(DatabaseTaskHost* host,
                                         int64_t service_worker_registration_id,
                                         const url::Origin& origin,
                                         const std::string& developer_id,
                                         GetRegistrationCallback callback)
    : DatabaseTask(host),
      service_worker_registration_id_(service_worker_registration_id),
      origin_(origin),
      developer_id_(developer_id),
      callback_(std::move(callback)) {}

GetRegistrationTask::~GetRegistrationTask() = default;

void GetRegistrationTask::Start() {
  AddSubTask(std::make_unique<GetMetadataTask>(
      this, service_worker_registration_id_, origin_, developer_id_,
      base::BindOnce(&GetRegistrationTask::DidGetMetadata,
                     weak_factory_.GetWeakPtr())));
}

void GetRegistrationTask::DidGetMetadata(
    blink::mojom::BackgroundFetchError error,
    std::unique_ptr<proto::BackgroundFetchMetadata> metadata_proto) {
  metadata_proto_ = std::move(metadata_proto);
  if (error == blink::mojom::BackgroundFetchError::STORAGE_ERROR)
    SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
  FinishWithError(error);
}

void GetRegistrationTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  auto registration_data = blink::mojom::BackgroundFetchRegistrationData::New();
  BackgroundFetchRegistrationId registration_id;

  if (error == blink::mojom::BackgroundFetchError::NONE) {
    DCHECK(metadata_proto_);

    bool converted = ToBackgroundFetchRegistration(*metadata_proto_,
                                                   registration_data.get());
    if (!converted) {
      // Database corrupted.
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
    }

    registration_id = BackgroundFetchRegistrationId(
        service_worker_registration_id_, origin_, developer_id_,
        metadata_proto_->registration().unique_id());
  }

  ReportStorageError();

  std::move(callback_).Run(error, std::move(registration_id),
                           std::move(registration_data));
  Finished();  // Destroys |this|.
}

std::string GetRegistrationTask::HistogramName() const {
  return "GetRegistrationTask";
}

}  // namespace background_fetch
}  // namespace content
