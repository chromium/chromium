// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

namespace content {

namespace {

void RunSoon(const base::Location& from_here, base::OnceClosure closure) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(from_here, std::move(closure));
}

void CompleteFindNow(scoped_refptr<ServiceWorkerRegistration> registration,
                     blink::ServiceWorkerStatusCode status,
                     ServiceWorkerStorage::FindRegistrationCallback callback) {
  if (registration && registration->is_deleted()) {
    // It's past the point of no return and no longer findable.
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorNotFound,
                            nullptr);
    return;
  }
  std::move(callback).Run(status, std::move(registration));
}

void CompleteFindSoon(const base::Location& from_here,
                      scoped_refptr<ServiceWorkerRegistration> registration,
                      blink::ServiceWorkerStatusCode status,
                      ServiceWorkerStorage::FindRegistrationCallback callback) {
  RunSoon(from_here, base::BindOnce(&CompleteFindNow, std::move(registration),
                                    status, std::move(callback)));
}

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("Database");
const base::FilePath::CharType kDiskCacheName[] =
    FILE_PATH_LITERAL("ScriptCache");

blink::ServiceWorkerStatusCode DatabaseStatusToStatusCode(
    ServiceWorkerDatabase::Status status) {
  switch (status) {
    case ServiceWorkerDatabase::STATUS_OK:
      return blink::ServiceWorkerStatusCode::kOk;
    case ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND:
      return blink::ServiceWorkerStatusCode::kErrorNotFound;
    case ServiceWorkerDatabase::STATUS_ERROR_MAX:
      NOTREACHED();
      FALLTHROUGH;
    default:
      return blink::ServiceWorkerStatusCode::kErrorFailed;
  }
}

void DidUpdateNavigationPreloadState(
    ServiceWorkerStorage::StatusCallback callback,
    ServiceWorkerDatabase::Status status) {
  std::move(callback).Run(DatabaseStatusToStatusCode(status));
}

}  // namespace

ServiceWorkerStorage::InitialData::InitialData()
    : next_registration_id(blink::mojom::kInvalidServiceWorkerRegistrationId),
      next_version_id(blink::mojom::kInvalidServiceWorkerVersionId),
      next_resource_id(ServiceWorkerConsts::kInvalidServiceWorkerResourceId) {}

ServiceWorkerStorage::InitialData::~InitialData() {
}

ServiceWorkerStorage::DidDeleteRegistrationParams::DidDeleteRegistrationParams(
    int64_t registration_id,
    GURL origin,
    StatusCallback callback)
    : registration_id(registration_id),
      origin(origin),
      callback(std::move(callback)) {}

ServiceWorkerStorage::DidDeleteRegistrationParams::
    ~DidDeleteRegistrationParams() {}

ServiceWorkerStorage::~ServiceWorkerStorage() {
  ClearSessionOnlyOrigins();
  weak_factory_.InvalidateWeakPtrs();
  database_task_runner_->DeleteSoon(FROM_HERE, std::move(database_));
}

// static
std::unique_ptr<ServiceWorkerStorage> ServiceWorkerStorage::Create(
    const base::FilePath& user_data_directory,
    ServiceWorkerContextCore* context,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy) {
  return base::WrapUnique(new ServiceWorkerStorage(
      user_data_directory, context, std::move(database_task_runner),
      quota_manager_proxy, special_storage_policy));
}

// static
std::unique_ptr<ServiceWorkerStorage> ServiceWorkerStorage::Create(
    ServiceWorkerContextCore* context,
    ServiceWorkerStorage* old_storage) {
  return base::WrapUnique(
      new ServiceWorkerStorage(old_storage->user_data_directory_, context,
                               old_storage->database_task_runner_,
                               old_storage->quota_manager_proxy_.get(),
                               old_storage->special_storage_policy_.get()));
}

void ServiceWorkerStorage::FindRegistrationForClientUrl(
    const GURL& client_url,
    FindRegistrationCallback callback) {
  DCHECK(!client_url.has_ref());
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      CompleteFindNow(scoped_refptr<ServiceWorkerRegistration>(),
                      blink::ServiceWorkerStatusCode::kErrorAbort,
                      std::move(callback));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::FindRegistrationForClientUrl,
          weak_factory_.GetWeakPtr(), client_url, std::move(callback)));
      TRACE_EVENT_INSTANT1(
          "ServiceWorker",
          "ServiceWorkerStorage::FindRegistrationForClientUrl:LazyInitialize",
          TRACE_EVENT_SCOPE_THREAD, "URL", client_url.spec());
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  // See if there are any stored registrations for the origin.
  if (!base::Contains(registered_origins_, client_url.GetOrigin())) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForClientUrl(client_url);
    blink::ServiceWorkerStatusCode status =
        installing_registration
            ? blink::ServiceWorkerStatusCode::kOk
            : blink::ServiceWorkerStatusCode::kErrorNotFound;
    TRACE_EVENT_INSTANT2(
        "ServiceWorker",
        "ServiceWorkerStorage::FindRegistrationForClientUrl:CheckInstalling",
        TRACE_EVENT_SCOPE_THREAD, "URL", client_url.spec(), "Status",
        blink::ServiceWorkerStatusToString(status));
    CompleteFindNow(std::move(installing_registration), status,
                    std::move(callback));
    return;
  }

  // To connect this TRACE_EVENT with the callback, TimeTicks is used for
  // callback id.
  int64_t callback_id = base::TimeTicks::Now().ToInternalValue();
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerStorage::FindRegistrationForClientUrl",
                           callback_id, "URL", client_url.spec());
  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FindForClientUrlInDB, database_.get(),
          base::ThreadTaskRunnerHandle::Get(), client_url,
          base::BindOnce(&ServiceWorkerStorage::DidFindRegistrationForClientUrl,
                         weak_factory_.GetWeakPtr(), client_url,
                         std::move(callback), callback_id)));
}

void ServiceWorkerStorage::FindRegistrationForScope(
    const GURL& scope,
    FindRegistrationCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      CompleteFindSoon(FROM_HERE, scoped_refptr<ServiceWorkerRegistration>(),
                       blink::ServiceWorkerStatusCode::kErrorAbort,
                       std::move(callback));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::FindRegistrationForScope,
          weak_factory_.GetWeakPtr(), scope, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  // See if there are any stored registrations for the origin.
  if (!base::Contains(registered_origins_, scope.GetOrigin())) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForScope(scope);
    blink::ServiceWorkerStatusCode installing_status =
        installing_registration
            ? blink::ServiceWorkerStatusCode::kOk
            : blink::ServiceWorkerStatusCode::kErrorNotFound;
    CompleteFindSoon(FROM_HERE, std::move(installing_registration),
                     installing_status, std::move(callback));
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FindForScopeInDB, database_.get(),
          base::ThreadTaskRunnerHandle::Get(), scope,
          base::BindOnce(&ServiceWorkerStorage::DidFindRegistrationForScope,
                         weak_factory_.GetWeakPtr(), scope,
                         std::move(callback))));
}

ServiceWorkerRegistration* ServiceWorkerStorage::GetUninstallingRegistration(
    const GURL& scope) {
  if (state_ != STORAGE_STATE_INITIALIZED)
    return nullptr;
  for (const auto& registration : uninstalling_registrations_) {
    if (registration.second->scope() == scope) {
      DCHECK(registration.second->is_uninstalling());
      return registration.second.get();
    }
  }
  return nullptr;
}

