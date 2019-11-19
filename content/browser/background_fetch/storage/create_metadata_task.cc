// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/create_metadata_task.h"

#include <numeric>
#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_data_manager_observer.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/background_fetch/storage/image_helpers.h"
#include "content/browser/background_fetch/storage/mark_registration_for_deletion_task.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/fetch/fetch_api_request_proto.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace content {
namespace background_fetch {

namespace {

// TODO(crbug.com/889401): Consider making this configurable by finch.
constexpr size_t kRegistrationLimitPerOrigin = 5u;

// Finds the number of active registrations associated with the provided origin,
// and compares it with the limit to determine whether this registration can go
// through.
class CanCreateRegistrationTask : public DatabaseTask {
 public:
  using CanCreateRegistrationCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError, bool)>;

  CanCreateRegistrationTask(DatabaseTaskHost* host,
                            const url::Origin& origin,
                            CanCreateRegistrationCallback callback)
      : DatabaseTask(host), origin_(origin), callback_(std::move(callback)) {}

  ~CanCreateRegistrationTask() override = default;

  void Start() override {
    service_worker_context()->GetRegistrationsForOrigin(
        origin_,
        base::BindOnce(&CanCreateRegistrationTask::DidGetRegistrationsForOrigin,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetRegistrationsForOrigin(
      blink::ServiceWorkerStatusCode status,
      const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
          registrations) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kOk:
        break;
      case DatabaseStatus::kNotFound:
        FinishWithError(blink::mojom::BackgroundFetchError::NONE);
        return;
      case DatabaseStatus::kFailed:
        FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        return;
    }

    std::set<int64_t> registration_ids;
    for (const auto& registration : registrations)
      registration_ids.insert(registration->id());

    base::RepeatingClosure barrier_closure = base::BarrierClosure(
        registration_ids.size(),
        base::BindOnce(&CanCreateRegistrationTask::FinishWithError,
                       weak_factory_.GetWeakPtr(),
                       blink::mojom::BackgroundFetchError::NONE));

    for (int64_t registration_id : registration_ids) {
      service_worker_context()->GetRegistrationUserDataByKeyPrefix(
          registration_id, kActiveRegistrationUniqueIdKeyPrefix,
          base::BindOnce(&CanCreateRegistrationTask::DidGetActiveRegistrations,
                         weak_factory_.GetWeakPtr(), barrier_closure));
    }
  }

  void DidGetActiveRegistrations(base::OnceClosure done_closure,
                                 const std::vector<std::string>& data,
                                 blink::ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kNotFound:
        std::move(done_closure).Run();
        return;
      case DatabaseStatus::kOk:
        num_active_registrations_ += data.size();
        std::move(done_closure).Run();
        return;
      case DatabaseStatus::kFailed:
        FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        return;
    }
  }

  void FinishWithError(blink::mojom::BackgroundFetchError error) override {
    std::move(callback_).Run(
        error, num_active_registrations_ < kRegistrationLimitPerOrigin);
    Finished();  // Destroys |this|.
  }

  url::Origin origin_;
  CanCreateRegistrationCallback callback_;

  // The number of existing registrations found for |origin_|.
  size_t num_active_registrations_ = 0u;

  base::WeakPtrFactory<CanCreateRegistrationTask> weak_factory_{
      this};  // Keep as last.
};

}  // namespace

CreateMetadataTask::CreateMetadataTask(
    DatabaseTaskHost* host,
    const BackgroundFetchRegistrationId& registration_id,
    std::vector<blink::mojom::FetchAPIRequestPtr> requests,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    bool start_paused,
    CreateMetadataCallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      requests_(std::move(requests)),
      options_(std::move(options)),
      icon_(icon),
      start_paused_(start_paused),
      callback_(std::move(callback)) {}

CreateMetadataTask::~CreateMetadataTask() = default;

void CreateMetadataTask::Start() {
  // Check if the registration can be created.
  AddSubTask(std::make_unique<CanCreateRegistrationTask>(
      this, registration_id_.origin(),
      base::BindOnce(&CreateMetadataTask::DidGetCanCreateRegistration,
                     weak_factory_.GetWeakPtr())));
}

void CreateMetadataTask::DidGetCanCreateRegistration(
    blink::mojom::BackgroundFetchError error,
    bool can_create) {
  if (error == blink::mojom::BackgroundFetchError::STORAGE_ERROR) {
    SetStorageErrorAndFinish(
        BackgroundFetchStorageError::kServiceWorkerStorageError);
    return;
  }

  DCHECK_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  if (!can_create) {
    FinishWithError(
        blink::mojom::BackgroundFetchError::REGISTRATION_LIMIT_EXCEEDED);
    return;
  }

  // Check if there is enough quota to download the data first.
  if (options_->download_total > 0) {
    IsQuotaAvailable(registration_id_.origin(), options_->download_total,
                     base::BindOnce(&CreateMetadataTask::DidGetIsQuotaAvailable,
                                    weak_factory_.GetWeakPtr()));
  } else {
    // Proceed with the fetch.
    GetRegistrationUniqueId();
  }
}

void CreateMetadataTask::DidGetIsQuotaAvailable(bool is_available) {
  if (!is_available)
    FinishWithError(blink::mojom::BackgroundFetchError::QUOTA_EXCEEDED);
  else
    GetRegistrationUniqueId();
}

void CreateMetadataTask::GetRegistrationUniqueId() {
  service_worker_context()->GetRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      {ActiveRegistrationUniqueIdKey(registration_id_.developer_id())},
      base::BindOnce(&CreateMetadataTask::DidGetUniqueId,
                     weak_factory_.GetWeakPtr()));
}

