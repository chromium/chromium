// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/delete_registration_task.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
namespace background_fetch {

namespace {
#if DCHECK_IS_ON()
// Checks that the |ActiveRegistrationUniqueIdKey| either does not exist, or is
// associated with a different |unique_id| than the given one which should have
// been already marked for deletion.
void DCheckRegistrationNotActive(const std::string& unique_id,
                                 const std::vector<std::string>& data,
                                 blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      DCHECK_EQ(1u, data.size());
      DCHECK_NE(unique_id, data[0])
          << "Must call MarkRegistrationForDeletion before DeleteRegistration";
      return;
    case DatabaseStatus::kFailed:
    case DatabaseStatus::kNotFound:
      return;
  }
}
#endif  // DCHECK_IS_ON()
}  // namespace

DeleteRegistrationTask::DeleteRegistrationTask(
    DatabaseTaskHost* host,
    int64_t service_worker_registration_id,
    const blink::StorageKey& storage_key,
    const std::string& unique_id,
    HandleBackgroundFetchErrorCallback callback)
    : DatabaseTask(host),
      service_worker_registration_id_(service_worker_registration_id),
      storage_key_(storage_key),
      unique_id_(unique_id),
      callback_(std::move(callback)) {}

DeleteRegistrationTask::~DeleteRegistrationTask() = default;

void DeleteRegistrationTask::Start() {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "DeleteRegistrationTask::Start",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2u, base::BindOnce(&DeleteRegistrationTask::FinishWithError,
                         weak_factory_.GetWeakPtr(),
                         blink::mojom::BackgroundFetchError::NONE));

#if DCHECK_IS_ON()
  // Get the registration |developer_id| to check it was deactivated.
  service_worker_context()->GetRegistrationUserData(
      service_worker_registration_id_, {RegistrationKey(unique_id_)},
      base::BindOnce(&DeleteRegistrationTask::DidGetRegistration,
                     weak_factory_.GetWeakPtr(), barrier_closure));
#else
  DidGetRegistration(barrier_closure, {}, blink::ServiceWorkerStatusCode::kOk);
#endif  // DCHECK_IS_ON()

  DeleteCache(storage_key_, unique_id_, trace_id,
              base::BindOnce(&DeleteRegistrationTask::DidDeleteCache,
                             weak_factory_.GetWeakPtr(),
                             std::move(barrier_closure), trace_id));
}

void DeleteRegistrationTask::DidGetRegistration(
    base::OnceClosure done_closure,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
#if DCHECK_IS_ON()
  if (ToDatabaseStatus(status) == DatabaseStatus::kOk) {
    DCHECK_EQ(1u, data.size());
    proto::BackgroundFetchMetadata metadata_proto;
    if (metadata_proto.ParseFromString(data[0]) &&
        metadata_proto.registration().has_developer_id()) {
      service_worker_context()->GetRegistrationUserData(
          service_worker_registration_id_,
          {ActiveRegistrationUniqueIdKey(
              metadata_proto.registration().developer_id())},
          base::BindOnce(&DCheckRegistrationNotActive, unique_id_));
    } else {
      // Service worker database has been corrupted. Abandon all fetches.
      SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
      AbandonFetches(service_worker_registration_id_);
      std::move(done_closure).Run();
      return;
    }
  }
#endif  // DCHECK_IS_ON()

  std::vector<std::string> deletion_key_prefixes{
      RegistrationKey(unique_id_),           UIOptionsKey(unique_id_),
      PendingRequestKeyPrefix(unique_id_),   ActiveRequestKeyPrefix(unique_id_),
      CompletedRequestKeyPrefix(unique_id_), StorageVersionKey(unique_id_)};

  service_worker_context()->ClearRegistrationUserDataByKeyPrefixes(
      service_worker_registration_id_, std::move(deletion_key_prefixes),
      base::BindOnce(&DeleteRegistrationTask::DidDeleteRegistration,
                     weak_factory_.GetWeakPtr(), std::move(done_closure)));
}

void DeleteRegistrationTask::DidDeleteRegistration(
    base::OnceClosure done_closure,
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
    case DatabaseStatus::kNotFound:
      break;
    case DatabaseStatus::kFailed:
      SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
      break;
  }
  std::move(done_closure).Run();
}

void DeleteRegistrationTask::DidDeleteCache(
    base::OnceClosure done_closure,
    int64_t trace_id,
    blink::mojom::CacheStorageError error) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "DeleteRegistrationTask::DidDeleteCache",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN);
  if (error != blink::mojom::CacheStorageError::kSuccess &&
      error != blink::mojom::CacheStorageError::kErrorNotFound) {
    SetStorageError(BackgroundFetchStorageError::kCacheStorageError);
  }
  std::move(done_closure).Run();
}

void DeleteRegistrationTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  if (HasStorageError())
    error = blink::mojom::BackgroundFetchError::STORAGE_ERROR;
  std::move(callback_).Run(error);
  Finished();  // Destroys |this|.
}

}  // namespace background_fetch
}  // namespace content
