// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_initialization_data_task.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/background_fetch/storage/image_helpers.h"
#include "content/browser/background_fetch/storage/mark_registration_for_deletion_task.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/fetch/fetch_api_request_proto.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

namespace content {
namespace background_fetch {

namespace {

// Base class with all the common implementation for the SubTasks
// needed in this file.
class InitializationSubTask : public DatabaseTask {
 public:
  // Holds data used by all SubTasks.
  struct SubTaskInit {
    SubTaskInit() = delete;
    ~SubTaskInit() = default;

    // Service Worker Database metadata.
    int64_t service_worker_registration_id;
    std::string unique_id;

    // The results to report.
    BackgroundFetchInitializationData* initialization_data;
  };

  InitializationSubTask(DatabaseTaskHost* host,
                        const SubTaskInit& sub_task_init,
                        base::OnceClosure done_closure)
      : DatabaseTask(host),
        sub_task_init_(sub_task_init),
        done_closure_(std::move(done_closure)) {
    DCHECK(sub_task_init_.initialization_data);
  }

  ~InitializationSubTask() override = default;

 protected:
  void FinishWithError(blink::mojom::BackgroundFetchError error) override {
    if (error != blink::mojom::BackgroundFetchError::NONE)
      sub_task_init_.initialization_data->error = error;
    std::move(done_closure_).Run();
    Finished();  // Destroys |this|.
  }

  SubTaskInit& sub_task_init() { return sub_task_init_; }

 private:
  SubTaskInit sub_task_init_;
  base::OnceClosure done_closure_;

  DISALLOW_COPY_AND_ASSIGN(InitializationSubTask);
};

// Fills the BackgroundFetchInitializationData with the most recent UI title.
class GetUIOptionsTask : public InitializationSubTask {
 public:
  GetUIOptionsTask(DatabaseTaskHost* host,
                   const SubTaskInit& sub_task_init,
                   base::OnceClosure done_closure)
      : InitializationSubTask(host, sub_task_init, std::move(done_closure)) {}

  ~GetUIOptionsTask() override = default;