void CreateMetadataTask::DidGetUniqueId(const std::vector<std::string>& data,
                                        blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
      break;
    case DatabaseStatus::kOk:
      // Can't create a registration since there is already an active
      // registration with the same |developer_id|. It must be deactivated
      // (completed/failed/aborted) first.
      FinishWithError(
          blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID);
      return;
    case DatabaseStatus::kFailed:
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
  }

  InitializeMetadataProto();

  if (ShouldPersistIcon(icon_)) {
    // Serialize the icon, then store all the metadata.
    SerializeIcon(icon_, base::BindOnce(&CreateMetadataTask::DidSerializeIcon,
                                        weak_factory_.GetWeakPtr()));
  } else {
    // Directly store the metadata.
    StoreMetadata();
  }
}

void CreateMetadataTask::InitializeMetadataProto() {
  metadata_proto_ = std::make_unique<proto::BackgroundFetchMetadata>();

  // Set BackgroundFetchRegistration fields.
  auto* registration_proto = metadata_proto_->mutable_registration();
  registration_proto->set_unique_id(registration_id_.unique_id());
  registration_proto->set_developer_id(registration_id_.developer_id());
  registration_proto->set_download_total(options_->download_total);
  registration_proto->set_result(
      proto::BackgroundFetchRegistration_BackgroundFetchResult_UNSET);
  registration_proto->set_failure_reason(
      proto::BackgroundFetchRegistration_BackgroundFetchFailureReason_NONE);
  registration_proto->set_upload_total(
      std::accumulate(requests_.begin(), requests_.end(), 0u,
                      [](uint64_t sum, const auto& request) {
                        return sum + (request->blob ? request->blob->size : 0u);
                      }));

  // Set Options fields.
  auto* options_proto = metadata_proto_->mutable_options();
  options_proto->set_title(options_->title);
  options_proto->set_download_total(options_->download_total);
  for (const auto& icon : options_->icons) {
    auto* image_resource_proto = options_proto->add_icons();

    image_resource_proto->set_src(icon.src.spec());

    for (const auto& size : icon.sizes) {
      auto* size_proto = image_resource_proto->add_sizes();
      size_proto->set_width(size.width());
      size_proto->set_height(size.height());
    }

    image_resource_proto->set_type(base::UTF16ToASCII(icon.type));

    for (const auto& purpose : icon.purpose) {
      switch (purpose) {
        case blink::Manifest::ImageResource::Purpose::ANY:
          image_resource_proto->add_purpose(
              proto::BackgroundFetchOptions_ImageResource_Purpose_ANY);
          break;
        case blink::Manifest::ImageResource::Purpose::BADGE:
          image_resource_proto->add_purpose(
              proto::BackgroundFetchOptions_ImageResource_Purpose_BADGE);
          break;
        case blink::Manifest::ImageResource::Purpose::MASKABLE:
          image_resource_proto->add_purpose(
              proto::BackgroundFetchOptions_ImageResource_Purpose_MASKABLE);
          break;
      }
    }
  }

  // Set other metadata fields.
  metadata_proto_->set_origin(registration_id_.origin().Serialize());
  metadata_proto_->set_creation_microseconds_since_unix_epoch(
      (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds());
  metadata_proto_->set_num_fetches(requests_.size());
}

void CreateMetadataTask::DidSerializeIcon(std::string serialized_icon) {
  serialized_icon_ = std::move(serialized_icon);
  StoreMetadata();
}

void CreateMetadataTask::StoreMetadata() {
  DCHECK(metadata_proto_);
  std::vector<std::pair<std::string, std::string>> entries;
  // - One BackgroundFetchPendingRequest per request
  // - DeveloperId -> UniqueID
  // - BackgroundFetchMetadata
  // - BackgroundFetchUIOptions
  // - BackgroundFetchStorageVersion
  entries.reserve(requests_.size() + 4u);

  std::string serialized_metadata_proto;

  if (!metadata_proto_->SerializeToString(&serialized_metadata_proto)) {
    SetStorageErrorAndFinish(
        BackgroundFetchStorageError::kServiceWorkerStorageError);
    return;
  }

  std::string serialized_ui_options_proto;
  proto::BackgroundFetchUIOptions ui_options;
  ui_options.set_title(options_->title);
  if (!serialized_icon_.empty())
    ui_options.set_icon(std::move(serialized_icon_));

  if (!ui_options.SerializeToString(&serialized_ui_options_proto)) {
    SetStorageErrorAndFinish(
        BackgroundFetchStorageError::kServiceWorkerStorageError);
    return;
  }

  entries.emplace_back(
      ActiveRegistrationUniqueIdKey(registration_id_.developer_id()),
      registration_id_.unique_id());
  entries.emplace_back(RegistrationKey(registration_id_.unique_id()),
                       std::move(serialized_metadata_proto));
  entries.emplace_back(UIOptionsKey(registration_id_.unique_id()),
                       serialized_ui_options_proto);
  entries.emplace_back(
      StorageVersionKey(registration_id_.unique_id()),
      base::NumberToString(proto::BackgroundFetchStorageVersion::SV_CURRENT));

  // Signed integers are used for request indexes to avoid unsigned gotchas.
  for (int i = 0; i < base::checked_cast<int>(requests_.size()); i++) {
    proto::BackgroundFetchPendingRequest pending_request_proto;
    pending_request_proto.set_unique_id(registration_id_.unique_id());
    pending_request_proto.set_request_index(i);
    pending_request_proto.set_serialized_request(
        SerializeFetchRequestToString(*requests_[i]));
    if (requests_[i]->blob)
      pending_request_proto.set_request_body_size(requests_[i]->blob->size);
    entries.emplace_back(PendingRequestKey(registration_id_.unique_id(), i),
                         pending_request_proto.SerializeAsString());
  }

  service_worker_context()->StoreRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      registration_id_.origin().GetURL(), entries,
      base::BindOnce(&CreateMetadataTask::DidStoreMetadata,
                     weak_factory_.GetWeakPtr()));
}

