// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "content/browser/service_worker/service_worker_registry.h"

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage_control_impl.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {

namespace {

blink::ServiceWorkerStatusCode DatabaseStatusToStatusCode(
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  switch (status) {
    case storage::mojom::ServiceWorkerDatabaseStatus::kOk:
      return blink::ServiceWorkerStatusCode::kOk;
    case storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound:
      return blink::ServiceWorkerStatusCode::kErrorNotFound;
    case storage::mojom::ServiceWorkerDatabaseStatus::kErrorDisabled:
      return blink::ServiceWorkerStatusCode::kErrorAbort;
      NOTREACHED();
    default:
      return blink::ServiceWorkerStatusCode::kErrorFailed;
  }
}

ServiceWorkerStorage::DatabaseStatusCallback CreateDatabaseStatusCallback(
    ServiceWorkerRegistry::StatusCallback callback) {
  return base::BindOnce(
      [](ServiceWorkerRegistry::StatusCallback callback,
         storage::mojom::ServiceWorkerDatabaseStatus database_status) {
        blink::ServiceWorkerStatusCode status =
            DatabaseStatusToStatusCode(database_status);
        std::move(callback).Run(status);
      },
      std::move(callback));
}

void RunSoon(const base::Location& from_here, base::OnceClosure closure) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(from_here, std::move(closure));
}

void CompleteFindNow(scoped_refptr<ServiceWorkerRegistration> registration,
                     blink::ServiceWorkerStatusCode status,
                     ServiceWorkerRegistry::FindRegistrationCallback callback) {
  if (registration && registration->is_deleted()) {
    // It's past the point of no return and no longer findable.
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorNotFound,
                            nullptr);
    return;
  }
  std::move(callback).Run(status, std::move(registration));
}

void CompleteFindSoon(
    const base::Location& from_here,
    scoped_refptr<ServiceWorkerRegistration> registration,
    blink::ServiceWorkerStatusCode status,
    ServiceWorkerRegistry::FindRegistrationCallback callback) {
  RunSoon(from_here, base::BindOnce(&CompleteFindNow, std::move(registration),
                                    status, std::move(callback)));
}

}  // namespace

// A helper class that runs on the IO thread to observe storage policy updates.
class ServiceWorkerRegistry::StoragePolicyObserver
    : public storage::SpecialStoragePolicy::Observer {
 public:
  StoragePolicyObserver(
      base::WeakPtr<ServiceWorkerRegistry> owner,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy)
      : owner_(owner), special_storage_policy_(special_storage_policy) {
    DCHECK(special_storage_policy_);
    special_storage_policy_->AddObserver(this);
  }

  StoragePolicyObserver(const StoragePolicyObserver&) = delete;
  StoragePolicyObserver& operator=(const StoragePolicyObserver&) = delete;

  ~StoragePolicyObserver() override {
    special_storage_policy_->RemoveObserver(this);
  }

 private:
  // storage::SpecialStoragePolicy::Observer:
  void OnPolicyChanged() override {
    ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerRegistry::OnStoragePolicyChanged, owner_));
  }

  // |owner_| is dereferenced on the core thread. This shouldn't be dereferenced
  // on the IO thread.
  base::WeakPtr<ServiceWorkerRegistry> owner_;
  const scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
};

ServiceWorkerRegistry::ServiceWorkerRegistry(
    const base::FilePath& user_data_directory,
    ServiceWorkerContextCore* context,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy)
    : context_(context),
      storage_control_(std::make_unique<ServiceWorkerStorageControlImpl>(
          ServiceWorkerStorage::Create(user_data_directory,
                                       std::move(database_task_runner),
                                       quota_manager_proxy))),
      special_storage_policy_(special_storage_policy) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(context_);
  Start();
}

ServiceWorkerRegistry::ServiceWorkerRegistry(
    ServiceWorkerContextCore* context,
    ServiceWorkerRegistry* old_registry)
    : context_(context),
      storage_control_(std::make_unique<ServiceWorkerStorageControlImpl>(
          ServiceWorkerStorage::Create(old_registry->storage()))),
      special_storage_policy_(old_registry->special_storage_policy_) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(context_);
  Start();
}

ServiceWorkerRegistry::~ServiceWorkerRegistry() = default;

ServiceWorkerStorage* ServiceWorkerRegistry::storage() const {
  return storage_control_->storage();
}

void ServiceWorkerRegistry::CreateNewRegistration(
    blink::mojom::ServiceWorkerRegistrationOptions options,
    NewRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  GetRemoteStorageControl()->GetNewRegistrationId(base::BindOnce(
      &ServiceWorkerRegistry::DidGetNewRegistrationId,
      weak_factory_.GetWeakPtr(), std::move(options), std::move(callback)));
}

void ServiceWorkerRegistry::CreateNewVersion(
    scoped_refptr<ServiceWorkerRegistration> registration,
    const GURL& script_url,
    blink::mojom::ScriptType script_type,
    NewVersionCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(registration);
  GetRemoteStorageControl()->GetNewVersionId(base::BindOnce(
      &ServiceWorkerRegistry::DidGetNewVersionId, weak_factory_.GetWeakPtr(),
      std::move(registration), script_url, script_type, std::move(callback)));
}

