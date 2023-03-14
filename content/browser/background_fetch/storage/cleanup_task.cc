// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/cleanup_task.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/background_fetch/storage/delete_registration_task.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {
namespace background_fetch {

namespace {
void EmptyErrorHandler(blink::mojom::BackgroundFetchError) {}
}  // namespace

CleanupTask::CleanupTask(DatabaseTaskHost* host) : DatabaseTask(host) {}

CleanupTask::~CleanupTask() = default;

void CleanupTask::Start() {
  service_worker_context()->GetUserDataForAllRegistrationsByKeyPrefix(
      kRegistrationKeyPrefix, base::BindOnce(&CleanupTask::DidGetRegistrations,
                                             weak_factory_.GetWeakPtr()));
}

void CleanupTask::DidGetRegistrations(
    const std::vector<std::pair<int64_t, std::string>>& registration_data,
    blink::ServiceWorkerStatusCode status) {
  if (ToDatabaseStatus(status) != DatabaseStatus::kOk ||
      registration_data.empty()) {
    FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
    return;
  }

  service_worker_context()->GetUserDataForAllRegistrationsByKeyPrefix(
      kActiveRegistrationUniqueIdKeyPrefix,
      base::BindOnce(&CleanupTask::DidGetActiveUniqueIds,
                     weak_factory_.GetWeakPtr(), registration_data));
}

void CleanupTask::DidGetActiveUniqueIds(
    const std::vector<std::pair<int64_t, std::string>>& registration_data,
    const std::vector<std::pair<int64_t, std::string>>& active_unique_id_data,
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

  auto active_unique_ids = base::MakeFlatSet<std::string>(
      active_unique_id_data, {}, &std::pair<int64_t, std::string>::second);

  for (const auto& entry : registration_data) {
    int64_t service_worker_registration_id = entry.first;
    proto::BackgroundFetchMetadata metadata_proto;
    if (metadata_proto.ParseFromString(entry.second)) {
      if (metadata_proto.registration().has_unique_id()) {
        const std::string& unique_id =
            metadata_proto.registration().unique_id();
        if (!active_unique_ids.count(unique_id) &&
            !ref_counted_unique_ids().count(unique_id)) {
          // This |unique_id| can be safely cleaned up. Re-use
          // DeleteRegistrationTask for the actual deletion logic.
          AddDatabaseTask(std::make_unique<DeleteRegistrationTask>(
              data_manager(), service_worker_registration_id,
              GetMetadataStorageKey(metadata_proto), unique_id,
              base::BindOnce(&EmptyErrorHandler)));
        }
      }
    }
  }

  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
  return;
}

void CleanupTask::FinishWithError(blink::mojom::BackgroundFetchError error) {
  Finished();  // Destroys |this|.
}

}  // namespace background_fetch
}  // namespace content
