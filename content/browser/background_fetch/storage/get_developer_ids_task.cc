// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_developer_ids_task.h"

#include <vector>

#include "base/bind.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"

namespace content {
namespace background_fetch {

GetDeveloperIdsTask::GetDeveloperIdsTask(
    DatabaseTaskHost* host,
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback)
    : DatabaseTask(host),
      service_worker_registration_id_(service_worker_registration_id),
      origin_(origin),
      callback_(std::move(callback)) {}

GetDeveloperIdsTask::~GetDeveloperIdsTask() = default;

void GetDeveloperIdsTask::Start() {
  service_worker_context()->GetRegistrationUserKeysAndDataByKeyPrefix(
      service_worker_registration_id_, {kActiveRegistrationUniqueIdKeyPrefix},
      base::BindOnce(&GetDeveloperIdsTask::DidGetUniqueIds,
                     weak_factory_.GetWeakPtr()));
}

void GetDeveloperIdsTask::DidGetUniqueIds(
    const base::flat_map<std::string, std::string>& data_map,
    blink::ServiceWorkerStatusCode status) {
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
  ReportStorageError();
  std::move(callback_).Run(error, std::move(developer_ids_));
  Finished();  // Destroys |this|.
}

std::string GetDeveloperIdsTask::HistogramName() const {
  return "GetDeveloperIdsTask";
}

}  // namespace background_fetch
}  // namespace content