void ServiceWorkerRegistry::FindRegistrationForClientUrl(
    const GURL& client_url,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  // To connect this TRACE_EVENT with the callback, Time::Now() is used as a
  // trace event id.
  int64_t trace_event_id =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  TRACE_EVENT_ASYNC_BEGIN1(
      "ServiceWorker", "ServiceWorkerRegistry::FindRegistrationForClientUrl",
      trace_event_id, "URL", client_url.spec());
  GetRemoteStorageControl()->FindRegistrationForClientUrl(
      client_url,
      base::BindOnce(&ServiceWorkerRegistry::DidFindRegistrationForClientUrl,
                     weak_factory_.GetWeakPtr(), client_url, trace_event_id,
                     std::move(callback)));
}

void ServiceWorkerRegistry::FindRegistrationForScope(
    const GURL& scope,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (is_storage_disabled_) {
    RunSoon(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       blink::ServiceWorkerStatusCode::kErrorAbort, nullptr));
    return;
  }

  // Look up installing registration before checking storage.
  scoped_refptr<ServiceWorkerRegistration> installing_registration =
      FindInstallingRegistrationForScope(scope);
  if (installing_registration && !installing_registration->is_deleted()) {
    CompleteFindSoon(FROM_HERE, std::move(installing_registration),
                     blink::ServiceWorkerStatusCode::kOk, std::move(callback));
    return;
  }

  GetRemoteStorageControl()->FindRegistrationForScope(
      scope, base::BindOnce(&ServiceWorkerRegistry::DidFindRegistrationForScope,
                            weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerRegistry::FindRegistrationForId(
    int64_t registration_id,
    const url::Origin& origin,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  // Registration lookup is expected to abort when storage is disabled.
  if (is_storage_disabled_) {
    CompleteFindNow(nullptr, blink::ServiceWorkerStatusCode::kErrorAbort,
                    std::move(callback));
    return;
  }

  // Lookup live registration first.
  base::Optional<scoped_refptr<ServiceWorkerRegistration>> registration =
      FindFromLiveRegistrationsForId(registration_id);
  if (registration) {
    blink::ServiceWorkerStatusCode status =
        registration.value() ? blink::ServiceWorkerStatusCode::kOk
                             : blink::ServiceWorkerStatusCode::kErrorNotFound;
    CompleteFindNow(std::move(registration.value()), status,
                    std::move(callback));
    return;
  }

  GetRemoteStorageControl()->FindRegistrationForId(
      registration_id, origin,
      base::BindOnce(&ServiceWorkerRegistry::DidFindRegistrationForId,
                     weak_factory_.GetWeakPtr(), registration_id,
                     std::move(callback)));
}

void ServiceWorkerRegistry::FindRegistrationForIdOnly(
    int64_t registration_id,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  // Registration lookup is expected to abort when storage is disabled.
  if (is_storage_disabled_) {
    CompleteFindNow(nullptr, blink::ServiceWorkerStatusCode::kErrorAbort,
                    std::move(callback));
    return;
  }

  // Lookup live registration first.
  base::Optional<scoped_refptr<ServiceWorkerRegistration>> registration =
      FindFromLiveRegistrationsForId(registration_id);
  if (registration) {
    blink::ServiceWorkerStatusCode status =
        registration.value() ? blink::ServiceWorkerStatusCode::kOk
                             : blink::ServiceWorkerStatusCode::kErrorNotFound;
    CompleteFindNow(std::move(registration.value()), status,
                    std::move(callback));
    return;
  }

  GetRemoteStorageControl()->FindRegistrationForId(
      registration_id, /*origin=*/base::nullopt,
      base::BindOnce(&ServiceWorkerRegistry::DidFindRegistrationForId,
                     weak_factory_.GetWeakPtr(), registration_id,
                     std::move(callback)));
}

void ServiceWorkerRegistry::GetRegistrationsForOrigin(
    const url::Origin& origin,
    GetRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  GetRemoteStorageControl()->GetRegistrationsForOrigin(
      origin,
      base::BindOnce(&ServiceWorkerRegistry::DidGetRegistrationsForOrigin,
                     weak_factory_.GetWeakPtr(), std::move(callback), origin));
}

void ServiceWorkerRegistry::GetStorageUsageForOrigin(
    const url::Origin& origin,
    GetStorageUsageForOriginCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  GetRemoteStorageControl()->GetUsageForOrigin(
      origin,
      base::BindOnce(
          [](GetStorageUsageForOriginCallback callback,
             storage::mojom::ServiceWorkerDatabaseStatus database_status,
             int64_t usage) {
            blink::ServiceWorkerStatusCode status =
                DatabaseStatusToStatusCode(database_status);
            std::move(callback).Run(status, usage);
          },
          std::move(callback)));
}

void ServiceWorkerRegistry::GetAllRegistrationsInfos(
    GetRegistrationsInfosCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  GetRemoteStorageControl()->GetAllRegistrationsDeprecated(
      base::BindOnce(&ServiceWorkerRegistry::DidGetAllRegistrations,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

ServiceWorkerRegistration* ServiceWorkerRegistry::GetUninstallingRegistration(
    const GURL& scope) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  // TODO(bashi): Should we check state of ServiceWorkerStorage?
  for (const auto& registration : uninstalling_registrations_) {
    if (registration.second->scope() == scope) {
      DCHECK(registration.second->is_uninstalling());
      return registration.second.get();
    }
  }
  return nullptr;
}

std::vector<scoped_refptr<ServiceWorkerRegistration>>
ServiceWorkerRegistry::GetUninstallingRegistrationsForOrigin(
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  std::vector<scoped_refptr<ServiceWorkerRegistration>> results;
  for (const auto& registration : uninstalling_registrations_) {
    if (url::Origin::Create(registration.second->scope()) == origin) {
      results.push_back(registration.second);
    }
  }
  return results;
}

void ServiceWorkerRegistry::StoreRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(registration);
  DCHECK(version);

  if (is_storage_disabled_) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }

  DCHECK_NE(version->fetch_handler_existence(),
            ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN);
  DCHECK_EQ(registration->status(), ServiceWorkerRegistration::Status::kIntact);

  auto data = storage::mojom::ServiceWorkerRegistrationData::New();
  data->registration_id = registration->id();
  data->scope = registration->scope();
  data->script = version->script_url();
  data->script_type = version->script_type();
  data->update_via_cache = registration->update_via_cache();
  data->has_fetch_handler = version->fetch_handler_existence() ==
                            ServiceWorkerVersion::FetchHandlerExistence::EXISTS;
  data->version_id = version->version_id();
  data->last_update_check = registration->last_update_check();
  data->is_active = (version == registration->active_version());
  if (version->origin_trial_tokens())
    data->origin_trial_tokens = *version->origin_trial_tokens();
  data->navigation_preload_state = blink::mojom::NavigationPreloadState::New();
  data->navigation_preload_state->enabled =
      registration->navigation_preload_state().enabled;
  data->navigation_preload_state->header =
      registration->navigation_preload_state().header;
  data->script_response_time = version->GetInfo().script_response_time;
  for (const blink::mojom::WebFeature feature : version->used_features())
    data->used_features.push_back(feature);

  // The ServiceWorkerVersion's COEP might be null if it is stored before
  // loading the main script. This happens in many unittests.
  if (version->cross_origin_embedder_policy()) {
    data->cross_origin_embedder_policy =
        version->cross_origin_embedder_policy().value();
  }

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
    DCHECK_GE(resource->size_bytes, 0);
    resources_total_size_bytes += resource->size_bytes;
  }
  data->resources_total_size_bytes = resources_total_size_bytes;

  GetRemoteStorageControl()->StoreRegistration(
      std::move(data), std::move(resources),
      base::BindOnce(&ServiceWorkerRegistry::DidStoreRegistration,
                     weak_factory_.GetWeakPtr(), registration->id(),
                     resources_total_size_bytes, registration->scope(),
                     std::move(callback)));
}

void ServiceWorkerRegistry::DeleteRegistration(
    scoped_refptr<ServiceWorkerRegistration> registration,
    const GURL& origin,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (is_storage_disabled_) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }

  DCHECK(!registration->is_deleted())
      << "attempt to delete a registration twice";

  GetRemoteStorageControl()->DeleteRegistration(
      registration->id(), origin,
      base::BindOnce(&ServiceWorkerRegistry::DidDeleteRegistration,
                     weak_factory_.GetWeakPtr(), registration->id(), origin,
                     std::move(callback)));

  DCHECK(!base::Contains(uninstalling_registrations_, registration->id()));
  uninstalling_registrations_[registration->id()] = registration;
  registration->SetStatus(ServiceWorkerRegistration::Status::kUninstalling);
}

