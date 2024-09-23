// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_developer_ids_task.h"

#include <vector>

#include "base/functional/bind.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"

namespace content {
namespace background_fetch {

GetDeveloperIdsTask::GetDeveloperIdsTask(
    DatabaseTaskHost* host,
    int64_t service_worker_registration_id,
    const blink::StorageKey& storage_key,
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback)
    : DatabaseTask(host),
      service_worker_registration_id_(service_worker_registration_id),
      storage_key_(storage_key),
      callback_(std::move(callback)) {}

GetDeveloperIdsTask::~GetDeveloperIdsTask() = default;

void GetDeveloperIdsTask::Start() {
  service_worker_context()->FindReadyRegistrationForIdOnly(
      service_worker_registration_id_,
      base::BindOnce(&GetDeveloperIdsTask::DidGetServiceWorkerRegistration,
                     weak_factory_.GetWeakPtr()));
}

void GetDeveloperIdsTask::DidGetServiceWorkerRegistration(
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (ToDatabaseStatus(status) != DatabaseStatus::kOk || !registration) {
    SetStorageErrorAndFinish(
        BackgroundFetchStorageError::kServiceWorkerStorageError);
    return;
  }

  // TODO(crbug.com/40177656): Move this check into the SW context.
  if (registration->key() != storage_key_) {
    SetStorageErrorAndFinish(
        BackgroundFetchStorageError::kServiceWorkerStorageError);
    return;
  }

  service_worker_context()->GetRegistrationUserKeysAndDataByKeyPrefix(
      service_worker_registration_id_, {kActiveRegistrationUniqueIdKeyPrefix},
      base::BindOnce(&GetDeveloperIdsTask::DidGetUniqueIds,
                     weak_factory_.GetWeakPtr()));
}

void GetDeveloperIdsTask::DidGetUniqueIds(
    blink::ServiceWorkerStatusCode status,
    const base::flat_map<std::string, std::string>& data_map) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
      FinishWithError(blink::mojom::BackgroundFetchError::NONE);
      break;
    case DatabaseStatus::kOk: {
      developer_ids_.reserve(data_map.size());
      for (const auto& pair : data_map)
        developer_ids_.push_back(pair.first);
      FinishWithError(blink::mojom::BackgroundFetchError::NONE);
      break;
    }
    case DatabaseStatus::kFailed:
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      break;
  }
}

void GetDeveloperIdsTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  std::move(callback_).Run(error, std::move(developer_ids_));
  Finished();  // Destroys |this|.
}

}  // namespace background_fetch
}  // namespace content