void ServiceWorkerStorage::FindRegistrationForId(
    int64_t registration_id,
    const GURL& origin,
    FindRegistrationCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      CompleteFindNow(scoped_refptr<ServiceWorkerRegistration>(),
                      blink::ServiceWorkerStatusCode::kErrorAbort,
                      std::move(callback));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::FindRegistrationForId,
                         weak_factory_.GetWeakPtr(), registration_id, origin,
                         std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  // See if there are any stored registrations for the origin.
  if (!base::Contains(registered_origins_, origin)) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForId(registration_id);
    CompleteFindNow(installing_registration,
                    installing_registration
                        ? blink::ServiceWorkerStatusCode::kOk
                        : blink::ServiceWorkerStatusCode::kErrorNotFound,
                    std::move(callback));
    return;
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration) {
    CompleteFindNow(std::move(registration),
                    blink::ServiceWorkerStatusCode::kOk, std::move(callback));
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FindForIdInDB, database_.get(), base::ThreadTaskRunnerHandle::Get(),
          registration_id, origin,
          base::BindOnce(&ServiceWorkerStorage::DidFindRegistrationForId,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void ServiceWorkerStorage::FindRegistrationForIdOnly(
    int64_t registration_id,
    FindRegistrationCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      CompleteFindNow(nullptr, blink::ServiceWorkerStatusCode::kErrorAbort,
                      std::move(callback));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::FindRegistrationForIdOnly,
          weak_factory_.GetWeakPtr(), registration_id, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration) {
    // Delegate to FindRegistrationForId to make sure the same subset of live
    // registrations is returned.
    // TODO(mek): CompleteFindNow should really do all the required checks, so
    // calling that directly here should be enough.
    FindRegistrationForId(registration_id, registration->scope().GetOrigin(),
                          std::move(callback));
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FindForIdOnlyInDB, database_.get(),
          base::ThreadTaskRunnerHandle::Get(), registration_id,
          base::BindOnce(&ServiceWorkerStorage::DidFindRegistrationForId,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void ServiceWorkerStorage::GetRegistrationsForOrigin(
    const GURL& origin,
    GetRegistrationsCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(
          FROM_HERE,
          base::BindOnce(
              std::move(callback), blink::ServiceWorkerStatusCode::kErrorAbort,
              std::vector<scoped_refptr<ServiceWorkerRegistration>>()));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::GetRegistrationsForOrigin,
          weak_factory_.GetWeakPtr(), origin, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  RegistrationList* registrations = new RegistrationList;
  std::vector<ResourceList>* resource_lists = new std::vector<ResourceList>;
  base::PostTaskAndReplyWithResult(
      database_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::GetRegistrationsForOrigin,
                     base::Unretained(database_.get()), origin, registrations,
                     resource_lists),
      base::BindOnce(&ServiceWorkerStorage::DidGetRegistrationsForOrigin,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     base::Owned(registrations), base::Owned(resource_lists),
                     origin));
}

void ServiceWorkerStorage::GetAllRegistrationsInfos(
    GetRegistrationsInfosCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             blink::ServiceWorkerStatusCode::kErrorAbort,
                             std::vector<ServiceWorkerRegistrationInfo>()));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::GetAllRegistrationsInfos,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  RegistrationList* registrations = new RegistrationList;
  base::PostTaskAndReplyWithResult(
      database_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::GetAllRegistrations,
                     base::Unretained(database_.get()), registrations),
      base::BindOnce(&ServiceWorkerStorage::DidGetAllRegistrationsInfos,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     base::Owned(registrations)));
}

void ServiceWorkerStorage::StoreRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version,
    StatusCallback callback) {
  DCHECK(registration);
  DCHECK(version);

  DCHECK(state_ == STORAGE_STATE_INITIALIZED ||
         state_ == STORAGE_STATE_DISABLED)
      << state_;
  if (IsDisabled()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }

  DCHECK_NE(version->fetch_handler_existence(),
            ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN);
  DCHECK_EQ(registration->status(), ServiceWorkerRegistration::Status::kIntact);

  ServiceWorkerDatabase::RegistrationData data;
  data.registration_id = registration->id();
  data.scope = registration->scope();
  data.script = version->script_url();
  data.script_type = version->script_type();
  data.update_via_cache = registration->update_via_cache();
  data.has_fetch_handler = version->fetch_handler_existence() ==
                           ServiceWorkerVersion::FetchHandlerExistence::EXISTS;
  data.version_id = version->version_id();
  data.last_update_check = registration->last_update_check();
  data.is_active = (version == registration->active_version());
  if (version->origin_trial_tokens())
    data.origin_trial_tokens = *version->origin_trial_tokens();
  data.navigation_preload_state = registration->navigation_preload_state();
  data.script_response_time = version->GetInfo().script_response_time;
  for (const blink::mojom::WebFeature feature : version->used_features())
    data.used_features.insert(feature);

  ResourceList resources;
  version->script_cache_map()->GetResources(&resources);

  if (resources.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }

  uint64_t resources_total_size_bytes = 0;
  for (const auto& resource : resources) {
    DCHECK_GE(resource.size_bytes, 0);
    resources_total_size_bytes += resource.size_bytes;
  }
  data.resources_total_size_bytes = resources_total_size_bytes;

  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteRegistrationInDB, database_.get(),
                     base::ThreadTaskRunnerHandle::Get(), data, resources,
                     base::BindOnce(&ServiceWorkerStorage::DidStoreRegistration,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(callback), data)));
}

void ServiceWorkerStorage::UpdateToActiveState(
    ServiceWorkerRegistration* registration,
    StatusCallback callback) {
  DCHECK(registration);

  DCHECK(state_ == STORAGE_STATE_INITIALIZED ||
         state_ == STORAGE_STATE_DISABLED)
      << state_;
  if (IsDisabled()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }

  base::PostTaskAndReplyWithResult(
      database_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::UpdateVersionToActive,
                     base::Unretained(database_.get()), registration->id(),
                     registration->scope().GetOrigin()),
      base::BindOnce(&ServiceWorkerStorage::DidUpdateToActiveState,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorage::UpdateLastUpdateCheckTime(
    ServiceWorkerRegistration* registration,
    StatusCallback callback) {
  DCHECK(registration);
  DCHECK(state_ == STORAGE_STATE_INITIALIZED ||
         state_ == STORAGE_STATE_DISABLED)
      << state_;
  if (IsDisabled()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }

  base::PostTaskAndReplyWithResult(
      database_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::UpdateLastCheckTime,
                     base::Unretained(database_.get()), registration->id(),
                     registration->scope().GetOrigin(),
                     registration->last_update_check()),
      base::BindOnce(
          [](StatusCallback callback, ServiceWorkerDatabase::Status status) {
            std::move(callback).Run(DatabaseStatusToStatusCode(status));
          },
          std::move(callback)));
}

void ServiceWorkerStorage::UpdateNavigationPreloadEnabled(
    int64_t registration_id,
    const GURL& origin,
    bool enable,
    StatusCallback callback) {
  DCHECK(state_ == STORAGE_STATE_INITIALIZED ||
         state_ == STORAGE_STATE_DISABLED)
      << state_;
  if (IsDisabled()) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }

  base::PostTaskAndReplyWithResult(
      database_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::UpdateNavigationPreloadEnabled,
                     base::Unretained(database_.get()), registration_id, origin,
                     enable),
      base::BindOnce(&DidUpdateNavigationPreloadState, std::move(callback)));
}

void ServiceWorkerStorage::UpdateNavigationPreloadHeader(
    int64_t registration_id,
    const GURL& origin,
    const std::string& value,
    StatusCallback callback) {
  DCHECK(state_ == STORAGE_STATE_INITIALIZED ||
         state_ == STORAGE_STATE_DISABLED)
      << state_;
  if (IsDisabled()) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }

  base::PostTaskAndReplyWithResult(
      database_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::UpdateNavigationPreloadHeader,
                     base::Unretained(database_.get()), registration_id, origin,
                     value),
      base::BindOnce(&DidUpdateNavigationPreloadState, std::move(callback)));
}

void ServiceWorkerStorage::DeleteRegistration(
    scoped_refptr<ServiceWorkerRegistration> registration,
    const GURL& origin,
    StatusCallback callback) {
  DCHECK(state_ == STORAGE_STATE_INITIALIZED ||
         state_ == STORAGE_STATE_DISABLED)
      << state_;
  if (IsDisabled()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }

  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();

  DCHECK(!registration->is_deleted())
      << "attempt to delete a registration twice";

  auto params = std::make_unique<DidDeleteRegistrationParams>(
      registration->id(), origin, std::move(callback));

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DeleteRegistrationFromDB, database_.get(),
          base::ThreadTaskRunnerHandle::Get(), registration->id(), origin,
          base::BindOnce(&ServiceWorkerStorage::DidDeleteRegistration,
                         weak_factory_.GetWeakPtr(), std::move(params))));

  uninstalling_registrations_[registration->id()] = registration;
  registration->SetStatus(ServiceWorkerRegistration::Status::kUninstalling);
}