void ServiceWorkerRegistry::NotifyInstallingRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(installing_registrations_.find(registration->id()) ==
         installing_registrations_.end());
  installing_registrations_[registration->id()] = registration;
}

void ServiceWorkerRegistry::NotifyDoneInstallingRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  installing_registrations_.erase(registration->id());
  if (status != blink::ServiceWorkerStatusCode::kOk && version) {
    ResourceList resources;
    version->script_cache_map()->GetResources(&resources);

    std::vector<int64_t> resource_ids;
    for (const auto& resource : resources)
      resource_ids.push_back(resource->resource_id);
    DoomUncommittedResources(resource_ids);
  }
}

void ServiceWorkerRegistry::NotifyDoneUninstallingRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerRegistration::Status new_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  registration->SetStatus(new_status);
  uninstalling_registrations_.erase(registration->id());
}

void ServiceWorkerRegistry::UpdateToActiveState(int64_t registration_id,
                                                const GURL& origin,
                                                StatusCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  GetRemoteStorageControl()->UpdateToActiveState(
      registration_id, origin,
      base::BindOnce(&ServiceWorkerRegistry::DidUpdateToActiveState,
                     weak_factory_.GetWeakPtr(), origin, std::move(callback)));
}

void ServiceWorkerRegistry::UpdateLastUpdateCheckTime(
    int64_t registration_id,
    const GURL& origin,
    base::Time last_update_check_time,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  GetRemoteStorageControl()->UpdateLastUpdateCheckTime(
      registration_id, origin, last_update_check_time,
      CreateDatabaseStatusCallback(std::move(callback)));
}

