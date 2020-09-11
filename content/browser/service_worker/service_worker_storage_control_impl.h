// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_CONTROL_IMPL_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_CONTROL_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

class ServiceWorkerLiveVersionRefImpl;

// This class wraps ServiceWorkerStorage to implement mojo interface defined by
// the storage service, i.e., ServiceWorkerStorageControl.
// TODO(crbug.com/1055677): Merge this implementation into ServiceWorkerStorage
// and move the merged class to components/services/storage.
class CONTENT_EXPORT ServiceWorkerStorageControlImpl
    : public storage::mojom::ServiceWorkerStorageControl {
 public:
  explicit ServiceWorkerStorageControlImpl(
      std::unique_ptr<ServiceWorkerStorage> storage);

  ServiceWorkerStorageControlImpl(const ServiceWorkerStorageControlImpl&) =
      delete;
  ServiceWorkerStorageControlImpl& operator=(
      const ServiceWorkerStorageControlImpl&) = delete;

  ~ServiceWorkerStorageControlImpl() override;

  // TODO(crbug.com/1055677): Remove this accessor after all
  // ServiceWorkerStorage method calls are replaced with mojo methods.
  ServiceWorkerStorage* storage() const { return storage_.get(); }

  void Bind(mojo::PendingReceiver<storage::mojom::ServiceWorkerStorageControl>
                receiver);

  void OnNoLiveVersion(int64_t version_id);

  void LazyInitializeForTest();

 private:
  void Disable() override;
  void Delete(DeleteCallback callback) override;
  // storage::mojom::ServiceWorkerStorageControl implementations:
  void GetRegisteredOrigins(GetRegisteredOriginsCallback callback) override;
  void FindRegistrationForClientUrl(
      const GURL& client_url,
      FindRegistrationForClientUrlCallback callback) override;
  void FindRegistrationForScope(
      const GURL& scope,
      FindRegistrationForScopeCallback callback) override;
  void FindRegistrationForId(int64_t registration_id,
                             const base::Optional<url::Origin>& origin,
                             FindRegistrationForIdCallback callback) override;
  void GetRegistrationsForOrigin(
      const url::Origin& origin,
      GetRegistrationsForOriginCallback callback) override;
  void GetUsageForOrigin(const url::Origin& origin,
                         GetUsageForOriginCallback callback) override;
  void GetAllRegistrationsDeprecated(
      GetAllRegistrationsDeprecatedCallback calback) override;
  void StoreRegistration(
      storage::mojom::ServiceWorkerRegistrationDataPtr registration,
      std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources,
      StoreRegistrationCallback callback) override;
  void DeleteRegistration(int64_t registration_id,
                          const GURL& origin,
                          DeleteRegistrationCallback callback) override;
  void UpdateToActiveState(int64_t registration_id,
                           const GURL& origin,
                           UpdateToActiveStateCallback callback) override;
  void UpdateLastUpdateCheckTime(
      int64_t registration_id,
      const GURL& origin,
      base::Time last_update_check_time,
      UpdateLastUpdateCheckTimeCallback callback) override;
  void UpdateNavigationPreloadEnabled(
      int64_t registration_id,
      const GURL& origin,
      bool enable,
      UpdateNavigationPreloadEnabledCallback callback) override;
  void UpdateNavigationPreloadHeader(
      int64_t registration_id,
      const GURL& origin,
      const std::string& value,
      UpdateNavigationPreloadHeaderCallback callback) override;
  void GetNewRegistrationId(GetNewRegistrationIdCallback callback) override;
  void GetNewVersionId(GetNewVersionIdCallback callback) override;
  void GetNewResourceId(GetNewResourceIdCallback callback) override;
  void CreateResourceReader(
      int64_t resource_id,
      mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceReader> reader)
      override;
  void CreateResourceWriter(
      int64_t resource_id,
      mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceWriter> writer)
      override;
  void CreateResourceMetadataWriter(
      int64_t resource_id,
      mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceMetadataWriter>
          writer) override;
  void StoreUncommittedResourceId(
      int64_t resource_id,
      const GURL& origin,
      StoreUncommittedResourceIdCallback callback) override;
  void DoomUncommittedResources(
      const std::vector<int64_t>& resource_ids,
      DoomUncommittedResourcesCallback callback) override;
  void GetUserData(int64_t registration_id,
                   const std::vector<std::string>& keys,
                   GetUserDataCallback callback) override;
  void StoreUserData(
      int64_t registration_id,
      const url::Origin& origin,
      std::vector<storage::mojom::ServiceWorkerUserDataPtr> user_data,
      StoreUserDataCallback callback) override;
  void ClearUserData(int64_t registration_id,
                     const std::vector<std::string>& keys,
                     ClearUserDataCallback callback) override;
  void GetUserDataByKeyPrefix(int64_t registration_id,
                              const std::string& key_prefix,
                              GetUserDataByKeyPrefixCallback callback) override;
  void GetUserKeysAndDataByKeyPrefix(
      int64_t registration_id,
      const std::string& key_prefix,
      GetUserKeysAndDataByKeyPrefixCallback callback) override;
  void ClearUserDataByKeyPrefixes(
      int64_t registration_id,
      const std::vector<std::string>& key_prefixes,
      ClearUserDataByKeyPrefixesCallback callback) override;
  void GetUserDataForAllRegistrations(
      const std::string& key,
      GetUserDataForAllRegistrationsCallback callback) override;
  void GetUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      GetUserDataForAllRegistrationsByKeyPrefixCallback callback) override;
  void ClearUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      ClearUserDataForAllRegistrationsByKeyPrefixCallback callback) override;
  void PerformStorageCleanup(PerformStorageCleanupCallback callback) override;
  void ApplyPolicyUpdates(
      const std::vector<storage::mojom::LocalStoragePolicyUpdatePtr>
          policy_updates) override;

  using ResourceList =
      std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>;

  // Callbacks for ServiceWorkerStorage methods.
  void DidFindRegistration(
      base::OnceCallback<void(
          storage::mojom::ServiceWorkerDatabaseStatus status,
          storage::mojom::ServiceWorkerFindRegistrationResultPtr)> callback,
      storage::mojom::ServiceWorkerRegistrationDataPtr data,
      std::unique_ptr<ResourceList> resources,
      storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidGetRegistrationsForOrigin(
      GetRegistrationsForOriginCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus status,
      std::unique_ptr<ServiceWorkerStorage::RegistrationList>
          registration_data_list,
      std::unique_ptr<std::vector<ResourceList>> resources_list);
  void DidStoreRegistration(
      StoreRegistrationCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus status,
      int64_t deleted_version_id,
      const std::vector<int64_t>& newly_purgeable_resources);
  void DidDeleteRegistration(
      DeleteRegistrationCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus status,
      ServiceWorkerStorage::OriginState origin_state,
      int64_t deleted_version_id,
      const std::vector<int64_t>& newly_purgeable_resources);
  void DidGetNewVersionId(GetNewVersionIdCallback callback, int64_t version_id);

  mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
  CreateLiveVersionReferenceRemote(int64_t version_id);

  void MaybePurgeResources(int64_t version_id,
                           const std::vector<int64_t>& purgeable_resources);

  const std::unique_ptr<ServiceWorkerStorage> storage_;

  mojo::ReceiverSet<storage::mojom::ServiceWorkerStorageControl> receivers_;

  base::flat_map<int64_t /*version_id*/,
                 std::unique_ptr<ServiceWorkerLiveVersionRefImpl>>
      live_versions_;

  base::WeakPtrFactory<ServiceWorkerStorageControlImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_CONTROLIMPL_H_