  void Start() override {
    service_worker_context()->GetRegistrationUserData(
        sub_task_init().service_worker_registration_id,
        {UIOptionsKey(sub_task_init().unique_id)},
        base::BindOnce(&GetUIOptionsTask::DidGetUIOptions,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetUIOptions(const std::vector<std::string>& data,
                       blink::ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kFailed:
        FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        return;
      case DatabaseStatus::kNotFound:
      case DatabaseStatus::kOk:
        break;
    }

    if (data.size() != 1u) {
      FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      return;
    }

    proto::BackgroundFetchUIOptions ui_options;
    if (!ui_options.ParseFromString(data[0])) {
      FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      return;
    }

    if (!ui_options.title().empty())
      sub_task_init().initialization_data->ui_title = ui_options.title();

    if (!ui_options.icon().empty()) {
      // Start an icon deserialization SubTask on another thread, then finish.
      DeserializeIcon(std::unique_ptr<std::string>(ui_options.release_icon()),
                      base::BindOnce(&GetUIOptionsTask::DidDeserializeIcon,
                                     weak_factory_.GetWeakPtr()));
    } else {
      FinishWithError(blink::mojom::BackgroundFetchError::NONE);
    }
  }

  void DidDeserializeIcon(SkBitmap icon) {
    sub_task_init().initialization_data->icon = std::move(icon);
    FinishWithError(blink::mojom::BackgroundFetchError::NONE);
  }

  base::WeakPtrFactory<GetUIOptionsTask> weak_factory_{this};  // Keep as last.
};

// Gets the number of completed fetches, the number of active fetches,
// and deletes inconsistencies in state transitions.
// 1. Get all completed requests.
// 2. Delete matching active requests.
// 3. Get active requests.
// 4. Delete matching pending requests.
class GetRequestsTask : public InitializationSubTask {
 public:
  GetRequestsTask(DatabaseTaskHost* host,
                  const SubTaskInit& sub_task_init,
                  base::OnceClosure done_closure)
      : InitializationSubTask(host, sub_task_init, std::move(done_closure)) {}

  ~GetRequestsTask() override = default;

  void Start() override {
    service_worker_context()->GetRegistrationUserDataByKeyPrefix(
        sub_task_init().service_worker_registration_id,
        CompletedRequestKeyPrefix(sub_task_init().unique_id),
        base::BindOnce(&GetRequestsTask::DidGetCompletedRequests,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetCompletedRequests(const std::vector<std::string>& data,
                               blink::ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kFailed:
        FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        return;
      case DatabaseStatus::kNotFound:
      case DatabaseStatus::kOk:
        break;
    }

    sub_task_init().initialization_data->num_completed_requests = data.size();

    std::vector<std::string> active_requests_to_delete;
    active_requests_to_delete.reserve(data.size());
    for (const std::string& serialized_completed_request : data) {
      proto::BackgroundFetchCompletedRequest completed_request;
      if (!completed_request.ParseFromString(serialized_completed_request) ||
          sub_task_init().unique_id != completed_request.unique_id()) {
        FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        return;
      }

      active_requests_to_delete.push_back(ActiveRequestKey(
          completed_request.unique_id(), completed_request.request_index()));
    }

    if (active_requests_to_delete.empty()) {
      DidClearActiveRequests(blink::ServiceWorkerStatusCode::kOk);
      return;
    }

    service_worker_context()->ClearRegistrationUserData(
        sub_task_init().service_worker_registration_id,
        std::move(active_requests_to_delete),
        base::BindOnce(&GetRequestsTask::DidClearActiveRequests,
                       weak_factory_.GetWeakPtr()));
  }

  void DidClearActiveRequests(blink::ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kFailed:
        FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        return;
      case DatabaseStatus::kNotFound:
      case DatabaseStatus::kOk:
        break;
    }

    service_worker_context()->GetRegistrationUserDataByKeyPrefix(
        sub_task_init().service_worker_registration_id,
        ActiveRequestKeyPrefix(sub_task_init().unique_id),
        base::BindOnce(&GetRequestsTask::DidGetRemainingActiveRequests,
                       weak_factory_.GetWeakPtr()));
  }

  void DidGetRemainingActiveRequests(const std::vector<std::string>& data,
                                     blink::ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kFailed:
        FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        return;
      case DatabaseStatus::kNotFound:
      case DatabaseStatus::kOk:
        break;
    }

    std::vector<std::string> pending_requests_to_delete;
    pending_requests_to_delete.reserve(data.size());
    for (const std::string& serialized_active_request : data) {
      proto::BackgroundFetchActiveRequest active_request;
      if (!active_request.ParseFromString(serialized_active_request)) {
        FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        return;
      }
      DCHECK_EQ(sub_task_init().unique_id, active_request.unique_id());

      auto request_info = base::MakeRefCounted<BackgroundFetchRequestInfo>(
          active_request.request_index(),
          DeserializeFetchRequestFromString(
              active_request.serialized_request()),
          active_request.request_body_size());
      request_info->SetDownloadGuid(active_request.download_guid());

      sub_task_init().initialization_data->active_fetch_requests.push_back(
          std::move(request_info));

      pending_requests_to_delete.push_back(PendingRequestKey(
          active_request.unique_id(), active_request.request_index()));
    }

    if (pending_requests_to_delete.empty()) {
      DidClearPendingRequests(blink::ServiceWorkerStatusCode::kOk);
      return;
    }

    service_worker_context()->ClearRegistrationUserData(
        sub_task_init().service_worker_registration_id,
        std::move(pending_requests_to_delete),
        base::BindOnce(&GetRequestsTask::DidClearPendingRequests,
                       weak_factory_.GetWeakPtr()));
  }

  void DidClearPendingRequests(blink::ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kFailed:
        FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        return;
      case DatabaseStatus::kNotFound:
      case DatabaseStatus::kOk:
        break;
    }

    FinishWithError(blink::mojom::BackgroundFetchError::NONE);
  }

  base::WeakPtrFactory<GetRequestsTask> weak_factory_{this};  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(GetRequestsTask);
};

// Fills the BackgroundFetchInitializationData with all the relevant information
// stored in the BackgroundFetchMetadata proto.
class FillFromMetadataTask : public InitializationSubTask {
 public:
  FillFromMetadataTask(DatabaseTaskHost* host,
                       const SubTaskInit& sub_task_init,
                       base::OnceClosure done_closure)
      : InitializationSubTask(host, sub_task_init, std::move(done_closure)) {}