void ServiceWorkerRegistry::UpdateNavigationPreloadEnabled(
    int64_t registration_id,
    const GURL& origin,
    bool enable,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  GetRemoteStorageControl()->UpdateNavigationPreloadEnabled(
      registration_id, origin, enable,
      CreateDatabaseStatusCallback(std::move(callback)));
}

void ServiceWorkerRegistry::UpdateNavigationPreloadHeader(
    int64_t registration_id,
    const GURL& origin,
    const std::string& value,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  GetRemoteStorageControl()->UpdateNavigationPreloadHeader(
      registration_id, origin, value,
      CreateDatabaseStatusCallback(std::move(callback)));
}

void ServiceWorkerRegistry::StoreUncommittedResourceId(int64_t resource_id,
                                                       const GURL& origin) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  GetRemoteStorageControl()->StoreUncommittedResourceId(
      resource_id, origin,
      base::BindOnce(&ServiceWorkerRegistry::DidWriteUncommittedResourceIds,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegistry::DoomUncommittedResource(int64_t resource_id) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  std::vector<int64_t> resource_ids = {resource_id};
  DoomUncommittedResources(resource_ids);
}

void ServiceWorkerRegistry::GetUserData(int64_t registration_id,
                                        const std::vector<std::string>& keys,
                                        GetUserDataCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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

  GetRemoteStorageControl()->GetUserData(
      registration_id, keys,
      base::BindOnce(&ServiceWorkerRegistry::DidGetUserData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerRegistry::GetUserDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserDataCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      key_prefix.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback), std::vector<std::string>(),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }

  GetRemoteStorageControl()->GetUserDataByKeyPrefix(
      registration_id, key_prefix,
      base::BindOnce(&ServiceWorkerRegistry::DidGetUserData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerRegistry::GetUserKeysAndDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserKeysAndDataCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      key_prefix.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorFailed,
                           base::flat_map<std::string, std::string>()));
    return;
  }

  GetRemoteStorageControl()->GetUserKeysAndDataByKeyPrefix(
      registration_id, key_prefix,
      base::BindOnce(&ServiceWorkerRegistry::DidGetUserKeysAndData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerRegistry::StoreUserData(
    int64_t registration_id,
    const url::Origin& origin,
    const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      key_value_pairs.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }
  std::vector<storage::mojom::ServiceWorkerUserDataPtr> user_data;
  // TODO(crbug.com/1055677): Change this method to take a vector of
  // storage::mojom::ServiceWorkerUserDataPtr instead of converting
  //|key_value_pairs|.
  for (const auto& kv : key_value_pairs) {
    if (kv.first.empty()) {
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             blink::ServiceWorkerStatusCode::kErrorFailed));
      return;
    }
    user_data.push_back(storage::mojom::ServiceWorkerUserData::New(
        registration_id, kv.first, kv.second));
  }

  GetRemoteStorageControl()->StoreUserData(
      registration_id, origin, std::move(user_data),
      base::BindOnce(&ServiceWorkerRegistry::DidStoreUserData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerRegistry::ClearUserData(int64_t registration_id,
                                          const std::vector<std::string>& keys,
                                          StatusCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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

  GetRemoteStorageControl()->ClearUserData(
      registration_id, keys,
      base::BindOnce(&ServiceWorkerRegistry::DidClearUserData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerRegistry::ClearUserDataByKeyPrefixes(
    int64_t registration_id,
    const std::vector<std::string>& key_prefixes,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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

  GetRemoteStorageControl()->ClearUserDataByKeyPrefixes(
      registration_id, key_prefixes,
      base::BindOnce(&ServiceWorkerRegistry::DidClearUserData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerRegistry::ClearUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (key_prefix.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }

  GetRemoteStorageControl()->ClearUserDataForAllRegistrationsByKeyPrefix(
      key_prefix,
      base::BindOnce(&ServiceWorkerRegistry::DidClearUserData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerRegistry::GetUserDataForAllRegistrations(
    const std::string& key,
    GetUserDataForAllRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (key.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           std::vector<std::pair<int64_t, std::string>>(),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }

  GetRemoteStorageControl()->GetUserDataForAllRegistrations(
      key,
      base::BindOnce(&ServiceWorkerRegistry::DidGetUserDataForAllRegistrations,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerRegistry::GetUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    GetUserDataForAllRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (key_prefix.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           std::vector<std::pair<int64_t, std::string>>(),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }

  GetRemoteStorageControl()->GetUserDataForAllRegistrationsByKeyPrefix(
      key_prefix,
      base::BindOnce(&ServiceWorkerRegistry::DidGetUserDataForAllRegistrations,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerRegistry::PrepareForDeleteAndStarOver() {
  should_schedule_delete_and_start_over_ = false;
  GetRemoteStorageControl()->Disable();
  is_storage_disabled_ = true;
}

void ServiceWorkerRegistry::DeleteAndStartOver(StatusCallback callback) {
  GetRemoteStorageControl()->Delete(
      CreateDatabaseStatusCallback(std::move(callback)));
}

void ServiceWorkerRegistry::DisableDeleteAndStartOverForTesting() {
  DCHECK(should_schedule_delete_and_start_over_);
  should_schedule_delete_and_start_over_ = false;
  is_storage_disabled_ = true;
}

void ServiceWorkerRegistry::Start() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (special_storage_policy_) {
    storage_policy_observer_ = base::SequenceBound<StoragePolicyObserver>(
        base::CreateSequencedTaskRunner(BrowserThread::IO),
        weak_factory_.GetWeakPtr(),
        base::WrapRefCounted(special_storage_policy_.get()));

    GetRemoteStorageControl()->GetRegisteredOrigins(
        base::BindOnce(&ServiceWorkerRegistry::DidGetRegisteredOriginsOnStartup,
                       weak_factory_.GetWeakPtr()));
  }
}

ServiceWorkerRegistration*
ServiceWorkerRegistry::FindInstallingRegistrationForClientUrl(
    const GURL& client_url) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(!client_url.has_ref());

  blink::ServiceWorkerLongestScopeMatcher matcher(client_url);
  ServiceWorkerRegistration* match = nullptr;

  // TODO(nhiroki): This searches over installing registrations linearly and it
  // couldn't be scalable. Maybe the regs should be partitioned by origin.
  for (const auto& registration : installing_registrations_)
    if (matcher.MatchLongest(registration.second->scope()))
      match = registration.second.get();
  return match;
}

ServiceWorkerRegistration*
ServiceWorkerRegistry::FindInstallingRegistrationForScope(const GURL& scope) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  for (const auto& registration : installing_registrations_)
    if (registration.second->scope() == scope)
      return registration.second.get();
  return nullptr;
}

ServiceWorkerRegistration*
ServiceWorkerRegistry::FindInstallingRegistrationForId(
    int64_t registration_id) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  RegistrationRefsById::const_iterator found =
      installing_registrations_.find(registration_id);
  if (found == installing_registrations_.end())
    return nullptr;
  return found->second.get();
}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerRegistry::GetOrCreateRegistration(
    const storage::mojom::ServiceWorkerRegistrationData& data,
    const ResourceList& resources,
    mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
        version_reference) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(data.registration_id);
  if (registration)
    return registration;

  blink::mojom::ServiceWorkerRegistrationOptions options(
      data.scope, data.script_type, data.update_via_cache);
  registration = base::MakeRefCounted<ServiceWorkerRegistration>(
      options, data.registration_id, context_->AsWeakPtr());
  registration->SetStored();
  registration->set_resources_total_size_bytes(data.resources_total_size_bytes);
  registration->set_last_update_check(data.last_update_check);
  DCHECK(!base::Contains(uninstalling_registrations_, data.registration_id));

  scoped_refptr<ServiceWorkerVersion> version =
      context_->GetLiveVersion(data.version_id);
  if (!version) {
    version = base::MakeRefCounted<ServiceWorkerVersion>(
        registration.get(), data.script, data.script_type, data.version_id,
        std::move(version_reference), context_->AsWeakPtr());
    version->set_fetch_handler_existence(
        data.has_fetch_handler
            ? ServiceWorkerVersion::FetchHandlerExistence::EXISTS
            : ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST);
    version->SetStatus(data.is_active ? ServiceWorkerVersion::ACTIVATED
                                      : ServiceWorkerVersion::INSTALLED);
    version->script_cache_map()->SetResources(resources);
    if (data.origin_trial_tokens)
      version->SetValidOriginTrialTokens(*data.origin_trial_tokens);

    std::set<blink::mojom::WebFeature> used_features(data.used_features.begin(),
                                                     data.used_features.end());
    version->set_used_features(std::move(used_features));
    version->set_cross_origin_embedder_policy(
        data.cross_origin_embedder_policy);
  }
  version->set_script_response_time_for_devtools(data.script_response_time);

  if (version->status() == ServiceWorkerVersion::ACTIVATED)
    registration->SetActiveVersion(version);
  else if (version->status() == ServiceWorkerVersion::INSTALLED)
    registration->SetWaitingVersion(version);
  else
    NOTREACHED();

  registration->EnableNavigationPreload(data.navigation_preload_state->enabled);
  registration->SetNavigationPreloadHeader(
      data.navigation_preload_state->header);
  return registration;
}

base::Optional<scoped_refptr<ServiceWorkerRegistration>>
ServiceWorkerRegistry::FindFromLiveRegistrationsForId(int64_t registration_id) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration) {
    // The registration is considered as findable when it's stored or in
    // installing state.
    if (registration->IsStored() ||
        base::Contains(installing_registrations_, registration_id)) {
      return registration;
    }
    // Otherwise, the registration should not be findable even if it's still
    // alive.
    return nullptr;
  }
  // There is no live registration. Storage lookup is required. Returning
  // nullopt results in storage lookup.
  return base::nullopt;
}

void ServiceWorkerRegistry::DoomUncommittedResources(
    const std::vector<int64_t>& resource_ids) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  GetRemoteStorageControl()->DoomUncommittedResources(
      resource_ids,
      base::BindOnce(&ServiceWorkerRegistry::DidDoomUncommittedResourceIds,
                     weak_factory_.GetWeakPtr(), resource_ids));
}

void ServiceWorkerRegistry::DidFindRegistrationForClientUrl(
    const GURL& client_url,
    int64_t trace_event_id,
    FindRegistrationCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    storage::mojom::ServiceWorkerFindRegistrationResultPtr result) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (database_status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      database_status !=
          storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }

  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForClientUrl(client_url);
    if (installing_registration) {
      blink::ServiceWorkerStatusCode installing_status =
          installing_registration->is_deleted()
              ? blink::ServiceWorkerStatusCode::kErrorNotFound
              : blink::ServiceWorkerStatusCode::kOk;
      TRACE_EVENT_ASYNC_END2(
          "ServiceWorker",
          "ServiceWorkerRegistry::FindRegistrationForClientUrl", trace_event_id,
          "Status", blink::ServiceWorkerStatusToString(status), "Info",
          (installing_status == blink::ServiceWorkerStatusCode::kOk)
              ? "Installing registration is found"
              : "Any registrations are not found");
      CompleteFindNow(std::move(installing_registration), installing_status,
                      std::move(callback));
      return;
    }
  }

  scoped_refptr<ServiceWorkerRegistration> registration;
  if (status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(result);
    DCHECK(result->registration);
    DCHECK(result->version_reference);
    registration =
        GetOrCreateRegistration(*(result->registration), result->resources,
                                std::move(result->version_reference));
  }

  TRACE_EVENT_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerRegistry::FindRegistrationForClientUrl",
      trace_event_id, "Status", blink::ServiceWorkerStatusToString(status));
  CompleteFindNow(std::move(registration), status, std::move(callback));
}

void ServiceWorkerRegistry::DidFindRegistrationForScope(
    FindRegistrationCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    storage::mojom::ServiceWorkerFindRegistrationResultPtr result) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (database_status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      database_status !=
          storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }

  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  scoped_refptr<ServiceWorkerRegistration> registration;
  if (status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(result);
    DCHECK(result->registration);
    DCHECK(result->version_reference);
    registration =
        GetOrCreateRegistration(*(result->registration), result->resources,
                                std::move(result->version_reference));
  }

  CompleteFindNow(std::move(registration), status, std::move(callback));
}

void ServiceWorkerRegistry::DidFindRegistrationForId(
    int64_t registration_id,
    FindRegistrationCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    storage::mojom::ServiceWorkerFindRegistrationResultPtr result) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (database_status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      database_status !=
          storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }

  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForId(registration_id);
    if (installing_registration) {
      CompleteFindNow(std::move(installing_registration),
                      blink::ServiceWorkerStatusCode::kOk, std::move(callback));
      return;
    }
  }

  scoped_refptr<ServiceWorkerRegistration> registration;
  if (status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(result);
    DCHECK(result->registration);
    DCHECK(result->version_reference);
    registration =
        GetOrCreateRegistration(*(result->registration), result->resources,
                                std::move(result->version_reference));
  }

  CompleteFindNow(std::move(registration), status, std::move(callback));
}

void ServiceWorkerRegistry::DidGetRegistrationsForOrigin(
    GetRegistrationsCallback callback,
    const url::Origin& origin_filter,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    std::vector<storage::mojom::ServiceWorkerFindRegistrationResultPtr>
        entries) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  if (status != blink::ServiceWorkerStatusCode::kOk &&
      status != blink::ServiceWorkerStatusCode::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
    std::move(callback).Run(
        status, std::vector<scoped_refptr<ServiceWorkerRegistration>>());
    return;
  }

  // Add all stored registrations.
  std::set<int64_t> registration_ids;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  for (const auto& entry : entries) {
    DCHECK(entry->registration);
    DCHECK(entry->version_reference);
    registration_ids.insert(entry->registration->registration_id);
    // TODO(crbug.com/1055677): Pass ServiceWorkerLiveVersionRef.
    registrations.push_back(
        GetOrCreateRegistration(*entry->registration, entry->resources,
                                std::move(entry->version_reference)));
  }

  // Add unstored registrations that are being installed.
  for (const auto& registration : installing_registrations_) {
    if (url::Origin::Create(registration.second->scope()) != origin_filter)
      continue;
    if (registration_ids.insert(registration.first).second)
      registrations.push_back(registration.second);
  }

  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk,
                          std::move(registrations));
}