void ServiceWorkerStorage::PerformStorageCleanup(base::OnceClosure callback) {
  DCHECK(state_ == STORAGE_STATE_INITIALIZED ||
         state_ == STORAGE_STATE_DISABLED)
      << state_;
  if (IsDisabled()) {
    RunSoon(FROM_HERE, std::move(callback));
    return;
  }

  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();

  database_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&PerformStorageCleanupInDB, database_.get()),
      std::move(callback));
}

std::unique_ptr<ServiceWorkerResponseReader>
ServiceWorkerStorage::CreateResponseReader(int64_t resource_id) {
  return base::WrapUnique(
      new ServiceWorkerResponseReader(resource_id, disk_cache()->GetWeakPtr()));
}

std::unique_ptr<ServiceWorkerResponseWriter>
ServiceWorkerStorage::CreateResponseWriter(int64_t resource_id) {
  return base::WrapUnique(
      new ServiceWorkerResponseWriter(resource_id, disk_cache()->GetWeakPtr()));
}

std::unique_ptr<ServiceWorkerResponseMetadataWriter>
ServiceWorkerStorage::CreateResponseMetadataWriter(int64_t resource_id) {
  return base::WrapUnique(new ServiceWorkerResponseMetadataWriter(
      resource_id, disk_cache()->GetWeakPtr()));
}

void ServiceWorkerStorage::StoreUncommittedResourceId(int64_t resource_id) {
  DCHECK_NE(ServiceWorkerConsts::kInvalidServiceWorkerResourceId, resource_id);
  DCHECK(STORAGE_STATE_INITIALIZED == state_ ||
         STORAGE_STATE_DISABLED == state_)
      << state_;
  if (IsDisabled())
    return;

  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();

  base::PostTaskAndReplyWithResult(
      database_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::WriteUncommittedResourceIds,
                     base::Unretained(database_.get()),
                     std::set<int64_t>(&resource_id, &resource_id + 1)),
      base::BindOnce(&ServiceWorkerStorage::DidWriteUncommittedResourceIds,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerStorage::DoomUncommittedResource(int64_t resource_id) {
  DCHECK_NE(ServiceWorkerConsts::kInvalidServiceWorkerResourceId, resource_id);
  DCHECK(STORAGE_STATE_INITIALIZED == state_ ||
         STORAGE_STATE_DISABLED == state_)
      << state_;
  if (IsDisabled())
    return;
  DoomUncommittedResources(std::set<int64_t>(&resource_id, &resource_id + 1));
}

void ServiceWorkerStorage::DoomUncommittedResources(
    const std::set<int64_t>& resource_ids) {
  DCHECK(STORAGE_STATE_INITIALIZED == state_ ||
         STORAGE_STATE_DISABLED == state_)
      << state_;
  if (IsDisabled())
    return;

  base::PostTaskAndReplyWithResult(
      database_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::PurgeUncommittedResourceIds,
                     base::Unretained(database_.get()), resource_ids),
      base::BindOnce(&ServiceWorkerStorage::DidPurgeUncommittedResourceIds,
                     weak_factory_.GetWeakPtr(), resource_ids));
}

void ServiceWorkerStorage::StoreUserData(
    int64_t registration_id,
    const GURL& origin,
    const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
    StatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             blink::ServiceWorkerStatusCode::kErrorAbort));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::StoreUserData, weak_factory_.GetWeakPtr(),
          registration_id, origin, key_value_pairs, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      key_value_pairs.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }
  for (const auto& kv : key_value_pairs) {
    if (kv.first.empty()) {
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             blink::ServiceWorkerStatusCode::kErrorFailed));
      return;
    }
  }

  base::PostTaskAndReplyWithResult(
      database_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::WriteUserData,
                     base::Unretained(database_.get()), registration_id, origin,
                     key_value_pairs),
      base::BindOnce(&ServiceWorkerStorage::DidStoreUserData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorage::GetUserData(int64_t registration_id,
                                       const std::vector<std::string>& keys,
                                       GetUserDataCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback), std::vector<std::string>(),
                             blink::ServiceWorkerStatusCode::kErrorAbort));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(&ServiceWorkerStorage::GetUserData,
                                    weak_factory_.GetWeakPtr(), registration_id,
                                    keys, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      keys.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback), std::vector<std::string>(),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }
  for (const std::string& key : keys) {
    if (key.empty()) {
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback), std::vector<std::string>(),
                             blink::ServiceWorkerStatusCode::kErrorFailed));
      return;
    }
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerStorage::GetUserDataInDB, database_.get(),
          base::ThreadTaskRunnerHandle::Get(), registration_id, keys,
          base::BindOnce(&ServiceWorkerStorage::DidGetUserData,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void ServiceWorkerStorage::GetUserDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserDataCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback), std::vector<std::string>(),
                             blink::ServiceWorkerStatusCode::kErrorAbort));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::GetUserDataByKeyPrefix,
                         weak_factory_.GetWeakPtr(), registration_id,
                         key_prefix, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback), std::vector<std::string>(),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }
  if (key_prefix.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback), std::vector<std::string>(),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerStorage::GetUserDataByKeyPrefixInDB, database_.get(),
          base::ThreadTaskRunnerHandle::Get(), registration_id, key_prefix,
          base::BindOnce(&ServiceWorkerStorage::DidGetUserData,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void ServiceWorkerStorage::GetUserKeysAndDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserKeysAndDataCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             base::flat_map<std::string, std::string>(),
                             blink::ServiceWorkerStatusCode::kErrorAbort));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::GetUserKeysAndDataByKeyPrefix,
                         weak_factory_.GetWeakPtr(), registration_id,
                         key_prefix, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      key_prefix.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           base::flat_map<std::string, std::string>(),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerStorage::GetUserKeysAndDataByKeyPrefixInDB,
          database_.get(), base::ThreadTaskRunnerHandle::Get(), registration_id,
          key_prefix,
          base::BindOnce(&ServiceWorkerStorage::DidGetUserKeysAndData,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void ServiceWorkerStorage::ClearUserData(int64_t registration_id,
                                         const std::vector<std::string>& keys,
                                         StatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             blink::ServiceWorkerStatusCode::kErrorAbort));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(&ServiceWorkerStorage::ClearUserData,
                                    weak_factory_.GetWeakPtr(), registration_id,
                                    keys, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      keys.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }
  for (const std::string& key : keys) {
    if (key.empty()) {
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             blink::ServiceWorkerStatusCode::kErrorFailed));
      return;
    }
  }

  base::PostTaskAndReplyWithResult(
      database_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::DeleteUserData,
                     base::Unretained(database_.get()), registration_id, keys),
      base::BindOnce(&ServiceWorkerStorage::DidDeleteUserData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorage::ClearUserDataByKeyPrefixes(
    int64_t registration_id,
    const std::vector<std::string>& key_prefixes,
    StatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             blink::ServiceWorkerStatusCode::kErrorAbort));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::ClearUserDataByKeyPrefixes,
                         weak_factory_.GetWeakPtr(), registration_id,
                         key_prefixes, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      key_prefixes.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }
  for (const std::string& key_prefix : key_prefixes) {
    if (key_prefix.empty()) {
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             blink::ServiceWorkerStatusCode::kErrorFailed));
      return;
    }
  }

  base::PostTaskAndReplyWithResult(
      database_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::DeleteUserDataByKeyPrefixes,
                     base::Unretained(database_.get()), registration_id,
                     key_prefixes),
      base::BindOnce(&ServiceWorkerStorage::DidDeleteUserData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorage::GetUserDataForAllRegistrations(
    const std::string& key,
    GetUserDataForAllRegistrationsCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             std::vector<std::pair<int64_t, std::string>>(),
                             blink::ServiceWorkerStatusCode::kErrorAbort));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::GetUserDataForAllRegistrations,
                         weak_factory_.GetWeakPtr(), key, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (key.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           std::vector<std::pair<int64_t, std::string>>(),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerStorage::GetUserDataForAllRegistrationsInDB,
          database_.get(), base::ThreadTaskRunnerHandle::Get(), key,
          base::BindOnce(
              &ServiceWorkerStorage::DidGetUserDataForAllRegistrations,
              weak_factory_.GetWeakPtr(), std::move(callback))));
}

