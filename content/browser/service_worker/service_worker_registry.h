// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRY_H_

#include <memory>
#include <optional>
#include <tuple>

#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/quota/storage_policy_observer.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_ancestor_frame_type.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}  // namespace storage

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerVersion;

class ServiceWorkerRegistryTest;
FORWARD_DECLARE_TEST(ServiceWorkerRegistryTest, StoragePolicyChange);

// Manages in-memory representation of service worker registrations
// (i.e., ServiceWorkerRegistration) including installing and uninstalling
// registrations. Owned by ServiceWorkerContextCore and has the same lifetime of
// the owner. Lives on the UI thread.
//
// Communicates with ServiceWorkerStorageControl via a mojo remote to
// store/retrieve registrations to/from persistent storage. Most methods of this
// class call relevant mojo methods. Callbacks passed to these methods are
// invoked after mojo calls complete.
//
// The implementation of ServiceWorkerStorageControl lives in the Storage
// Service. This means that the mojo remote can be disconnected when the
// Storage Service crashes. To avoid a situation where callbacks never get
// invoked this class has a recovery mechanism. Upon detecting a disconnection,
// this tries to reconnect to ServiceWorkerStorageControl then retry all
// inflight mojo calls.
class CONTENT_EXPORT ServiceWorkerRegistry {
 public:
  using ResourceList =
      std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>;
  using RegistrationList =
      std::vector<storage::mojom::ServiceWorkerRegistrationDataPtr>;
  using FindRegistrationCallback = base::OnceCallback<void(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration)>;
  using GetRegisteredStorageKeysCallback = base::OnceCallback<void(
      const std::vector<blink::StorageKey>& storage_keys)>;
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
  using GetStorageUsageForStorageKeyCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status,
                              int64_t usage)>;
  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status)>;

  enum class Purpose { kNotForNavigation, kNavigation };

  ServiceWorkerRegistry(ServiceWorkerContextCore* context,
                        storage::QuotaManagerProxy* quota_manager_proxy,
                        storage::SpecialStoragePolicy* special_storage_policy);

  // For re-creating the registry from the old one. This is called when
  // something went wrong during storage access.
  ServiceWorkerRegistry(ServiceWorkerContextCore* context,
                        ServiceWorkerRegistry* old_registry);

  ~ServiceWorkerRegistry();

  // Creates a new in-memory representation of registration. Can be null when
  // storage is disabled. This method must be called after storage is
  // initialized.
  using NewRegistrationCallback = base::OnceCallback<void(
      scoped_refptr<ServiceWorkerRegistration> registration)>;
  void CreateNewRegistration(
      blink::mojom::ServiceWorkerRegistrationOptions options,
      const blink::StorageKey& key,
      blink::mojom::AncestorFrameType ancestor_frame_type,
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

  // Finds registration for `client_url`, `scope`, or `registration_id` with the
  // associated `key`. The Find methods will find stored and initially
  // installing registrations. Returns blink::ServiceWorkerStatusCode::kOk with
  // non-null registration if registration is found, or returns
  // blink::ServiceWorkerStatusCode::kErrorNotFound if no
  // matching registration is found.  The FindRegistrationForScope method is
  // guaranteed to return asynchronously. However, the methods to find
  // for `client_url` or `registration_id` may complete immediately
  // (the callback may be called prior to the method returning) or
  // asynchronously.
  void FindRegistrationForClientUrl(Purpose purpose,
                                    const GURL& client_url,
                                    const blink::StorageKey& key,
                                    FindRegistrationCallback callback);
  void FindRegistrationForScope(const GURL& scope,
                                const blink::StorageKey& key,
                                FindRegistrationCallback callback);
  // These FindRegistrationForId() methods look up live registrations and may
  // return a "findable" registration without looking up storage. A registration
  // is considered as "findable" when the registration is stored or in the
  // installing state.
  void FindRegistrationForId(int64_t registration_id,
                             const blink::StorageKey& key,
                             FindRegistrationCallback callback);
  // Generally |FindRegistrationForId| should be used to look up a registration
  // by |registration_id| since it's more efficient. But if a |registration_id|
  // is all that is available this method can be used instead.
  // Like |FindRegistrationForId| this method may complete immediately (the
  // callback may be called prior to the method returning) or asynchronously.
  void FindRegistrationForIdOnly(int64_t registration_id,
                                 FindRegistrationCallback callback);
  // Returns all stored and installing registrations for a given StorageKey.
  void GetRegistrationsForStorageKey(const blink::StorageKey& key,
                                     GetRegistrationsCallback callback);
  // Reads the total resource size stored in the storage for a given storage
  // key.
  void GetStorageUsageForStorageKey(
      const blink::StorageKey& key,
      GetStorageUsageForStorageKeyCallback callback);

  // Returns info about all stored and initially installing registrations.
  // TODO(crbug.com/807440,1055677): Consider removing this method. Getting all
  // registrations at once might not be a good idea.
  void GetAllRegistrationsInfos(GetRegistrationsInfosCallback callback);
  ServiceWorkerRegistration* GetUninstallingRegistration(
      const GURL& scope,
      const blink::StorageKey& key);
  std::vector<scoped_refptr<ServiceWorkerRegistration>>
  GetUninstallingRegistrationsForStorageKey(const blink::StorageKey& key);

  // Commits |registration| with the installed but not activated |version|
  // to storage, overwriting any pre-existing registration data for the scope.
  // A pre-existing version's script resources remain available if that version
  // is live. ServiceWorkerStorage::PurgeResources() should be called when it's
  // OK to delete them.
  void StoreRegistration(ServiceWorkerRegistration* registration,
                         ServiceWorkerVersion* version,
                         StatusCallback callback);

  // Deletes the registration data for `registration`. The live registration is
  // still findable via GetUninstallingRegistration(), and versions are usable
  // because their script resources have not been deleted. After calling this,
  // the caller should later:
  // - Call NotifyDoneUninstallingRegistration() to let registry know the
  //   uninstalling operation is done.
  // - If it no longer wants versions to be usable, call
  //   ServiceWorkerStorage::PurgeResources() to delete their script resources.
  // If these aren't called, on the next profile session the cleanup occurs.
  void DeleteRegistration(scoped_refptr<ServiceWorkerRegistration> registration,
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
                           const blink::StorageKey& key,
                           StatusCallback callback);
  void UpdateLastUpdateCheckTime(int64_t registration_id,
                                 const blink::StorageKey& key,
                                 base::Time last_update_check_time,
                                 StatusCallback callback);
  void UpdateNavigationPreloadEnabled(int64_t registration_id,
                                      const blink::StorageKey& key,
                                      bool enable,
                                      StatusCallback callback);
  void UpdateNavigationPreloadHeader(int64_t registration_id,
                                     const blink::StorageKey& key,
                                     const std::string& value,
                                     StatusCallback callback);
  void UpdateFetchHandlerType(
      int64_t registration_id,
      const blink::StorageKey& key,
      blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type,
      StatusCallback callback);
  void UpdateResourceSha256Checksums(
      int64_t registration_id,
      const blink::StorageKey& key,
      const base::flat_map<int64_t, std::string>& updated_sha256_checksums,
      StatusCallback callback);
  void StoreUncommittedResourceId(int64_t resource_id,
                                  const blink::StorageKey& key);
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
      const blink::StorageKey& key,
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

  // Returns a set of storage keys which have at least one stored registration.
  // The set doesn't include installing/uninstalling/uninstalled registrations.
  void GetRegisteredStorageKeys(GetRegisteredStorageKeysCallback callback);

  // Performs internal storage cleanup. Operations to the storage in the past
  // (e.g. deletion) are usually recorded in disk for a certain period until
  // compaction happens. This method wipes them out to ensure that the deleted
  // entries and other traces like log files are removed.
  void PerformStorageCleanup(base::OnceClosure callback);

  // Disables the internal storage to prepare for error recovery.
  void PrepareForDeleteAndStartOver();
  // Deletes this registry and internal storage, then starts over for error
  // recovery.
  void DeleteAndStartOver(StatusCallback callback);

  mojo::Remote<storage::mojom::ServiceWorkerStorageControl>&
  GetRemoteStorageControl();

  // Call storage::mojom::ServiceWorkerStorageControl::Disable() immediately.
  // This method sends an IPC message without using the queuing mechanism.
  void DisableStorageForTesting(base::OnceClosure callback);

 private:
  friend class ServiceWorkerRegistryTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerRegistryTest, StoragePolicyChange);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerRegistryTest,
                           RetryInflightCalls_ApplyPolicyUpdates);

  void Start();
  void FindRegistrationForIdInternal(
      int64_t registration_id,
      const std::optional<blink::StorageKey>& key,
      FindRegistrationCallback callback);
  ServiceWorkerRegistration* FindInstallingRegistrationForClientUrl(
      const GURL& client_url,
      const blink::StorageKey& key);
  ServiceWorkerRegistration* FindInstallingRegistrationForScope(
      const GURL& scope,
      const blink::StorageKey& key);
  ServiceWorkerRegistration* FindInstallingRegistrationForId(
      int64_t registration_id);

  scoped_refptr<ServiceWorkerRegistration> GetOrCreateRegistration(
      const storage::mojom::ServiceWorkerRegistrationData& data,
      const ResourceList& resources,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
          version_reference);

  void CreateNewRegistrationWithBucketInfo(
      blink::mojom::ServiceWorkerRegistrationOptions options,
      const blink::StorageKey& key,
      blink::mojom::AncestorFrameType ancestor_frame_type,
      NewRegistrationCallback callback,
      storage::QuotaErrorOr<storage::BucketInfo> result);

  // Looks up live registrations and returns an optional value which may contain
  // a "findable" registration. See the implementation of this method for
  // what "findable" means and when a registration is returned.
  std::optional<scoped_refptr<ServiceWorkerRegistration>>
  FindFromLiveRegistrationsForId(int64_t registration_id);

  void DoomUncommittedResources(const std::vector<int64_t>& resource_ids);
  void DidFindRegistrationForClientUrl(
      const GURL& client_url,
      const blink::StorageKey& key,
      int64_t trace_event_id,
      FindRegistrationCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      storage::mojom::ServiceWorkerFindRegistrationResultPtr result,
      const std::optional<std::vector<GURL>>& scopes);
  void RunFindRegistrationCallbacks(
      const GURL& client_url,
      const blink::StorageKey& key,
      scoped_refptr<ServiceWorkerRegistration> registration,
      blink::ServiceWorkerStatusCode status);
  void DidFindRegistrationForScope(
      FindRegistrationCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      storage::mojom::ServiceWorkerFindRegistrationResultPtr result);
  void DidFindRegistrationForId(
      int64_t registration_id,
      FindRegistrationCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      storage::mojom::ServiceWorkerFindRegistrationResultPtr result);
  void DidGetRegistrationsForStorageKey(
      GetRegistrationsCallback callback,
      const blink::StorageKey& key_filter,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      std::vector<storage::mojom::ServiceWorkerFindRegistrationResultPtr>
          entries);
  void DidGetAllRegistrations(
      GetRegistrationsInfosCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      RegistrationList registration_data_list);
  void DidGetStorageUsageForStorageKey(
      GetStorageUsageForStorageKeyCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      int64_t usage);
  void DidStoreRegistration(
      int64_t stored_registration_id,
      uint64_t stored_resources_total_size_bytes,
      const GURL& stored_scope,
      const blink::StorageKey& key,
      StatusCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      uint64_t deleted_resources_size);
  void NotifyRegistrationStored(int64_t stored_registration_id,
                                uint64_t stored_resources_total_size_bytes,
                                const GURL& stored_scope,
                                const blink::StorageKey& key,
                                StatusCallback callback);
  void DidDeleteRegistration(
      int64_t registration_id,
      const GURL& stored_scope,
      const blink::StorageKey& key,
      StatusCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus database_status,
      uint64_t deleted_resources_size,
      storage::mojom::ServiceWorkerStorageStorageKeyState storage_key_state);
  void NotifyRegistrationDeletedForStorageKey(
      int64_t registration_id,
      const GURL& stored_scope,
      const blink::StorageKey& key,
      storage::mojom::ServiceWorkerStorageStorageKeyState storage_key_state,
      StatusCallback callback);
  void DidUpdateRegistration(
      StatusCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidUpdateToActiveState(
      const blink::StorageKey& key,
      StatusCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidWriteUncommittedResourceIds(
      const blink::StorageKey& key,
      storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidDoomUncommittedResourceIds(
      storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidGetUserData(GetUserDataCallback callback,
                      storage::mojom::ServiceWorkerDatabaseStatus status,
                      const std::vector<std::string>& data);
  void DidGetUserKeysAndData(
      GetUserKeysAndDataCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus status,
      const base::flat_map<std::string, std::string>& data_map);
  void DidStoreUserData(StatusCallback callback,
                        const blink::StorageKey& key,
                        storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidClearUserData(StatusCallback callback,
                        storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidGetUserDataForAllRegistrations(
      GetUserDataForAllRegistrationsCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus status,
      std::vector<storage::mojom::ServiceWorkerUserDataPtr> entries);

  void DidGetNewRegistrationId(
      blink::mojom::ServiceWorkerRegistrationOptions options,
      const blink::StorageKey& key,
      blink::mojom::AncestorFrameType ancestor_frame_type,
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
  void DidDeleteAndStartOver(
      StatusCallback callback,
      storage::mojom::ServiceWorkerDatabaseStatus status);

  void DidGetRegisteredStorageKeys(GetRegisteredStorageKeysCallback callback,
                                   const std::vector<blink::StorageKey>& keys);
  void DidPerformStorageCleanup(base::OnceClosure callback);
  void DidDisable();
  void DidApplyPolicyUpdates(
      storage::mojom::ServiceWorkerDatabaseStatus status);
  void DidGetRegisteredStorageKeysOnStartup(
      const std::vector<blink::StorageKey>& storage_keys);
  void ApplyPolicyUpdates(
      std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates);
  bool ShouldPurgeOnShutdownForTesting(const blink::StorageKey& key);

  void OnRemoteStorageDisconnected();

  void DidRecover();

  // Represents an inflight mojo remote call. Used to support retry.
  class InflightCall {
   public:
    virtual ~InflightCall() = default;
    virtual void Run() = 0;
  };

  template <typename...>
  friend class InflightCallWithInvoker;

  void StartRemoteCall(std::unique_ptr<InflightCall> call);
  void FinishRemoteCall(const InflightCall* call);

  // A helper function to call a mojo remote call that will automatically be
  // reissued if the mojo::Remote becomes disconnected. To allow the call to be
  // dispatched multiple times, all arguments must be either be:
  //
  // - passed by const reference
  // - copyable
  // - or clonable via `mojo::Clone()`.
  //
  // Example:
  //
  //   (in mojom)
  //   Foo(int64 arg1, int64 arg2) => (ServiceWorkerDatabaseStatus status);
  //
  //   CreateInvokerAndStartRemoteCall(
  //       &storage::mojom::ServiceWorkerStorageControl::Foo,
  //       base::BindOnce(&ServiceWorkerRegistry::DidFoo,
  //                      weak_factory_.GetWeakPtr(),
  //                      std::move(callback)),
  //       arg1, arg2);
  //
  //   void ServiceWorkerRegistry::DidFoo(
  //       FooCallback callback,
  //       storage::mojom::ServiceWorkerDatabaseStatus status) {
  //     // ...
  //   }
  template <typename Functor, typename... Args, typename... CallbackArgs>
  void CreateInvokerAndStartRemoteCall(
      Functor&& f,
      base::OnceCallback<void(CallbackArgs...)> callback,
      Args&&... args);

  // The ServiceWorkerContextCore object must outlive this.
  const raw_ptr<ServiceWorkerContextCore> context_;

  mojo::Remote<storage::mojom::ServiceWorkerStorageControl>
      remote_storage_control_;

  bool is_storage_disabled_ = false;

  // TODO(crbug.com/40103974): Consider moving QuotaManagerProxy to
  // ServiceWorkerStorage once QuotaManager gets mojofied.
  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  const scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  std::optional<storage::StoragePolicyObserver> storage_policy_observer_;

  // For finding registrations being installed or uninstalled.
  using RegistrationRefsById =
      std::map<int64_t, scoped_refptr<ServiceWorkerRegistration>>;
  RegistrationRefsById installing_registrations_;
  RegistrationRefsById uninstalling_registrations_;

  // Indicates whether recovery process should be scheduled.
  bool should_schedule_delete_and_start_over_ = true;

  // Stores in-flight FindRegistrationForClientUrl callbacks to merge duplicate
  // requests.
  base::flat_map<std::pair<GURL, blink::StorageKey>,
                 std::vector<FindRegistrationCallback>>
      find_registration_callbacks_;

  // ServiceWorker registration scope cache to skip calling
  // FindRegistrationForClientUrl mojo function (https://crbug.com/1411197).
  base::LRUCache<blink::StorageKey, std::set<GURL>> registration_scope_cache_;

  // Live registration's `registration_id` cache to skip calling
  // FindRegistrationForClientUrl mojo function (https://crbug.com/1446216).
  base::LRUCache<std::tuple<GURL, blink::StorageKey>, int64_t>
      registration_id_cache_;

  enum class ConnectionState {
    kNormal,
    kRecovering,
  };
  ConnectionState connection_state_ = ConnectionState::kNormal;
  size_t recovery_retry_counts_ = 0;

  base::flat_set<std::unique_ptr<InflightCall>, base::UniquePtrComparator>
      inflight_calls_;

  base::WeakPtrFactory<ServiceWorkerRegistry> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRY_H_