void ServiceWorkerRegistry::DidGetAllRegistrations(
    GetRegistrationsInfosCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    RegistrationList registration_data_list) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  if (status != blink::ServiceWorkerStatusCode::kOk &&
      status != blink::ServiceWorkerStatusCode::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
    std::move(callback).Run(status,
                            std::vector<ServiceWorkerRegistrationInfo>());
    return;
  }

  // Add all stored registrations.
  std::set<int64_t> pushed_registrations;
  std::vector<ServiceWorkerRegistrationInfo> infos;
  for (const auto& registration_data : registration_data_list) {
    const bool inserted =
        pushed_registrations.insert(registration_data->registration_id).second;
    DCHECK(inserted);

    ServiceWorkerRegistration* registration =
        context_->GetLiveRegistration(registration_data->registration_id);
    if (registration) {
      infos.push_back(registration->GetInfo());
      continue;
    }

    ServiceWorkerRegistrationInfo info;
    info.scope = registration_data->scope;
    info.update_via_cache = registration_data->update_via_cache;
    info.registration_id = registration_data->registration_id;
    info.stored_version_size_bytes =
        registration_data->resources_total_size_bytes;
    info.navigation_preload_enabled =
        registration_data->navigation_preload_state->enabled;
    info.navigation_preload_header_length =
        registration_data->navigation_preload_state->header.size();
    if (ServiceWorkerVersion* version =
            context_->GetLiveVersion(registration_data->version_id)) {
      if (registration_data->is_active)
        info.active_version = version->GetInfo();
      else
        info.waiting_version = version->GetInfo();
      infos.push_back(info);
      continue;
    }

    if (registration_data->is_active) {
      info.active_version.status = ServiceWorkerVersion::ACTIVATED;
      info.active_version.script_url = registration_data->script;
      info.active_version.version_id = registration_data->version_id;
      info.active_version.registration_id = registration_data->registration_id;
      info.active_version.script_response_time =
          registration_data->script_response_time;
      info.active_version.fetch_handler_existence =
          registration_data->has_fetch_handler
              ? ServiceWorkerVersion::FetchHandlerExistence::EXISTS
              : ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST;
      info.active_version.navigation_preload_state.enabled =
          registration_data->navigation_preload_state->enabled;
      info.active_version.navigation_preload_state.header =
          registration_data->navigation_preload_state->header;
    } else {
      info.waiting_version.status = ServiceWorkerVersion::INSTALLED;
      info.waiting_version.script_url = registration_data->script;
      info.waiting_version.version_id = registration_data->version_id;
      info.waiting_version.registration_id = registration_data->registration_id;
      info.waiting_version.script_response_time =
          registration_data->script_response_time;
      info.waiting_version.fetch_handler_existence =
          registration_data->has_fetch_handler
              ? ServiceWorkerVersion::FetchHandlerExistence::EXISTS
              : ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST;
      info.waiting_version.navigation_preload_state.enabled =
          registration_data->navigation_preload_state->enabled;
      info.waiting_version.navigation_preload_state.header =
          registration_data->navigation_preload_state->header;
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

void ServiceWorkerRegistry::DidStoreRegistration(
    int64_t stored_registration_id,
    uint64_t stored_resources_total_size_bytes,
    const GURL& stored_scope,
    StatusCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    ScheduleDeleteAndStartOver();
    std::move(callback).Run(status);
    return;
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(stored_registration_id);
  if (registration) {
    registration->SetStored();
    registration->set_resources_total_size_bytes(
        stored_resources_total_size_bytes);
  }
  context_->NotifyRegistrationStored(stored_registration_id, stored_scope);

  if (special_storage_policy_) {
    EnsureRegisteredOriginIsTracked(url::Origin::Create(stored_scope));
    OnStoragePolicyChanged();
  }

  std::move(callback).Run(status);
}

void ServiceWorkerRegistry::DidDeleteRegistration(
    int64_t registration_id,
    const GURL& origin,
    StatusCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    ServiceWorkerStorage::OriginState origin_state) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    ScheduleDeleteAndStartOver();
    std::move(callback).Run(status);
    return;
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration)
    registration->UnsetStored();

  if (origin_state == ServiceWorkerStorage::OriginState::kDelete) {
    context_->NotifyAllRegistrationsDeletedForOrigin(
        url::Origin::Create(origin));
    if (special_storage_policy_) {
      tracked_origins_for_policy_update_.erase(url::Origin::Create(origin));
    }
  }

  std::move(callback).Run(status);
}