void ServiceWorkerStorage::GetUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    GetUserDataForAllRegistrationsCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             std::vector<std::pair<int64_t, std::string>>(),
                             blink::ServiceWorkerStatusCode::kErrorAbort));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::GetUserDataForAllRegistrationsByKeyPrefix,
          weak_factory_.GetWeakPtr(), key_prefix, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (key_prefix.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           std::vector<std::pair<int64_t, std::string>>(),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerStorage::GetUserDataForAllRegistrationsByKeyPrefixInDB,
          database_.get(), base::ThreadTaskRunnerHandle::Get(), key_prefix,
          base::BindOnce(
              &ServiceWorkerStorage::DidGetUserDataForAllRegistrations,
              weak_factory_.GetWeakPtr(), std::move(callback))));
}

void ServiceWorkerStorage::ClearUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    StatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             blink::ServiceWorkerStatusCode::kErrorAbort));
      return;
    case STORAGE_STATE_INITIALIZING:  // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::ClearUserDataForAllRegistrationsByKeyPrefix,
          weak_factory_.GetWeakPtr(), key_prefix, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (key_prefix.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }

  base::PostTaskAndReplyWithResult(
      database_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &ServiceWorkerDatabase::DeleteUserDataForAllRegistrationsByKeyPrefix,
          base::Unretained(database_.get()), key_prefix),
      base::BindOnce(&ServiceWorkerStorage::DidDeleteUserData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorage::DeleteAndStartOver(StatusCallback callback) {
  Disable();

  // Will be used in DiskCacheImplDoneWithDisk()
  delete_and_start_over_callback_ = std::move(callback);

  // Won't get a callback about cleanup being done, so call it ourselves.
  if (!expecting_done_with_disk_on_disable_)
    DiskCacheImplDoneWithDisk();
}

void ServiceWorkerStorage::DiskCacheImplDoneWithDisk() {
  expecting_done_with_disk_on_disable_ = false;
  if (!delete_and_start_over_callback_.is_null()) {
    // Delete the database on the database thread.
    base::PostTaskAndReplyWithResult(
        database_task_runner_.get(), FROM_HERE,
        base::BindOnce(&ServiceWorkerDatabase::DestroyDatabase,
                       base::Unretained(database_.get())),
        base::BindOnce(&ServiceWorkerStorage::DidDeleteDatabase,
                       weak_factory_.GetWeakPtr(),
                       std::move(delete_and_start_over_callback_)));
  }
}

int64_t ServiceWorkerStorage::NewRegistrationId() {
  if (state_ == STORAGE_STATE_DISABLED)
    return blink::mojom::kInvalidServiceWorkerRegistrationId;
  DCHECK_EQ(STORAGE_STATE_INITIALIZED, state_);
  return next_registration_id_++;
}

int64_t ServiceWorkerStorage::NewVersionId() {
  if (state_ == STORAGE_STATE_DISABLED)
    return blink::mojom::kInvalidServiceWorkerVersionId;
  DCHECK_EQ(STORAGE_STATE_INITIALIZED, state_);
  return next_version_id_++;
}

int64_t ServiceWorkerStorage::NewResourceId() {
  if (state_ == STORAGE_STATE_DISABLED)
    return ServiceWorkerConsts::kInvalidServiceWorkerResourceId;
  DCHECK_EQ(STORAGE_STATE_INITIALIZED, state_);
  return next_resource_id_++;
}

void ServiceWorkerStorage::NotifyInstallingRegistration(
      ServiceWorkerRegistration* registration) {
  DCHECK(installing_registrations_.find(registration->id()) ==
         installing_registrations_.end());
  installing_registrations_[registration->id()] = registration;
}

void ServiceWorkerStorage::NotifyDoneInstallingRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version,
    blink::ServiceWorkerStatusCode status) {
  installing_registrations_.erase(registration->id());
  if (status != blink::ServiceWorkerStatusCode::kOk && version) {
    ResourceList resources;
    version->script_cache_map()->GetResources(&resources);

    std::set<int64_t> resource_ids;
    for (const auto& resource : resources)
      resource_ids.insert(resource.resource_id);
    DoomUncommittedResources(resource_ids);
  }
}

void ServiceWorkerStorage::NotifyDoneUninstallingRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerRegistration::Status new_status) {
  registration->SetStatus(new_status);
  uninstalling_registrations_.erase(registration->id());
}

void ServiceWorkerStorage::Disable() {
  state_ = STORAGE_STATE_DISABLED;
  if (disk_cache_)
    disk_cache_->Disable();
}

void ServiceWorkerStorage::PurgeResources(const ResourceList& resources) {
  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();
  StartPurgingResources(resources);
}

ServiceWorkerStorage::ServiceWorkerStorage(
    const base::FilePath& user_data_directory,
    ServiceWorkerContextCore* context,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy)
    : next_registration_id_(blink::mojom::kInvalidServiceWorkerRegistrationId),
      next_version_id_(blink::mojom::kInvalidServiceWorkerVersionId),
      next_resource_id_(ServiceWorkerConsts::kInvalidServiceWorkerResourceId),
      state_(STORAGE_STATE_UNINITIALIZED),
      expecting_done_with_disk_on_disable_(false),
      user_data_directory_(user_data_directory),
      context_(context),
      database_task_runner_(std::move(database_task_runner)),
      quota_manager_proxy_(quota_manager_proxy),
      special_storage_policy_(special_storage_policy),
      is_purge_pending_(false),
      has_checked_for_stale_resources_(false) {
  DCHECK(context_);
  database_.reset(new ServiceWorkerDatabase(GetDatabasePath()));
}

base::FilePath ServiceWorkerStorage::GetDatabasePath() {
  if (user_data_directory_.empty())
    return base::FilePath();
  return user_data_directory_
      .Append(ServiceWorkerContextCore::kServiceWorkerDirectory)
      .Append(kDatabaseName);
}

base::FilePath ServiceWorkerStorage::GetDiskCachePath() {
  if (user_data_directory_.empty())
    return base::FilePath();
  return user_data_directory_
      .Append(ServiceWorkerContextCore::kServiceWorkerDirectory)
      .Append(kDiskCacheName);
}

void ServiceWorkerStorage::LazyInitializeForTest() {
  DCHECK_NE(state_, STORAGE_STATE_DISABLED);

  if (state_ == STORAGE_STATE_INITIALIZED)
    return;
  base::RunLoop loop;
  LazyInitialize(loop.QuitClosure());
  loop.Run();
}

void ServiceWorkerStorage::SetPurgingCompleteCallbackForTest(
    base::OnceClosure callback) {
  purging_complete_callback_for_test_ = std::move(callback);
}

void ServiceWorkerStorage::LazyInitialize(base::OnceClosure callback) {
  DCHECK(state_ == STORAGE_STATE_UNINITIALIZED ||
         state_ == STORAGE_STATE_INITIALIZING)
      << state_;
  pending_tasks_.push_back(std::move(callback));
  if (state_ == STORAGE_STATE_INITIALIZING) {
    return;
  }

  state_ = STORAGE_STATE_INITIALIZING;
  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ReadInitialDataFromDB, database_.get(),
                     base::ThreadTaskRunnerHandle::Get(),
                     base::BindOnce(&ServiceWorkerStorage::DidReadInitialData,
                                    weak_factory_.GetWeakPtr())));
}