void CreateMetadataTask::DidStoreMetadata(
    blink::ServiceWorkerStatusCode status) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "CacheStorageMigrationTask::DidStoreMetadata",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      break;
    case DatabaseStatus::kFailed:
    case DatabaseStatus::kNotFound:
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
  }

  // Create cache entries.
  CacheStorageHandle cache_storage = GetOrOpenCacheStorage(registration_id_);
  cache_storage.value()->OpenCache(
      /* cache_name= */ registration_id_.unique_id(), trace_id,
      base::BindOnce(&CreateMetadataTask::DidOpenCache,
                     weak_factory_.GetWeakPtr(), trace_id));
}

void CreateMetadataTask::DidOpenCache(int64_t trace_id,
                                      CacheStorageCacheHandle handle,
                                      blink::mojom::CacheStorageError error) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "CacheStorageMigrationTask::DidReopenCache",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (error != blink::mojom::CacheStorageError::kSuccess) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  DCHECK(handle.value());

  // Create batch PUT operations instead of putting them one-by-one.
  std::vector<blink::mojom::BatchOperationPtr> operations;
  operations.reserve(requests_.size());
  for (size_t i = 0; i < requests_.size(); i++) {
    auto operation = blink::mojom::BatchOperation::New();
    operation->operation_type = blink::mojom::OperationType::kPut;
    requests_[i]->url =
        MakeCacheUrlUnique(requests_[i]->url, registration_id_.unique_id(), i);
    operation->request = std::move(requests_[i]);
    // Empty response.
    operation->response = blink::mojom::FetchAPIResponse::New();
    operations.push_back(std::move(operation));
  }

  handle.value()->BatchOperation(
      std::move(operations), trace_id,
      base::BindOnce(&CreateMetadataTask::DidStoreRequests,
                     weak_factory_.GetWeakPtr(), handle.Clone()),
      base::DoNothing());
}

void CreateMetadataTask::DidStoreRequests(
    CacheStorageCacheHandle handle,
    blink::mojom::CacheStorageVerboseErrorPtr error) {
  if (error->value != blink::mojom::CacheStorageError::kSuccess) {
    // Delete the metadata in the SWDB.
    AddDatabaseTask(std::make_unique<MarkRegistrationForDeletionTask>(
        data_manager(), registration_id_, /* check_for_failure= */ false,
        base::DoNothing()));
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
}

void CreateMetadataTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  auto registration_data = blink::mojom::BackgroundFetchRegistrationData::New();

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

    for (auto& observer : data_manager()->observers()) {
      observer.OnRegistrationCreated(registration_id_, *registration_data,
                                     options_.Clone(), icon_, requests_.size(),
                                     start_paused_);
    }
  }

  ReportStorageError();

  std::move(callback_).Run(error, std::move(registration_data));
  Finished();  // Destroys |this|.
}

std::string CreateMetadataTask::HistogramName() const {
  return "CreateMetadataTask";
}

}  // namespace background_fetch
}  // namespace content