void ServiceWorkerRegistry::DidUpdateToActiveState(
    const GURL& origin,
    StatusCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      status != storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }
  std::move(callback).Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerRegistry::DidWriteUncommittedResourceIds(
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk)
    ScheduleDeleteAndStartOver();
}

void ServiceWorkerRegistry::DidDoomUncommittedResourceIds(
    const std::vector<int64_t>& resource_ids,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk)
    ScheduleDeleteAndStartOver();
}

void ServiceWorkerRegistry::DidGetUserData(
    GetUserDataCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    const std::vector<std::string>& data) {
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      status != storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }
  std::move(callback).Run(data, DatabaseStatusToStatusCode(status));
}

void ServiceWorkerRegistry::DidGetUserKeysAndData(
    GetUserKeysAndDataCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    const base::flat_map<std::string, std::string>& data_map) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      status != storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }
  std::move(callback).Run(DatabaseStatusToStatusCode(status), data_map);
}

void ServiceWorkerRegistry::DidStoreUserData(
    StatusCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  // |status| can be NOT_FOUND when the associated registration did not exist in
  // the database. In the case, we don't have to schedule the corruption
  // recovery.
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      status != storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }
  std::move(callback).Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerRegistry::DidClearUserData(
    StatusCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk)
    ScheduleDeleteAndStartOver();
  std::move(callback).Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerRegistry::DidGetUserDataForAllRegistrations(
    GetUserDataForAllRegistrationsCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    std::vector<storage::mojom::ServiceWorkerUserDataPtr> entries) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  // TODO(crbug.com/1055677): Update call sites of
  // GetUserDataForAllRegistrations so that we can avoid converting mojo struct
  // to a pair.
  std::vector<std::pair<int64_t, std::string>> user_data;
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk)
    ScheduleDeleteAndStartOver();
  for (auto& entry : entries) {
    user_data.emplace_back(entry->registration_id, entry->value);
  }
  std::move(callback).Run(user_data, DatabaseStatusToStatusCode(status));
}