void ServiceWorkerStorage::DidReadInitialData(
    std::unique_ptr<InitialData> data,
    ServiceWorkerDatabase::Status status) {
  DCHECK(data);
  DCHECK_EQ(STORAGE_STATE_INITIALIZING, state_);

  if (status == ServiceWorkerDatabase::STATUS_OK) {
    next_registration_id_ = data->next_registration_id;
    next_version_id_ = data->next_version_id;
    next_resource_id_ = data->next_resource_id;
    registered_origins_.swap(data->origins);
    state_ = STORAGE_STATE_INITIALIZED;
    ServiceWorkerMetrics::RecordRegisteredOriginCount(
        registered_origins_.size());
  } else {
    DVLOG(2) << "Failed to initialize: "
             << ServiceWorkerDatabase::StatusToString(status);
    ScheduleDeleteAndStartOver();
  }

  for (base::OnceClosure& task : pending_tasks_)
    RunSoon(FROM_HERE, std::move(task));
  pending_tasks_.clear();
}

void ServiceWorkerStorage::DidFindRegistrationForClientUrl(
    const GURL& client_url,
    FindRegistrationCallback callback,
    int64_t callback_id,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources,
    ServiceWorkerDatabase::Status status) {
  if (status == ServiceWorkerDatabase::STATUS_OK) {
    ReturnFoundRegistration(std::move(callback), data, resources);
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker", "ServiceWorkerStorage::FindRegistrationForClientUrl",
        callback_id, "Status", ServiceWorkerDatabase::StatusToString(status));
    return;
  }

  if (status == ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForClientUrl(client_url);
    blink::ServiceWorkerStatusCode installing_status =
        installing_registration
            ? blink::ServiceWorkerStatusCode::kOk
            : blink::ServiceWorkerStatusCode::kErrorNotFound;
    std::move(callback).Run(installing_status,
                            std::move(installing_registration));
    TRACE_EVENT_ASYNC_END2(
        "ServiceWorker", "ServiceWorkerStorage::FindRegistrationForClientUrl",
        callback_id, "Status", ServiceWorkerDatabase::StatusToString(status),
        "Info",
        (installing_status == blink::ServiceWorkerStatusCode::kOk)
            ? "Installing registration is found"
            : "Any registrations are not found");
    return;
  }

  ScheduleDeleteAndStartOver();
  std::move(callback).Run(DatabaseStatusToStatusCode(status),
                          scoped_refptr<ServiceWorkerRegistration>());
  TRACE_EVENT_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerStorage::FindRegistrationForClientUrl",
      callback_id, "Status", ServiceWorkerDatabase::StatusToString(status));
}

void ServiceWorkerStorage::DidFindRegistrationForScope(
    const GURL& scope,
    FindRegistrationCallback callback,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources,
    ServiceWorkerDatabase::Status status) {
  if (status == ServiceWorkerDatabase::STATUS_OK) {
    ReturnFoundRegistration(std::move(callback), data, resources);
    return;
  }

  if (status == ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForScope(scope);
    blink::ServiceWorkerStatusCode installing_status =
        installing_registration
            ? blink::ServiceWorkerStatusCode::kOk
            : blink::ServiceWorkerStatusCode::kErrorNotFound;
    std::move(callback).Run(installing_status,
                            std::move(installing_registration));
    return;
  }

  ScheduleDeleteAndStartOver();
  std::move(callback).Run(DatabaseStatusToStatusCode(status),
                          scoped_refptr<ServiceWorkerRegistration>());
}

void ServiceWorkerStorage::DidFindRegistrationForId(
    FindRegistrationCallback callback,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources,
    ServiceWorkerDatabase::Status status) {
  if (status == ServiceWorkerDatabase::STATUS_OK) {
    ReturnFoundRegistration(std::move(callback), data, resources);
    return;
  }

  if (status == ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    // TODO(nhiroki): Find a registration in |installing_registrations_|.
    std::move(callback).Run(DatabaseStatusToStatusCode(status),
                            scoped_refptr<ServiceWorkerRegistration>());
    return;
  }

  ScheduleDeleteAndStartOver();
  std::move(callback).Run(DatabaseStatusToStatusCode(status),
                          scoped_refptr<ServiceWorkerRegistration>());
}

void ServiceWorkerStorage::ReturnFoundRegistration(
    FindRegistrationCallback callback,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources) {
  DCHECK(!resources.empty());
  scoped_refptr<ServiceWorkerRegistration> registration =
      GetOrCreateRegistration(data, resources);
  CompleteFindNow(std::move(registration), blink::ServiceWorkerStatusCode::kOk,
                  std::move(callback));
}

void ServiceWorkerStorage::DidGetRegistrationsForOrigin(
    GetRegistrationsCallback callback,
    RegistrationList* registration_data_list,
    std::vector<ResourceList>* resources_list,
    const GURL& origin_filter,
    ServiceWorkerDatabase::Status status) {
  DCHECK(registration_data_list);
  DCHECK(resources_list);
  DCHECK(origin_filter.is_valid());

  if (status != ServiceWorkerDatabase::STATUS_OK &&
      status != ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    ScheduleDeleteAndStartOver();
    std::move(callback).Run(
        DatabaseStatusToStatusCode(status),
        std::vector<scoped_refptr<ServiceWorkerRegistration>>());
    return;
  }

  // Add all stored registrations.
  std::set<int64_t> registration_ids;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  size_t index = 0;
  for (const auto& registration_data : *registration_data_list) {
    registration_ids.insert(registration_data.registration_id);
    registrations.push_back(GetOrCreateRegistration(
        registration_data, resources_list->at(index++)));
  }

  // Add unstored registrations that are being installed.
  for (const auto& registration : installing_registrations_) {
    if (registration.second->scope().GetOrigin() != origin_filter)
      continue;
    if (registration_ids.insert(registration.first).second)
      registrations.push_back(registration.second);
  }

  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk,
                          std::move(registrations));
}

void ServiceWorkerStorage::DidGetAllRegistrationsInfos(
    GetRegistrationsInfosCallback callback,
    RegistrationList* registration_data_list,
    ServiceWorkerDatabase::Status status) {
  DCHECK(registration_data_list);
  if (status != ServiceWorkerDatabase::STATUS_OK &&
      status != ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    ScheduleDeleteAndStartOver();
    std::move(callback).Run(DatabaseStatusToStatusCode(status),
                            std::vector<ServiceWorkerRegistrationInfo>());
    return;
  }

  // Add all stored registrations.
  std::set<int64_t> pushed_registrations;
  std::vector<ServiceWorkerRegistrationInfo> infos;
  for (const auto& registration_data : *registration_data_list) {
    const bool inserted =
        pushed_registrations.insert(registration_data.registration_id).second;
    DCHECK(inserted);

    ServiceWorkerRegistration* registration =
        context_->GetLiveRegistration(registration_data.registration_id);
    if (registration) {
      infos.push_back(registration->GetInfo());
      continue;
    }

    ServiceWorkerRegistrationInfo info;
    info.scope = registration_data.scope;
    info.update_via_cache = registration_data.update_via_cache;
    info.registration_id = registration_data.registration_id;
    info.stored_version_size_bytes =
        registration_data.resources_total_size_bytes;
    if (ServiceWorkerVersion* version =
            context_->GetLiveVersion(registration_data.version_id)) {
      if (registration_data.is_active)
        info.active_version = version->GetInfo();
      else
        info.waiting_version = version->GetInfo();
      infos.push_back(info);
      continue;
    }

    if (registration_data.is_active) {
      info.active_version.status = ServiceWorkerVersion::ACTIVATED;
      info.active_version.script_url = registration_data.script;
      info.active_version.version_id = registration_data.version_id;
      info.active_version.registration_id = registration_data.registration_id;
      info.active_version.script_response_time =
          registration_data.script_response_time;
      info.active_version.fetch_handler_existence =
          registration_data.has_fetch_handler
              ? ServiceWorkerVersion::FetchHandlerExistence::EXISTS
              : ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST;
    } else {
      info.waiting_version.status = ServiceWorkerVersion::INSTALLED;
      info.waiting_version.script_url = registration_data.script;
      info.waiting_version.version_id = registration_data.version_id;
      info.waiting_version.registration_id = registration_data.registration_id;
      info.waiting_version.script_response_time =
          registration_data.script_response_time;
      info.waiting_version.fetch_handler_existence =
          registration_data.has_fetch_handler
              ? ServiceWorkerVersion::FetchHandlerExistence::EXISTS
              : ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST;
    }
    infos.push_back(info);
  }

  // Add unstored registrations that are being installed.
  for (const auto& registration : installing_registrations_) {
    if (pushed_registrations.insert(registration.first).second)
      infos.push_back(registration.second->GetInfo());
  }

  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk, infos);
}