  ~FillFromMetadataTask() override = default;

  void Start() override {
    service_worker_context()->GetRegistrationUserDataByKeyPrefix(
        sub_task_init().service_worker_registration_id,
        {RegistrationKey(sub_task_init().unique_id)},
        base::BindOnce(&FillFromMetadataTask::DidGetMetadata,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetMetadata(const std::vector<std::string>& data,
                      blink::ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kFailed:
      case DatabaseStatus::kNotFound:
        FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        return;
      case DatabaseStatus::kOk:
        break;
    }

    if (data.size() != 1u) {
      FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      return;
    }

    proto::BackgroundFetchMetadata metadata;
    if (!metadata.ParseFromString(data[0])) {
      FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      return;
    }

    if (sub_task_init().unique_id != metadata.registration().unique_id()) {
      FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      return;
    }

    // Fill BackgroundFetchRegistrationId.
    sub_task_init().initialization_data->registration_id =
        BackgroundFetchRegistrationId(
            sub_task_init().service_worker_registration_id,
            url::Origin::Create(GURL(metadata.origin())),
            metadata.registration().developer_id(),
            metadata.registration().unique_id());

    // Fill BackgroundFetchRegistrationData.
    auto& registration_data =
        sub_task_init().initialization_data->registration_data;
    ToBackgroundFetchRegistration(metadata, registration_data.get());

    // Total number of requests.
    sub_task_init().initialization_data->num_requests = metadata.num_fetches();
    // Fill BackgroundFetchOptions.
    auto& options = sub_task_init().initialization_data->options;
    options->title = metadata.options().title();
    options->download_total = metadata.options().download_total();
    options->icons.reserve(metadata.options().icons_size());
    for (const auto& icon : metadata.options().icons()) {
      blink::Manifest::ImageResource ir;
      ir.src = GURL(icon.src());
      ir.type = base::ASCIIToUTF16(icon.type());

      ir.sizes.reserve(icon.sizes_size());
      for (const auto& size : icon.sizes())
        ir.sizes.emplace_back(size.width(), size.height());

      ir.purpose.reserve(icon.purpose_size());
      for (auto purpose : icon.purpose()) {
        switch (purpose) {
          case proto::BackgroundFetchOptions_ImageResource_Purpose_ANY:
            ir.purpose.push_back(blink::Manifest::ImageResource::Purpose::ANY);
            break;
          case proto::BackgroundFetchOptions_ImageResource_Purpose_BADGE:
            ir.purpose.push_back(
                blink::Manifest::ImageResource::Purpose::BADGE);
            break;
        }
      }
    }

    FinishWithError(blink::mojom::BackgroundFetchError::NONE);
  }

  base::WeakPtrFactory<FillFromMetadataTask> weak_factory_{
      this};  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(FillFromMetadataTask);
};

// Asynchronously calls the SubTasks required to collect all the information for
// the BackgroundFetchInitializationData.
class FillBackgroundFetchInitializationDataTask : public InitializationSubTask {
 public:
  FillBackgroundFetchInitializationDataTask(DatabaseTaskHost* host,
                                            const SubTaskInit& sub_task_init,
                                            base::OnceClosure done_closure)
      : InitializationSubTask(host, sub_task_init, std::move(done_closure)) {}

  ~FillBackgroundFetchInitializationDataTask() override = default;

  void Start() override {
    // We need 3 queries to get the initialization data. These are wrapped
    // in a BarrierClosure to avoid querying them serially.
    // 1. Metadata
    // 2. Request statuses and state sanitization
    // 3. UI Options (+ icon deserialization)
    base::RepeatingClosure barrier_closure = base::BarrierClosure(
        3u, base::BindOnce(&FillBackgroundFetchInitializationDataTask::
                               DidQueryInitializationData,
                           weak_factory_.GetWeakPtr()));
    AddSubTask(std::make_unique<FillFromMetadataTask>(this, sub_task_init(),
                                                      barrier_closure));
    AddSubTask(std::make_unique<GetRequestsTask>(this, sub_task_init(),
                                                 barrier_closure));
    AddSubTask(std::make_unique<GetUIOptionsTask>(this, sub_task_init(),
                                                  barrier_closure));
  }

  void DidQueryInitializationData() {
    FinishWithError(sub_task_init().initialization_data->error);
  }

 private:
  base::WeakPtrFactory<FillBackgroundFetchInitializationDataTask> weak_factory_{
      this};  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(FillBackgroundFetchInitializationDataTask);
};

}  // namespace

BackgroundFetchInitializationData::BackgroundFetchInitializationData() =
    default;

BackgroundFetchInitializationData::BackgroundFetchInitializationData(
    BackgroundFetchInitializationData&&) = default;

BackgroundFetchInitializationData::~BackgroundFetchInitializationData() =
    default;

GetInitializationDataTask::GetInitializationDataTask(
    DatabaseTaskHost* host,
    GetInitializationDataCallback callback)
    : DatabaseTask(host), callback_(std::move(callback)) {}

GetInitializationDataTask::~GetInitializationDataTask() = default;

void GetInitializationDataTask::Start() {
  service_worker_context()->GetUserDataForAllRegistrationsByKeyPrefix(
      kActiveRegistrationUniqueIdKeyPrefix,
      base::BindOnce(&GetInitializationDataTask::DidGetRegistrations,
                     weak_factory_.GetWeakPtr()));
}

void GetInitializationDataTask::DidGetRegistrations(
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kFailed:
      FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      return;
    case DatabaseStatus::kNotFound:
    case DatabaseStatus::kOk:
      break;
  }