void ServiceWorkerRegistry::DidGetNewRegistrationId(
    blink::mojom::ServiceWorkerRegistrationOptions options,
    NewRegistrationCallback callback,
    int64_t registration_id) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId) {
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(base::MakeRefCounted<ServiceWorkerRegistration>(
      std::move(options), registration_id, context_->AsWeakPtr()));
}

void ServiceWorkerRegistry::DidGetNewVersionId(
    scoped_refptr<ServiceWorkerRegistration> registration,
    const GURL& script_url,
    blink::mojom::ScriptType script_type,
    NewVersionCallback callback,
    int64_t version_id,
    mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
        version_reference) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (version_id == blink::mojom::kInvalidServiceWorkerVersionId) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto version = base::MakeRefCounted<ServiceWorkerVersion>(
      registration.get(), script_url, script_type, version_id,
      std::move(version_reference), context_->AsWeakPtr());
  std::move(callback).Run(std::move(version));
}

void ServiceWorkerRegistry::ScheduleDeleteAndStartOver() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (!should_schedule_delete_and_start_over_) {
    // Recovery process has already been scheduled.
    return;
  }

  // Ideally, the corruption recovery should not be scheduled if the error
  // is transient as it can get healed soon (e.g. IO error). However we
  // unconditionally start recovery here for simplicity and low error rates.
  DVLOG(1) << "Schedule to delete the context and start over.";
  context_->ScheduleDeleteAndStartOver();
  // ServiceWorkerContextCore should call PrepareForDeleteAndStartOver().
  DCHECK(!should_schedule_delete_and_start_over_);
  DCHECK(is_storage_disabled_);
}