void ServiceWorkerStorage::DidStoreRegistration(
    StatusCallback callback,
    const ServiceWorkerDatabase::RegistrationData& new_version,
    const GURL& origin,
    const ServiceWorkerDatabase::RegistrationData& deleted_version,
    const std::vector<int64_t>& newly_purgeable_resources,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    ScheduleDeleteAndStartOver();
    std::move(callback).Run(DatabaseStatusToStatusCode(status));
    return;
  }
  registered_origins_.insert(origin);

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(new_version.registration_id);
  if (registration) {
    registration->set_resources_total_size_bytes(
        new_version.resources_total_size_bytes);
  }
  if (quota_manager_proxy_) {
    // Can be nullptr in tests.
    quota_manager_proxy_->NotifyStorageModified(
        storage::QuotaClient::kServiceWorker, url::Origin::Create(origin),
        blink::mojom::StorageType::kTemporary,
        new_version.resources_total_size_bytes -
            deleted_version.resources_total_size_bytes);
  }

  // Purge the deleted version's resources now if needed. This is subtle. The
  // version might still be used for a long time even after it's deleted. We can
  // only purge safely once the version is REDUNDANT, since it will never be
  // used again.
  //
  // If the deleted version's ServiceWorkerVersion doesn't exist, we can assume
  // it's effectively REDUNDANT so it's safe to purge now. This is because the
  // caller is assumed to promote the new version to active unless the deleted
  // version is doing work, and it can't be doing work if it's not live.
  //
  // If the ServiceWorkerVersion does exist, it triggers purging once it reaches
  // REDUNDANT. Otherwise, purging happens on the next browser session (via
  // DeleteStaleResources).
  if (!context_->GetLiveVersion(deleted_version.version_id))
    StartPurgingResources(newly_purgeable_resources);

  context_->NotifyRegistrationStored(new_version.registration_id,
                                     new_version.scope);
  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
}

void ServiceWorkerStorage::DidUpdateToActiveState(
    StatusCallback callback,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK &&
      status != ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    ScheduleDeleteAndStartOver();
  }
  std::move(callback).Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerStorage::DidDeleteRegistration(
    std::unique_ptr<DidDeleteRegistrationParams> params,
    OriginState origin_state,
    const ServiceWorkerDatabase::RegistrationData& deleted_version,
    const std::vector<int64_t>& newly_purgeable_resources,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    ScheduleDeleteAndStartOver();
    std::move(params->callback).Run(DatabaseStatusToStatusCode(status));
    return;
  }
  if (quota_manager_proxy_) {
    // Can be nullptr in tests.
    quota_manager_proxy_->NotifyStorageModified(
        storage::QuotaClient::kServiceWorker,
        url::Origin::Create(params->origin),
        blink::mojom::StorageType::kTemporary,
        -deleted_version.resources_total_size_bytes);
  }
  if (origin_state == OriginState::kDelete)
    registered_origins_.erase(params->origin);
  std::move(params->callback).Run(blink::ServiceWorkerStatusCode::kOk);

  if (!context_->GetLiveVersion(deleted_version.version_id))
    StartPurgingResources(newly_purgeable_resources);
}

void ServiceWorkerStorage::DidWriteUncommittedResourceIds(
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK)
    ScheduleDeleteAndStartOver();
}

void ServiceWorkerStorage::DidPurgeUncommittedResourceIds(
    const std::set<int64_t>& resource_ids,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    ScheduleDeleteAndStartOver();
    return;
  }
  StartPurgingResources(resource_ids);
}

void ServiceWorkerStorage::DidStoreUserData(
    StatusCallback callback,
    ServiceWorkerDatabase::Status status) {
  // |status| can be NOT_FOUND when the associated registration did not exist in
  // the database. In the case, we don't have to schedule the corruption
  // recovery.
  if (status != ServiceWorkerDatabase::STATUS_OK &&
      status != ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    ScheduleDeleteAndStartOver();
  }
  std::move(callback).Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerStorage::DidGetUserData(
    GetUserDataCallback callback,
    const std::vector<std::string>& data,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK &&
      status != ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    ScheduleDeleteAndStartOver();
  }
  std::move(callback).Run(data, DatabaseStatusToStatusCode(status));
}

void ServiceWorkerStorage::DidGetUserKeysAndData(
    GetUserKeysAndDataCallback callback,
    const base::flat_map<std::string, std::string>& data_map,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK &&
      status != ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    ScheduleDeleteAndStartOver();
  }
  std::move(callback).Run(data_map, DatabaseStatusToStatusCode(status));
}

void ServiceWorkerStorage::DidDeleteUserData(
    StatusCallback callback,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK)
    ScheduleDeleteAndStartOver();
  std::move(callback).Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerStorage::DidGetUserDataForAllRegistrations(
    GetUserDataForAllRegistrationsCallback callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK)
    ScheduleDeleteAndStartOver();
  std::move(callback).Run(user_data, DatabaseStatusToStatusCode(status));
}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerStorage::GetOrCreateRegistration(
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources) {
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(data.registration_id);
  if (registration)
    return registration;

  blink::mojom::ServiceWorkerRegistrationOptions options(
      data.scope, data.script_type, data.update_via_cache);
  registration = new ServiceWorkerRegistration(options, data.registration_id,
                                               context_->AsWeakPtr());
  registration->set_resources_total_size_bytes(data.resources_total_size_bytes);
  registration->set_last_update_check(data.last_update_check);
  DCHECK(uninstalling_registrations_.find(data.registration_id) ==
         uninstalling_registrations_.end());

  scoped_refptr<ServiceWorkerVersion> version =
      context_->GetLiveVersion(data.version_id);
  if (!version) {
    version = base::MakeRefCounted<ServiceWorkerVersion>(
        registration.get(), data.script, data.script_type, data.version_id,
        context_->AsWeakPtr());
    version->set_fetch_handler_existence(
        data.has_fetch_handler
            ? ServiceWorkerVersion::FetchHandlerExistence::EXISTS
            : ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST);
    version->SetStatus(data.is_active ?
        ServiceWorkerVersion::ACTIVATED : ServiceWorkerVersion::INSTALLED);
    version->script_cache_map()->SetResources(resources);
    if (data.origin_trial_tokens)
      version->SetValidOriginTrialTokens(*data.origin_trial_tokens);

    version->set_used_features(data.used_features);
  }
  version->set_script_response_time_for_devtools(data.script_response_time);

  if (version->status() == ServiceWorkerVersion::ACTIVATED)
    registration->SetActiveVersion(version);
  else if (version->status() == ServiceWorkerVersion::INSTALLED)
    registration->SetWaitingVersion(version);
  else
    NOTREACHED();

  registration->EnableNavigationPreload(data.navigation_preload_state.enabled);
  registration->SetNavigationPreloadHeader(
      data.navigation_preload_state.header);
  return registration;
}

ServiceWorkerRegistration*
ServiceWorkerStorage::FindInstallingRegistrationForClientUrl(
    const GURL& client_url) {
  DCHECK(!client_url.has_ref());

  LongestScopeMatcher matcher(client_url);
  ServiceWorkerRegistration* match = nullptr;

  // TODO(nhiroki): This searches over installing registrations linearly and it
  // couldn't be scalable. Maybe the regs should be partitioned by origin.
  for (const auto& registration : installing_registrations_)
    if (matcher.MatchLongest(registration.second->scope()))
      match = registration.second.get();
  return match;
}

ServiceWorkerRegistration*
ServiceWorkerStorage::FindInstallingRegistrationForScope(const GURL& scope) {
  for (const auto& registration : installing_registrations_)
    if (registration.second->scope() == scope)
      return registration.second.get();
  return nullptr;
}