  if (user_data.empty()) {
    FinishWithError(blink::mojom::BackgroundFetchError::NONE);
    return;
  }

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      user_data.size(),
      base::BindOnce(&GetInitializationDataTask::FinishWithError,
                     weak_factory_.GetWeakPtr(),
                     blink::mojom::BackgroundFetchError::NONE));

  for (const auto& ud : user_data) {
    auto insertion_result = initialization_data_map_.emplace(
        ud.second, BackgroundFetchInitializationData());
    DCHECK(insertion_result.second);  // Check unique_id is in fact unique.

    AddSubTask(std::make_unique<FillBackgroundFetchInitializationDataTask>(
        this,
        InitializationSubTask::SubTaskInit{
            ud.first, ud.second,
            /* initialization_data= */ &insertion_result.first->second},
        barrier_closure));
  }
}
void GetInitializationDataTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  std::vector<BackgroundFetchInitializationData> results;
  results.reserve(initialization_data_map_.size());

  for (auto& data : initialization_data_map_) {
    if (data.second.error == blink::mojom::BackgroundFetchError::NONE) {
      // If we successfully extracted all the data, move it to the
      // initialization vector to be handed over to create a controller.
      results.emplace_back(std::move(data.second));
    } else if (!data.second.registration_id.developer_id().empty()) {
      // There was an error in getting the initialization data
      // (e.g. corrupt data, SWDB error). If the Developer ID of the fetch
      // is available, mark the registration for deletion.
      // Note that the Developer ID isn't available if the metadata extraction
      // failed.
      // TODO(crbug.com/865388): Getting the Developer ID should be possible
      // since it is part of the key for when we got the Unique ID.
      AddDatabaseTask(std::make_unique<MarkRegistrationForDeletionTask>(
          data_manager(), data.second.registration_id,
          /* check_for_failure= */ false, base::DoNothing()));
    }

    if (data.second.error ==
        blink::mojom::BackgroundFetchError::STORAGE_ERROR) {
      // The subtasks only access the Service Worker storage, so if there is
      // a storage error, that would be the cause.
      SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
    }
  }

  ReportStorageError();

  std::move(callback_).Run(error, std::move(results));
  Finished();  // Destroys |this|.
}

std::string GetInitializationDataTask::HistogramName() const {
  return "GetInitializationDataTask";
}

}  // namespace background_fetch
}  // namespace content
