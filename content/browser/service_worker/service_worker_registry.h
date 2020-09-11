// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRY_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}  // namespace storage

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerVersion;
class ServiceWorkerStorageControlImpl;

class ServiceWorkerRegistryTest;
FORWARD_DECLARE_TEST(ServiceWorkerRegistryTest, StoragePolicyChange);

// This class manages in-memory representation of service worker registrations
// (i.e., ServiceWorkerRegistration) including installing and uninstalling
// registrations. The instance of this class is owned by
// ServiceWorkerContextCore and has the same lifetime of the owner.
// The instance owns ServiceworkerStorage and uses it to store/retrieve
// registrations to/from persistent storage.
// The instance lives on the core thread.
class CONTENT_EXPORT ServiceWorkerRegistry {
 public:
  using ResourceList = ServiceWorkerStorage::ResourceList;
  using RegistrationList = ServiceWorkerStorage::RegistrationList;
  using FindRegistrationCallback = base::OnceCallback<void(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration)>;
  using GetRegistrationsCallback = base::OnceCallback<void(
      blink::ServiceWorkerStatusCode status,
      const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
          registrations)>;
  using GetRegistrationsInfosCallback = base::OnceCallback<void(
      blink::ServiceWorkerStatusCode status,
      const std::vector<ServiceWorkerRegistrationInfo>& registrations)>;
  using GetUserDataCallback =
      base::OnceCallback<void(const std::vector<std::string>& data,
                              blink::ServiceWorkerStatusCode status)>;
  using GetUserKeysAndDataCallback = base::OnceCallback<void(
      blink::ServiceWorkerStatusCode status,
      const base::flat_map<std::string, std::string>& data_map)>;
  using GetUserDataForAllRegistrationsCallback = base::OnceCallback<void(
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      blink::ServiceWorkerStatusCode status)>;
  using GetStorageUsageForOriginCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status,
                              int64_t usage)>;
  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status)>;

  ServiceWorkerRegistry(
      const base::FilePath& user_data_directory,
      ServiceWorkerContextCore* context,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy);

  // For re-creating the registry from the old one. This is called when
  // something went wrong during storage access.
  ServiceWorkerRegistry(ServiceWorkerContextCore* context,
                        ServiceWorkerRegistry* old_registry);

  ~ServiceWorkerRegistry();

  ServiceWorkerStorage* storage() const;

  // Creates a new in-memory representation of registration. Can be null when
  // storage is disabled. This method must be called after storage is
  // initialized.
  using NewRegistrationCallback = base::OnceCallback<void(
      scoped_refptr<ServiceWorkerRegistration> registration)>;
  void CreateNewRegistration(
      blink::mojom::ServiceWorkerRegistrationOptions options,
      NewRegistrationCallback callback);

  // Create a new instance of ServiceWorkerVersion which is associated with the
  // given |registration|. Can be null when storage is disabled. This method
  // must be called after storage is initialized.
  using NewVersionCallback =
      base::OnceCallback<void(scoped_refptr<ServiceWorkerVersion> version)>;
  void CreateNewVersion(scoped_refptr<ServiceWorkerRegistration> registration,
                        const GURL& script_url,
                        blink::mojom::ScriptType script_type,
                        NewVersionCallback callback);

  // Finds registration for |client_url| or |scope| or |registration_id|.
  // The Find methods will find stored and initially installing registrations.
  // Returns blink::ServiceWorkerStatusCode::kOk with non-null
  // registration if registration is found, or returns
  // blink::ServiceWorkerStatusCode::kErrorNotFound if no
  // matching registration is found.  The FindRegistrationForScope method is
  // guaranteed to return asynchronously. However, the methods to find
  // for |client_url| or |registration_id| may complete immediately
  // (the callback may be called prior to the method returning) or
  // asynchronously.
  void FindRegistrationForClientUrl(const GURL& client_url,
                                    FindRegistrationCallback callback);
  void FindRegistrationForScope(const GURL& scope,
                                FindRegistrationCallback callback);
  // These FindRegistrationForId() methods look up live registrations and may
  // return a "findable" registration without looking up storage. A registration
  // is considered as "findable" when the registration is stored or in the
  // installing state.
  void FindRegistrationForId(int64_t registration_id,
                             const url::Origin& origin,
                             FindRegistrationCallback callback);
  // Generally |FindRegistrationForId| should be used to look up a registration
  // by |registration_id| since it's more efficient. But if a |registration_id|
  // is all that is available this method can be used instead.
  // Like |FindRegistrationForId| this method may complete immediately (the
  // callback may be called prior to the method returning) or asynchronously.
  void FindRegistrationForIdOnly(int64_t registration_id,
                                 FindRegistrationCallback callback);

  // Returns all stored and installing registrations for a given origin.
  void GetRegistrationsForOrigin(const url::Origin& origin,
                                 GetRegistrationsCallback callback);

  // Reads the total resource size stored in the storage for a given origin.
  void GetStorageUsageForOrigin(const url::Origin& origin,
                                GetStorageUsageForOriginCallback callback);

  // Returns info about all stored and initially installing registrations.
  // TODO(crbug.com/807440,1055677): Consider removing this method. Getting all
  // registrations at once might not be a good idea.
  void GetAllRegistrationsInfos(GetRegistrationsInfosCallback callback);

  ServiceWorkerRegistration* GetUninstallingRegistration(const GURL& scope);

  std::vector<scoped_refptr<ServiceWorkerRegistration>>
  GetUninstallingRegistrationsForOrigin(const url::Origin& origin);

  // Commits |registration| with the installed but not activated |version|
  // to storage, overwriting any pre-existing registration data for the scope.
  // A pre-existing version's script resources remain available if that version
  // is live. ServiceWorkerStorage::PurgeResources() should be called when it's
  // OK to delete them.
  void StoreRegistration(ServiceWorkerRegistration* registration,
                         ServiceWorkerVersion* version,
                         StatusCallback callback);

  // Deletes the registration data for |registration|. The live registration is
  // still findable via GetUninstallingRegistration(), and versions are usable
  // because their script resources have not been deleted. After calling this,
  // the caller should later:
  // - Call NotifyDoneUninstallingRegistration() to let registry know the
  //   uninstalling operation is done.
  // - If it no longer wants versions to be usable, call
  //   ServiceWorkerStorage::PurgeResources() to delete their script resources.
  // If these aren't called, on the next profile session the cleanup occurs.
  void DeleteRegistration(scoped_refptr<ServiceWorkerRegistration> registration,
                          const GURL& origin,
                          StatusCallback callback);

  // Intended for use only by ServiceWorkerRegisterJob and
  // ServiceWorkerRegistration.
  void NotifyInstallingRegistration(ServiceWorkerRegistration* registration);
  void NotifyDoneInstallingRegistration(ServiceWorkerRegistration* registration,
                                        ServiceWorkerVersion* version,
                                        blink::ServiceWorkerStatusCode status);
  void NotifyDoneUninstallingRegistration(
      ServiceWorkerRegistration* registration,
      ServiceWorkerRegistration::Status new_status);

  // Wrapper functions of ServiceWorkerStorage. These wrappers provide error
  // recovering mechanism when database operations fail.
  void UpdateToActiveState(int64_t registration_id,
                           const GURL& origin,
                           StatusCallback callback);
  void UpdateLastUpdateCheckTime(int64_t registration_id,
                                 const GURL& origin,
                                 base::Time last_update_check_time,
                                 StatusCallback callback);
  void UpdateNavigationPreloadEnabled(int64_t registration_id,
                                      const GURL& origin,
                                      bool enable,
                                      StatusCallback callback);
  void UpdateNavigationPreloadHeader(int64_t registration_id,
                                     const GURL& origin,
                                     const std::string& value,
                                     StatusCallback callback);
  void StoreUncommittedResourceId(int64_t resource_id, const GURL& origin);
  void DoomUncommittedResource(int64_t resource_id);
  void GetUserData(int64_t registration_id,
                   const std::vector<std::string>& keys,
                   GetUserDataCallback callback);
  void GetUserDataByKeyPrefix(int64_t registration_id,
                              const std::string& key_prefix,
                              GetUserDataCallback callback);
  void GetUserKeysAndDataByKeyPrefix(int64_t registration_id,
                                     const std::string& key_prefix,
                                     GetUserKeysAndDataCallback callback);
  void StoreUserData(
      int64_t registration_id,
      const url::Origin& origin,
      const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
      StatusCallback callback);
  void ClearUserData(int64_t registration_id,
                     const std::vector<std::string>& keys,
                     StatusCallback callback);
  void ClearUserDataByKeyPrefixes(int64_t registration_id,
                                  const std::vector<std::string>& key_prefixes,
                                  StatusCallback callback);
  void ClearUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      StatusCallback callback);
  void GetUserDataForAllRegistrations(
      const std::string& key,
      GetUserDataForAllRegistrationsCallback callback);
  void GetUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      GetUserDataForAllRegistrationsCallback callback);

  mojo::Remote<storage::mojom::ServiceWorkerStorageControl>&
  GetRemoteStorageControl();

  // Disables the internal storage to prepare for error recovery.
  void PrepareForDeleteAndStarOver();

  // Deletes this registry and internal storage, then starts over for error
  // recovery.
  void DeleteAndStartOver(StatusCallback callback);

  void DisableDeleteAndStartOverForTesting();

 private:
  friend class ServiceWorkerRegistryTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerRegistryTest, StoragePolicyChange);

  void Start();

  ServiceWorkerRegistration* FindInstallingRegistrationForClientUrl(
      const GURL& client_url);
  ServiceWorkerRegistration* FindInstallingRegistrationForScope(
      const GURL& scope);
  ServiceWorkerRegistration* FindInstallingRegistrationForId(
      int64_t registration_id);

  scoped_refptr<ServiceWorkerRegistration> GetOrCreateRegistration(
      const storage::mojom::ServiceWorkerRegistrationData& data,
      const ResourceList& resources,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
          version_reference);

  // Looks up live registrations and returns an optional value which may contain
  // a "findable" registration. See the implementation of this method for
  // what "findable" means and when a registration is returned.
  base::Optional<scoped_refptr<ServiceWorkerRegistration>>
  FindFromLiveRegistrationsForId(int64_t registration_id);

  void DoomUncommittedResources(const std::vector<int64_t>& resource_ids);

  void DidFindRegistrationForClientUrl(
      const GURL& client_url,
      int64_t trace_event_id,
      FindRegistrationCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      storage::mojom::ServiceWorkerFindRegistrationResultPtr result);
  void DidFindRegistrationForScope(
      FindRegistrationCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      storage::mojom::ServiceWorkerFindRegistrationResultPtr result);
  void DidFindRegistrationForId(
      int64_t registration_id,
      FindRegistrationCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      storage::mojom::ServiceWorkerFindRegistrationResultPtr result);

  void DidGetRegistrationsForOrigin(
      GetRegistrationsCallback callback,
      const url::Origin& origin_filter,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      std::vector<storage::mojom::ServiceWorkerFindRegistrationResultPtr>
          entries);
  void DidGetAllRegistrations(
      GetRegistrationsInfosCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      RegistrationList registration_data_list);

  void DidStoreRegistration(
      int64_t stored_registration_id,
      uint64_t stored_resources_total_size_bytes,
      const GURL& stored_scope,
      StatusCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status);
  void DidDeleteRegistration(
      int64_t registration_id,
      const GURL& origin,
      StatusCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      ServiceWorkerStorage::OriginState origin_state);

  void DidUpdateToActiveState(
      const GURL& origin,
      StatusCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidWriteUncommittedResourceIds(
      storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidDoomUncommittedResourceIds(
      const std::vector<int64_t>& resource_ids,
      storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidGetUserData(GetUserDataCallback callback,
                      storage::mojom::ServiceWorkerDatabaseStatus status,
                      const std::vector<std::string>& data);
  void DidGetUserKeysAndData(
      GetUserKeysAndDataCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus status,
      const base::flat_map<std::string, std::string>& data_map);
  void DidStoreUserData(StatusCallback callback,
                        storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidClearUserData(StatusCallback callback,
                        storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidGetUserDataForAllRegistrations(
      GetUserDataForAllRegistrationsCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus status,
      std::vector<storage::mojom::ServiceWorkerUserDataPtr> entries);

  void DidGetNewRegistrationId(
      blink::mojom::ServiceWorkerRegistrationOptions options,
      NewRegistrationCallback callback,
      int64_t registration_id);
  void DidGetNewVersionId(
      scoped_refptr<ServiceWorkerRegistration> registration,
      const GURL& script_url,
      blink::mojom::ScriptType script_type,
      NewVersionCallback callback,
      int64_t version_id,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
          version_reference);

  void ScheduleDeleteAndStartOver();

  // TODO(bashi): Consider introducing a helper class that handles the below.
  // These are almost the same as DOMStorageContextWrapper.
  void DidGetRegisteredOriginsOnStartup(
      const std::vector<url::Origin>& origins);
  void EnsureRegisteredOriginIsTracked(const url::Origin& origin);
  void OnStoragePolicyChanged();
  bool ShouldPurgeOnShutdown(const url::Origin& origin);

  // The ServiceWorkerContextCore object must outlive this.
  ServiceWorkerContextCore* const context_;

  mojo::Remote<storage::mojom::ServiceWorkerStorageControl>
      remote_storage_control_;
  // TODO(crbug.com/1055677): Remove this field after all storage operations are
  // called via |remote_storage_control_|. An instance of this impl should live
  // in the storage service.
  std::unique_ptr<ServiceWorkerStorageControlImpl> storage_control_;

  bool is_storage_disabled_ = false;

  const scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  class StoragePolicyObserver;
  base::SequenceBound<StoragePolicyObserver> storage_policy_observer_;

  // TODO(bashi): Avoid duplication. Merge this with LocalStorageOriginState.
  struct StorageOriginState {
    bool should_purge_on_shutdown = false;
    bool will_purge_on_shutdown = false;
  };
  // IMPORTANT: Don't use this other than updating storage policies. This can
  // be out of sync with |registered_origins_| in ServiceWorkerStorage.
  std::map<url::Origin, StorageOriginState> tracked_origins_for_policy_update_;

  // For finding registrations being installed or uninstalled.
  using RegistrationRefsById =
      std::map<int64_t, scoped_refptr<ServiceWorkerRegistration>>;
  RegistrationRefsById installing_registrations_;
  RegistrationRefsById uninstalling_registrations_;

  // Indicates whether recovery process should be scheduled.
  bool should_schedule_delete_and_start_over_ = true;

  base::WeakPtrFactory<ServiceWorkerRegistry> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRY_H_
