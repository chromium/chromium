// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerDiskCache;
class ServiceWorkerResponseMetadataWriter;
class ServiceWorkerResponseReader;
class ServiceWorkerResponseWriter;
class ServiceWorkerVersion;
struct ServiceWorkerRegistrationInfo;

namespace service_worker_storage_unittest {
class ServiceWorkerStorageTest;
class ServiceWorkerResourceStorageTest;
class ServiceWorkerResourceStorageDiskTest;
FORWARD_DECLARE_TEST(ServiceWorkerResourceStorageDiskTest, CleanupOnRestart);
FORWARD_DECLARE_TEST(ServiceWorkerResourceStorageDiskTest, DeleteAndStartOver);
FORWARD_DECLARE_TEST(ServiceWorkerResourceStorageDiskTest,
                     DeleteAndStartOver_UnrelatedFileExists);
FORWARD_DECLARE_TEST(ServiceWorkerResourceStorageDiskTest,
                     DeleteAndStartOver_OpenedFileExists);
}  // namespace service_worker_storage_unittest

// This class provides an interface to store and retrieve ServiceWorker
// registration data. The lifetime is equal to ServiceWorkerContextCore that is
// an owner of this class. When a storage operation fails, this is marked as
// disabled and all subsequent requests are aborted until the context core is
// restarted.
class CONTENT_EXPORT ServiceWorkerStorage {
 public:
  using ResourceList = std::vector<ServiceWorkerDatabase::ResourceRecord>;
  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status)>;
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
      const base::flat_map<std::string, std::string>& data_map,
      blink::ServiceWorkerStatusCode status)>;
  using GetUserDataForAllRegistrationsCallback = base::OnceCallback<void(
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      blink::ServiceWorkerStatusCode status)>;

  ~ServiceWorkerStorage();

  static std::unique_ptr<ServiceWorkerStorage> Create(
      const base::FilePath& user_data_directory,
      ServiceWorkerContextCore* context,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy);

  // Used for DeleteAndStartOver. Creates new storage based on |old_storage|.
  static std::unique_ptr<ServiceWorkerStorage> Create(
      ServiceWorkerContextCore* context,
      ServiceWorkerStorage* old_storage);

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
  void FindRegistrationForId(int64_t registration_id,
                             const GURL& origin,
                             FindRegistrationCallback callback);

  // Generally |FindRegistrationForId| should be used to look up a registration
  // by |registration_id| since it's more efficient. But if a |registration_id|
  // is all that is available this method can be used instead.
  // Like |FindRegistrationForId| this method may complete immediately (the
  // callback may be called prior to the method returning) or asynchronously.
  void FindRegistrationForIdOnly(int64_t registration_id,
                                 FindRegistrationCallback callback);

  ServiceWorkerRegistration* GetUninstallingRegistration(const GURL& scope);

  // Returns all stored registrations for a given origin.
  void GetRegistrationsForOrigin(const GURL& origin,
                                 GetRegistrationsCallback callback);

  // Returns info about all stored and initially installing registrations.
  void GetAllRegistrationsInfos(GetRegistrationsInfosCallback callback);

  // Commits |registration| with the installed but not activated |version|
  // to storage, overwritting any pre-existing registration data for the scope.
  // A pre-existing version's script resources remain available if that version
  // is live. PurgeResources should be called when it's OK to delete them.
  void StoreRegistration(ServiceWorkerRegistration* registration,
                         ServiceWorkerVersion* version,
                         StatusCallback callback);

  // Updates the state of the registration's stored version to active.
  void UpdateToActiveState(ServiceWorkerRegistration* registration,
                           StatusCallback callback);

  // Updates the stored time to match the value of
  // registration->last_update_check().
  void UpdateLastUpdateCheckTime(ServiceWorkerRegistration* registration,
                                 StatusCallback callback);

  // Updates the specified registration's navigation preload state in storage.
  // The caller is responsible for mutating the live registration's state.
  void UpdateNavigationPreloadEnabled(int64_t registration_id,
                                      const GURL& origin,
                                      bool enable,
                                      StatusCallback callback);
  void UpdateNavigationPreloadHeader(int64_t registration_id,
                                     const GURL& origin,
                                     const std::string& value,
                                     StatusCallback callback);

  // Deletes the registration data for |registration|. The live registration is
  // still findable via GetUninstallingRegistration(), and versions are usable
  // because their script resources have not been deleted. After calling this,
  // the caller should later:
  // - Call NotifyDoneUninstallingRegistration() to let storage know the
  //   uninstalling operation is done.
  // - If it no longer wants versions to be usable, call PurgeResources() to
  //   delete their script resources.
  // If these aren't called, on the next profile session the cleanup occurs.
  void DeleteRegistration(scoped_refptr<ServiceWorkerRegistration> registration,
                          const GURL& origin,
                          StatusCallback callback);

  // Removes traces of deleted data on disk.
  void PerformStorageCleanup(base::OnceClosure callback);

  // Creates a resource accessor. Never returns nullptr but an accessor may be
  // associated with the disabled disk cache if the storage is disabled.
  std::unique_ptr<ServiceWorkerResponseReader> CreateResponseReader(
      int64_t resource_id);
  std::unique_ptr<ServiceWorkerResponseWriter> CreateResponseWriter(
      int64_t resource_id);
  std::unique_ptr<ServiceWorkerResponseMetadataWriter>
  CreateResponseMetadataWriter(int64_t resource_id);

  // Adds |resource_id| to the set of resources that are in the disk cache
  // but not yet stored with a registration.
  void StoreUncommittedResourceId(int64_t resource_id);

  // Removes resource ids from uncommitted list, adds them to the purgeable list
  // and purges them.
  void DoomUncommittedResource(int64_t resource_id);
  void DoomUncommittedResources(const std::set<int64_t>& resource_ids);

  // Provide a storage mechanism to read/write arbitrary data associated with
  // a registration. Each registration has its own key namespace.
  // GetUserData responds OK only if all keys are found; otherwise NOT_FOUND,
  // and the callback's data will be empty.
  void GetUserData(int64_t registration_id,
                   const std::vector<std::string>& keys,
                   GetUserDataCallback callback);
  // GetUserDataByKeyPrefix responds OK with a vector containing data rows that
  // had matching keys assuming the database was read successfully.
  void GetUserDataByKeyPrefix(int64_t registration_id,
                              const std::string& key_prefix,
                              GetUserDataCallback callback);
  // GetUserKeysAndDataByKeyPrefix responds OK with a flat_map containing
  // matching keys and their data assuming the database was read successfully.
  // The map keys have |key_prefix| stripped from them.
  void GetUserKeysAndDataByKeyPrefix(int64_t registration_id,
                                     const std::string& key_prefix,
                                     GetUserKeysAndDataCallback callback);

  // Stored data is deleted when the associated registraton is deleted.
  void StoreUserData(
      int64_t registration_id,
      const GURL& origin,
      const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
      StatusCallback callback);
  // Responds OK if all are successfully deleted or not found in the database.
  void ClearUserData(int64_t registration_id,
                     const std::vector<std::string>& keys,
                     StatusCallback callback);
  // Responds OK if all are successfully deleted or not found in the database.
  // Neither |key_prefixes| nor the prefixes within can be empty.
  void ClearUserDataByKeyPrefixes(int64_t registration_id,
                                  const std::vector<std::string>& key_prefixes,
                                  StatusCallback callback);
  // Responds with all registrations that have user data with a particular key,
  // as well as that user data.
  void GetUserDataForAllRegistrations(
      const std::string& key,
      GetUserDataForAllRegistrationsCallback callback);
  // Responds with all registrations that have user data with a particular key,
  // as well as that user data.
  void GetUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      GetUserDataForAllRegistrationsCallback callback);
  // Responds OK if all are successfully deleted or not found in the database.
  // |key_prefix| cannot be empty.
  void ClearUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      StatusCallback callback);

  // Deletes the storage and starts over.
  void DeleteAndStartOver(StatusCallback callback);

  // Returns a new registration id which is guaranteed to be unique in the
  // storage. Returns blink::mojom::kInvalidServiceWorkerRegistrationId if the
  // storage is disabled.
  int64_t NewRegistrationId();

  // Returns a new version id which is guaranteed to be unique in the storage.
  // Returns kInvalidServiceWorkerVersionId if the storage is disabled.
  int64_t NewVersionId();

  // Returns a new resource id which is guaranteed to be unique in the storage.
  // Returns ServiceWorkerConsts::kInvalidServiceWorkerResourceId if the storage
  // is disabled.
  int64_t NewResourceId();

  // Intended for use only by ServiceWorkerRegisterJob and
  // ServiceWorkerRegistration.
  void NotifyInstallingRegistration(
      ServiceWorkerRegistration* registration);
  void NotifyDoneInstallingRegistration(ServiceWorkerRegistration* registration,
                                        ServiceWorkerVersion* version,
                                        blink::ServiceWorkerStatusCode status);
  void NotifyDoneUninstallingRegistration(
      ServiceWorkerRegistration* registration,
      ServiceWorkerRegistration::Status new_status);

  void Disable();

  // Schedules deleting |resources| from the disk cache and removing their keys
  // as purgeable resources from the service worker database. It's OK to call
  // this for resources that don't have purgeable resource keys, like
  // uncommitted resources, as long as the caller does its own cleanup to remove
  // the uncommitted resource keys.
  void PurgeResources(const ResourceList& resources);

  void LazyInitializeForTest();

  void SetPurgingCompleteCallbackForTest(base::OnceClosure callback);

 private:
  friend class service_worker_storage_unittest::ServiceWorkerStorageTest;
  friend class service_worker_storage_unittest::
      ServiceWorkerResourceStorageTest;
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_storage_unittest::ServiceWorkerResourceStorageDiskTest,
      CleanupOnRestart);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_storage_unittest::ServiceWorkerResourceStorageDiskTest,
      DeleteAndStartOver);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_storage_unittest::ServiceWorkerResourceStorageDiskTest,
      DeleteAndStartOver_UnrelatedFileExists);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_storage_unittest::ServiceWorkerResourceStorageDiskTest,
      DeleteAndStartOver_OpenedFileExists);

  struct InitialData {
    int64_t next_registration_id;
    int64_t next_version_id;
    int64_t next_resource_id;
    std::set<GURL> origins;

    InitialData();
    ~InitialData();
  };

  // Because there are too many params for base::Bind to wrap a closure around.
  struct DidDeleteRegistrationParams {
    int64_t registration_id;
    GURL origin;
    StatusCallback callback;

    DidDeleteRegistrationParams(int64_t registration_id,
                                GURL origin,
                                StatusCallback callback);
    ~DidDeleteRegistrationParams();
  };

  enum class OriginState {
    // Registrations may exist at this origin. It cannot be deleted.
    kKeep,
    // No registrations exist at this origin. It can be deleted.
    kDelete
  };

  using RegistrationList = std::vector<ServiceWorkerDatabase::RegistrationData>;
  using RegistrationRefsById =
      std::map<int64_t, scoped_refptr<ServiceWorkerRegistration>>;
  using InitializeCallback =
      base::OnceCallback<void(std::unique_ptr<InitialData> data,
                              ServiceWorkerDatabase::Status status)>;
  using WriteRegistrationCallback = base::OnceCallback<void(
      const GURL& origin,
      const ServiceWorkerDatabase::RegistrationData& deleted_version_data,
      const std::vector<int64_t>& newly_purgeable_resources,
      ServiceWorkerDatabase::Status status)>;
  using DeleteRegistrationCallback = base::OnceCallback<void(
      OriginState origin_state,
      const ServiceWorkerDatabase::RegistrationData& deleted_version_data,
      const std::vector<int64_t>& newly_purgeable_resources,
      ServiceWorkerDatabase::Status status)>;
  using FindInDBCallback = base::OnceCallback<void(
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources,
      ServiceWorkerDatabase::Status status)>;
  using GetUserKeysAndDataInDBCallback = base::OnceCallback<void(
      const base::flat_map<std::string, std::string>& data_map,
      ServiceWorkerDatabase::Status)>;
  using GetUserDataInDBCallback =
      base::OnceCallback<void(const std::vector<std::string>& data,
                              ServiceWorkerDatabase::Status)>;
  using GetUserDataForAllRegistrationsInDBCallback = base::OnceCallback<void(
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      ServiceWorkerDatabase::Status)>;
  using GetResourcesCallback =
      base::OnceCallback<void(const std::vector<int64_t>& resource_ids,
                              ServiceWorkerDatabase::Status status)>;

  ServiceWorkerStorage(
      const base::FilePath& user_data_directory,
      ServiceWorkerContextCore* context,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy);

  base::FilePath GetDatabasePath();
  base::FilePath GetDiskCachePath();

  void LazyInitialize(base::OnceClosure callback);
  void DidReadInitialData(std::unique_ptr<InitialData> data,
                          ServiceWorkerDatabase::Status status);
  void DidFindRegistrationForClientUrl(
      const GURL& client_url,
      FindRegistrationCallback callback,
      int64_t callback_id,
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources,
      ServiceWorkerDatabase::Status status);
  void DidFindRegistrationForScope(
      const GURL& scope,
      FindRegistrationCallback callback,
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources,
      ServiceWorkerDatabase::Status status);
  void DidFindRegistrationForId(
      FindRegistrationCallback callback,
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources,
      ServiceWorkerDatabase::Status status);
  void DidGetRegistrationsForOrigin(GetRegistrationsCallback callback,
                                    RegistrationList* registration_data_list,
                                    std::vector<ResourceList>* resources_list,
                                    const GURL& origin_filter,
                                    ServiceWorkerDatabase::Status status);
  void DidGetAllRegistrationsInfos(GetRegistrationsInfosCallback callback,
                                   RegistrationList* registration_data_list,
                                   ServiceWorkerDatabase::Status status);
  void DidStoreRegistration(
      StatusCallback callback,
      const ServiceWorkerDatabase::RegistrationData& new_version,
      const GURL& origin,
      const ServiceWorkerDatabase::RegistrationData& deleted_version,
      const std::vector<int64_t>& newly_purgeable_resources,
      ServiceWorkerDatabase::Status status);
  void DidUpdateToActiveState(StatusCallback callback,
                              ServiceWorkerDatabase::Status status);
  void DidDeleteRegistration(
      std::unique_ptr<DidDeleteRegistrationParams> params,
      OriginState origin_state,
      const ServiceWorkerDatabase::RegistrationData& deleted_version,
      const std::vector<int64_t>& newly_purgeable_resources,
      ServiceWorkerDatabase::Status status);
  void DidWriteUncommittedResourceIds(ServiceWorkerDatabase::Status status);
  void DidPurgeUncommittedResourceIds(const std::set<int64_t>& resource_ids,
                                      ServiceWorkerDatabase::Status status);
  void DidStoreUserData(StatusCallback callback,
                        ServiceWorkerDatabase::Status status);
  void DidGetUserData(GetUserDataCallback callback,
                      const std::vector<std::string>& data,
                      ServiceWorkerDatabase::Status status);
  void DidGetUserKeysAndData(
      GetUserKeysAndDataCallback callback,
      const base::flat_map<std::string, std::string>& map,
      ServiceWorkerDatabase::Status status);
  void DidDeleteUserData(StatusCallback callback,
                         ServiceWorkerDatabase::Status status);
  void DidGetUserDataForAllRegistrations(
      GetUserDataForAllRegistrationsCallback callback,
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      ServiceWorkerDatabase::Status status);
  void ReturnFoundRegistration(
      FindRegistrationCallback callback,
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources);

  scoped_refptr<ServiceWorkerRegistration> GetOrCreateRegistration(
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources);
  ServiceWorkerRegistration* FindInstallingRegistrationForClientUrl(
      const GURL& client_url);
  ServiceWorkerRegistration* FindInstallingRegistrationForScope(
      const GURL& scope);
  ServiceWorkerRegistration* FindInstallingRegistrationForId(
      int64_t registration_id);

  // Lazy disk_cache getter.
  ServiceWorkerDiskCache* disk_cache();
  void InitializeDiskCache();
  void OnDiskCacheInitialized(int rv);

  void StartPurgingResources(const std::set<int64_t>& resource_ids);
  void StartPurgingResources(const std::vector<int64_t>& resource_ids);
  void StartPurgingResources(const ResourceList& resources);
  void ContinuePurgingResources();
  void PurgeResource(int64_t id);
  void OnResourcePurged(int64_t id, int rv);

  // Deletes purgeable and uncommitted resources left over from the previous
  // browser session. This must be called once per session before any database
  // operation that may mutate the purgeable or uncommitted resource lists.
  void DeleteStaleResources();
  void DidCollectStaleResources(const std::vector<int64_t>& stale_resource_ids,
                                ServiceWorkerDatabase::Status status);

  void ClearSessionOnlyOrigins();

  // Static cross-thread helpers.
  static void CollectStaleResourcesFromDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      GetResourcesCallback callback);
  static void ReadInitialDataFromDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      InitializeCallback callback);
  static void DeleteRegistrationFromDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      int64_t registration_id,
      const GURL& origin,
      DeleteRegistrationCallback callback);
  static void WriteRegistrationInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      const ServiceWorkerDatabase::RegistrationData& registration,
      const ResourceList& resources,
      WriteRegistrationCallback callback);
  static void FindForClientUrlInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      const GURL& client_url,
      FindInDBCallback callback);
  static void FindForScopeInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      const GURL& scope,
      FindInDBCallback callback);
  static void FindForIdInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      int64_t registration_id,
      const GURL& origin,
      FindInDBCallback callback);
  static void FindForIdOnlyInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      int64_t registration_id,
      FindInDBCallback callback);
  static void GetUserDataInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      int64_t registration_id,
      const std::vector<std::string>& keys,
      GetUserDataInDBCallback callback);
  static void GetUserDataByKeyPrefixInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      int64_t registration_id,
      const std::string& key_prefix,
      GetUserDataInDBCallback callback);
  static void GetUserKeysAndDataByKeyPrefixInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      int64_t registration_id,
      const std::string& key_prefix,
      GetUserKeysAndDataInDBCallback callback);
  static void GetUserDataForAllRegistrationsInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      const std::string& key,
      GetUserDataForAllRegistrationsInDBCallback callback);
  static void GetUserDataForAllRegistrationsByKeyPrefixInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      const std::string& key_prefix,
      GetUserDataForAllRegistrationsInDBCallback callback);
  static void DeleteAllDataForOriginsFromDB(
      ServiceWorkerDatabase* database,
      const std::set<GURL>& origins);
  static void PerformStorageCleanupInDB(ServiceWorkerDatabase* database);

  bool IsDisabled() const;
  void ScheduleDeleteAndStartOver();

  // Posted by the underlying cache implementation after it finishes making
  // disk changes upon its destruction.
  void DiskCacheImplDoneWithDisk();
  void DidDeleteDatabase(StatusCallback callback,
                         ServiceWorkerDatabase::Status status);
  // Posted when we finish deleting the cache directory.
  void DidDeleteDiskCache(StatusCallback callback, bool result);

  // For finding registrations being installed or uninstalled.
  RegistrationRefsById installing_registrations_;
  RegistrationRefsById uninstalling_registrations_;

  // Origins having registations.
  std::set<GURL> registered_origins_;

  // Pending database tasks waiting for initialization.
  std::vector<base::OnceClosure> pending_tasks_;

  int64_t next_registration_id_;
  int64_t next_version_id_;
  int64_t next_resource_id_;

  enum State {
    STORAGE_STATE_UNINITIALIZED,
    STORAGE_STATE_INITIALIZING,
    STORAGE_STATE_INITIALIZED,
    STORAGE_STATE_DISABLED,
  };
  State state_;

  // non-null between when DeleteAndStartOver() is called and when the
  // underlying disk cache stops using the disk.
  StatusCallback delete_and_start_over_callback_;

  // This is set when we know that a call to Disable() will result in
  // DiskCacheImplDoneWithDisk() eventually called. This might not happen
  // for many reasons:
  // 1) A previous call to Disable() may have already triggered that.
  // 2) We may be using a memory backend.
  // 3) |disk_cache_| might not have been created yet.
  // ... so it's easier to keep track of the case when it will happen.
  bool expecting_done_with_disk_on_disable_;

  base::FilePath user_data_directory_;

  // The ServiceWorkerContextCore object must outlive this.
  ServiceWorkerContextCore* const context_;

  // |database_| is only accessed using |database_task_runner_|.
  std::unique_ptr<ServiceWorkerDatabase> database_;
  scoped_refptr<base::SequencedTaskRunner> database_task_runner_;

  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  std::unique_ptr<ServiceWorkerDiskCache> disk_cache_;

  base::circular_deque<int64_t> purgeable_resource_ids_;
  bool is_purge_pending_;
  bool has_checked_for_stale_resources_;
  base::OnceClosure purging_complete_callback_for_test_;

  base::WeakPtrFactory<ServiceWorkerStorage> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