void ServiceWorkerRegistry::DidGetRegisteredOriginsOnStartup(
    const std::vector<url::Origin>& origins) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  for (const auto& origin : origins)
    EnsureRegisteredOriginIsTracked(origin);
  OnStoragePolicyChanged();
}

void ServiceWorkerRegistry::EnsureRegisteredOriginIsTracked(
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  auto it = tracked_origins_for_policy_update_.find(origin);
  if (it == tracked_origins_for_policy_update_.end())
    tracked_origins_for_policy_update_[origin] = {};
}

void ServiceWorkerRegistry::OnStoragePolicyChanged() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (is_storage_disabled_)
    return;

  std::vector<storage::mojom::LocalStoragePolicyUpdatePtr> policy_updates;
  for (auto& entry : tracked_origins_for_policy_update_) {
    const url::Origin& origin = entry.first;
    StorageOriginState& state = entry.second;
    state.should_purge_on_shutdown = ShouldPurgeOnShutdown(origin);
    if (state.should_purge_on_shutdown != state.will_purge_on_shutdown) {
      state.will_purge_on_shutdown = state.should_purge_on_shutdown;
      policy_updates.push_back(storage::mojom::LocalStoragePolicyUpdate::New(
          origin, state.should_purge_on_shutdown));
    }
  }

  if (!policy_updates.empty())
    GetRemoteStorageControl()->ApplyPolicyUpdates(std::move(policy_updates));
}

bool ServiceWorkerRegistry::ShouldPurgeOnShutdown(const url::Origin& origin) {
  if (!special_storage_policy_)
    return false;
  return special_storage_policy_->IsStorageSessionOnly(origin.GetURL()) &&
         !special_storage_policy_->IsStorageProtected(origin.GetURL());
}

mojo::Remote<storage::mojom::ServiceWorkerStorageControl>&
ServiceWorkerRegistry::GetRemoteStorageControl() {
  DCHECK(!(remote_storage_control_.is_bound() &&
           !remote_storage_control_.is_connected()))
      << "Rebinding is not supported yet.";

  if (!remote_storage_control_.is_bound()) {
    storage_control_->Bind(
        remote_storage_control_.BindNewPipeAndPassReceiver());
  }

  return remote_storage_control_;
}

}  // namespace content