ServiceWorkerRegistration*
ServiceWorkerStorage::FindInstallingRegistrationForId(int64_t registration_id) {
  RegistrationRefsById::const_iterator found =
      installing_registrations_.find(registration_id);
  if (found == installing_registrations_.end())
    return nullptr;
  return found->second.get();
}

ServiceWorkerDiskCache* ServiceWorkerStorage::disk_cache() {
  DCHECK(STORAGE_STATE_INITIALIZED == state_ ||
         STORAGE_STATE_DISABLED == state_)
      << state_;
  if (disk_cache_)
    return disk_cache_.get();
  disk_cache_.reset(new ServiceWorkerDiskCache);

  if (IsDisabled()) {
    disk_cache_->Disable();
    return disk_cache_.get();
  }

  base::FilePath path = GetDiskCachePath();
  if (path.empty()) {
    int rv = disk_cache_->InitWithMemBackend(0, net::CompletionOnceCallback());
    DCHECK_EQ(net::OK, rv);
    return disk_cache_.get();
  }

  InitializeDiskCache();
  return disk_cache_.get();
}

void ServiceWorkerStorage::InitializeDiskCache() {
  disk_cache_->set_is_waiting_to_initialize(false);
  expecting_done_with_disk_on_disable_ = true;
  int rv = disk_cache_->InitWithDiskBackend(
      GetDiskCachePath(), false,
      base::BindOnce(&ServiceWorkerStorage::DiskCacheImplDoneWithDisk,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&ServiceWorkerStorage::OnDiskCacheInitialized,
                     weak_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING)
    OnDiskCacheInitialized(rv);
}

void ServiceWorkerStorage::OnDiskCacheInitialized(int rv) {
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to open the serviceworker diskcache: "
               << net::ErrorToString(rv);
    ScheduleDeleteAndStartOver();
  }
  ServiceWorkerMetrics::CountInitDiskCacheResult(rv == net::OK);
}

void ServiceWorkerStorage::StartPurgingResources(
    const std::set<int64_t>& resource_ids) {
  DCHECK(has_checked_for_stale_resources_);
  for (int64_t resource_id : resource_ids)
    purgeable_resource_ids_.push_back(resource_id);
  ContinuePurgingResources();
}

void ServiceWorkerStorage::StartPurgingResources(
    const std::vector<int64_t>& resource_ids) {
  DCHECK(has_checked_for_stale_resources_);
  for (int64_t resource_id : resource_ids)
    purgeable_resource_ids_.push_back(resource_id);
  ContinuePurgingResources();
}

void ServiceWorkerStorage::StartPurgingResources(
    const ResourceList& resources) {
  DCHECK(has_checked_for_stale_resources_);
  for (const auto& resource : resources)
    purgeable_resource_ids_.push_back(resource.resource_id);
  ContinuePurgingResources();
}

void ServiceWorkerStorage::ContinuePurgingResources() {
  if (is_purge_pending_)
    return;
  if (purgeable_resource_ids_.empty()) {
    if (purging_complete_callback_for_test_)
      std::move(purging_complete_callback_for_test_).Run();
    return;
  }

  // Do one at a time until we're done, use RunSoon to avoid recursion when
  // DoomEntry returns immediately.
  is_purge_pending_ = true;
  int64_t id = purgeable_resource_ids_.front();
  purgeable_resource_ids_.pop_front();
  RunSoon(FROM_HERE, base::BindOnce(&ServiceWorkerStorage::PurgeResource,
                                    weak_factory_.GetWeakPtr(), id));
}

void ServiceWorkerStorage::PurgeResource(int64_t id) {
  DCHECK(is_purge_pending_);
  int rv = disk_cache()->DoomEntry(
      id, base::BindOnce(&ServiceWorkerStorage::OnResourcePurged,
                         weak_factory_.GetWeakPtr(), id));
  if (rv != net::ERR_IO_PENDING)
    OnResourcePurged(id, rv);
}

void ServiceWorkerStorage::OnResourcePurged(int64_t id, int rv) {
  DCHECK(is_purge_pending_);
  is_purge_pending_ = false;

  ServiceWorkerMetrics::RecordPurgeResourceResult(rv);

  // TODO(falken): Is it always OK to ClearPurgeableResourceIds if |rv| is
  // failure? The disk cache entry might still remain and once we remove its
  // purgeable id, we will never retry deleting it.
  std::set<int64_t> ids = {id};
  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&ServiceWorkerDatabase::ClearPurgeableResourceIds),
          base::Unretained(database_.get()), ids));

  // Continue purging resources regardless of the previous result.
  ContinuePurgingResources();
}

void ServiceWorkerStorage::DeleteStaleResources() {
  DCHECK(!has_checked_for_stale_resources_);
  has_checked_for_stale_resources_ = true;
  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerStorage::CollectStaleResourcesFromDB, database_.get(),
          base::ThreadTaskRunnerHandle::Get(),
          base::BindOnce(&ServiceWorkerStorage::DidCollectStaleResources,
                         weak_factory_.GetWeakPtr())));
}

void ServiceWorkerStorage::DidCollectStaleResources(
    const std::vector<int64_t>& stale_resource_ids,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    DCHECK_NE(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND, status);
    ScheduleDeleteAndStartOver();
    return;
  }
  StartPurgingResources(stale_resource_ids);
}

void ServiceWorkerStorage::ClearSessionOnlyOrigins() {
  // Can be null in tests.
  if (!special_storage_policy_)
    return;

  if (!special_storage_policy_->HasSessionOnlyOrigins())
    return;

  std::set<GURL> session_only_origins;
  for (const GURL& origin : registered_origins_) {
    if (!special_storage_policy_->IsStorageSessionOnly(origin))
      continue;
    if (special_storage_policy_->IsStorageProtected(origin))
      continue;
    session_only_origins.insert(origin);
  }

  database_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeleteAllDataForOriginsFromDB, database_.get(),
                                session_only_origins));
}

// static
void ServiceWorkerStorage::CollectStaleResourcesFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    GetResourcesCallback callback) {
  std::set<int64_t> ids;
  ServiceWorkerDatabase::Status status =
      database->GetUncommittedResourceIds(&ids);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<int64_t>(ids.begin(), ids.end()), status));
    return;
  }

  status = database->PurgeUncommittedResourceIds(ids);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<int64_t>(ids.begin(), ids.end()), status));
    return;
  }

  ids.clear();
  status = database->GetPurgeableResourceIds(&ids);
  original_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     std::vector<int64_t>(ids.begin(), ids.end()), status));
}

// static
void ServiceWorkerStorage::ReadInitialDataFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    InitializeCallback callback) {
  DCHECK(database);
  std::unique_ptr<ServiceWorkerStorage::InitialData> data(
      new ServiceWorkerStorage::InitialData());

  ServiceWorkerDatabase::Status status =
      database->GetNextAvailableIds(&data->next_registration_id,
                                    &data->next_version_id,
                                    &data->next_resource_id);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(data), status));
    return;
  }

  status = database->GetOriginsWithRegistrations(&data->origins);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(data), status));
    return;
  }

  original_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(data), status));
}

void ServiceWorkerStorage::DeleteRegistrationFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const GURL& origin,
    DeleteRegistrationCallback callback) {
  DCHECK(database);

  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ServiceWorkerDatabase::Status status = database->DeleteRegistration(
      registration_id, origin, &deleted_version, &newly_purgeable_resources);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), OriginState::kKeep, deleted_version,
                       std::vector<int64_t>(), status));
    return;
  }

  // TODO(nhiroki): Add convenient method to ServiceWorkerDatabase to check the
  // unique origin list.
  RegistrationList registrations;
  status = database->GetRegistrationsForOrigin(origin, &registrations, nullptr);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), OriginState::kKeep, deleted_version,
                       std::vector<int64_t>(), status));
    return;
  }

  OriginState origin_state =
      registrations.empty() ? OriginState::kDelete : OriginState::kKeep;
  original_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), origin_state, deleted_version,
                     newly_purgeable_resources, status));
}

void ServiceWorkerStorage::WriteRegistrationInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources,
    WriteRegistrationCallback callback) {
  DCHECK(database);
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ServiceWorkerDatabase::Status status = database->WriteRegistration(
      data, resources, &deleted_version, &newly_purgeable_resources);
  original_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), data.script.GetOrigin(),
                     deleted_version, newly_purgeable_resources, status));
}

// static
void ServiceWorkerStorage::FindForClientUrlInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const GURL& client_url,
    FindInDBCallback callback) {
  GURL origin = client_url.GetOrigin();
  RegistrationList registration_data_list;
  ServiceWorkerDatabase::Status status = database->GetRegistrationsForOrigin(
      origin, &registration_data_list, nullptr);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  ServiceWorkerDatabase::RegistrationData(),
                                  ResourceList(), status));
    return;
  }

  ServiceWorkerDatabase::RegistrationData data;
  ResourceList resources;
  status = ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND;

  // Find one with a scope match.
  LongestScopeMatcher matcher(client_url);
  int64_t match = blink::mojom::kInvalidServiceWorkerRegistrationId;
  for (const auto& registration_data : registration_data_list)
    if (matcher.MatchLongest(registration_data.scope))
      match = registration_data.registration_id;
  if (match != blink::mojom::kInvalidServiceWorkerRegistrationId)
    status = database->ReadRegistration(match, origin, &data, &resources);

  original_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), data, resources, status));
}

// static
void ServiceWorkerStorage::FindForScopeInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const GURL& scope,
    FindInDBCallback callback) {
  GURL origin = scope.GetOrigin();
  RegistrationList registration_data_list;
  ServiceWorkerDatabase::Status status = database->GetRegistrationsForOrigin(
      origin, &registration_data_list, nullptr);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  ServiceWorkerDatabase::RegistrationData(),
                                  ResourceList(), status));
    return;
  }

  // Find one with an exact matching scope.
  ServiceWorkerDatabase::RegistrationData data;
  ResourceList resources;
  status = ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND;
  for (const auto& registration_data : registration_data_list) {
    if (scope != registration_data.scope)
      continue;
    status = database->ReadRegistration(registration_data.registration_id,
                                        origin, &data, &resources);
    break;  // We're done looping.
  }

  original_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), data, resources, status));
}

// static
void ServiceWorkerStorage::FindForIdInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const GURL& origin,
    FindInDBCallback callback) {
  ServiceWorkerDatabase::RegistrationData data;
  ResourceList resources;
  ServiceWorkerDatabase::Status status =
      database->ReadRegistration(registration_id, origin, &data, &resources);
  original_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), data, resources, status));
}

// static
void ServiceWorkerStorage::FindForIdOnlyInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    FindInDBCallback callback) {
  GURL origin;
  ServiceWorkerDatabase::Status status =
      database->ReadRegistrationOrigin(registration_id, &origin);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  ServiceWorkerDatabase::RegistrationData(),
                                  ResourceList(), status));
    return;
  }
  FindForIdInDB(database, original_task_runner, registration_id, origin,
                std::move(callback));
}

void ServiceWorkerStorage::GetUserDataInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const std::vector<std::string>& keys,
    GetUserDataInDBCallback callback) {
  std::vector<std::string> values;
  ServiceWorkerDatabase::Status status =
      database->ReadUserData(registration_id, keys, &values);
  original_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), values, status));
}

void ServiceWorkerStorage::GetUserDataByKeyPrefixInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserDataInDBCallback callback) {
  std::vector<std::string> values;
  ServiceWorkerDatabase::Status status =
      database->ReadUserDataByKeyPrefix(registration_id, key_prefix, &values);
  original_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), values, status));
}

void ServiceWorkerStorage::GetUserKeysAndDataByKeyPrefixInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserKeysAndDataInDBCallback callback) {
  base::flat_map<std::string, std::string> data_map;
  ServiceWorkerDatabase::Status status =
      database->ReadUserKeysAndDataByKeyPrefix(registration_id, key_prefix,
                                               &data_map);
  original_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), data_map, status));
}

void ServiceWorkerStorage::GetUserDataForAllRegistrationsInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const std::string& key,
    GetUserDataForAllRegistrationsInDBCallback callback) {
  std::vector<std::pair<int64_t, std::string>> user_data;
  ServiceWorkerDatabase::Status status =
      database->ReadUserDataForAllRegistrations(key, &user_data);
  original_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), user_data, status));
}

void ServiceWorkerStorage::GetUserDataForAllRegistrationsByKeyPrefixInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const std::string& key_prefix,
    GetUserDataForAllRegistrationsInDBCallback callback) {
  std::vector<std::pair<int64_t, std::string>> user_data;
  ServiceWorkerDatabase::Status status =
      database->ReadUserDataForAllRegistrationsByKeyPrefix(key_prefix,
                                                           &user_data);
  original_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), user_data, status));
}

void ServiceWorkerStorage::DeleteAllDataForOriginsFromDB(
    ServiceWorkerDatabase* database,
    const std::set<GURL>& origins) {
  DCHECK(database);

  std::vector<int64_t> newly_purgeable_resources;
  database->DeleteAllDataForOrigins(origins, &newly_purgeable_resources);
}

void ServiceWorkerStorage::PerformStorageCleanupInDB(
    ServiceWorkerDatabase* database) {
  DCHECK(database);
  database->RewriteDB();
}

bool ServiceWorkerStorage::IsDisabled() const {
  return state_ == STORAGE_STATE_DISABLED;
}

// TODO(nhiroki): The corruption recovery should not be scheduled if the error
// is transient and it can get healed soon (e.g. IO error). To do that, the
// database should not disable itself when an error occurs and the storage
// controls it instead.
void ServiceWorkerStorage::ScheduleDeleteAndStartOver() {
  // TODO(dmurph): Notify the quota manager somehow that all of our data is now
  // removed.
  if (state_ == STORAGE_STATE_DISABLED) {
    // Recovery process has already been scheduled.
    return;
  }
  Disable();

  DVLOG(1) << "Schedule to delete the context and start over.";
  context_->ScheduleDeleteAndStartOver();
}

void ServiceWorkerStorage::DidDeleteDatabase(
    StatusCallback callback,
    ServiceWorkerDatabase::Status status) {
  DCHECK_EQ(STORAGE_STATE_DISABLED, state_);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    // Give up the corruption recovery until the browser restarts.
    LOG(ERROR) << "Failed to delete the database: "
               << ServiceWorkerDatabase::StatusToString(status);
    ServiceWorkerMetrics::RecordDeleteAndStartOverResult(
        ServiceWorkerMetrics::DELETE_DATABASE_ERROR);
    std::move(callback).Run(DatabaseStatusToStatusCode(status));
    return;
  }
  DVLOG(1) << "Deleted ServiceWorkerDatabase successfully.";

  // Delete the disk cache. Use BLOCK_SHUTDOWN to try to avoid things being
  // half-deleted.
  // TODO(falken): Investigate if BLOCK_SHUTDOWN is needed, as the next startup
  // is expected to cleanup the disk cache anyway. Also investigate whether
  // ClearSessionOnlyOrigins() should try to delete relevant entries from the
  // disk cache before shutdown.

  // TODO(nhiroki): What if there is a bunch of files in the cache directory?
  // Deleting the directory could take a long time and restart could be delayed.
  // We should probably rename the directory and delete it later.
  PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&base::DeleteFile, GetDiskCachePath(), true),
      base::BindOnce(&ServiceWorkerStorage::DidDeleteDiskCache,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorage::DidDeleteDiskCache(StatusCallback callback,
                                              bool result) {
  DCHECK_EQ(STORAGE_STATE_DISABLED, state_);
  if (!result) {
    // Give up the corruption recovery until the browser restarts.
    LOG(ERROR) << "Failed to delete the diskcache.";
    ServiceWorkerMetrics::RecordDeleteAndStartOverResult(
        ServiceWorkerMetrics::DELETE_DISK_CACHE_ERROR);
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorFailed);
    return;
  }
  DVLOG(1) << "Deleted ServiceWorkerDiskCache successfully.";
  ServiceWorkerMetrics::RecordDeleteAndStartOverResult(
      ServiceWorkerMetrics::DELETE_OK);
  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
}

}  // namespace content
