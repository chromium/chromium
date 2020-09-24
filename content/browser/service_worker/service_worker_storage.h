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
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/browser/service_worker/service_worker_resource_ops.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {
class QuotaManagerProxy;
}

namespace content {

class ServiceWorkerDiskCache;
class ServiceWorkerResponseMetadataWriter;
class ServiceWorkerResponseWriter;
class ServiceWorkerStorageControlImplTest;

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
FORWARD_DECLARE_TEST(ServiceWorkerStorageTest, DisabledStorage);
}  // namespace service_worker_storage_unittest

// This class provides an interface to store and retrieve ServiceWorker
// registration data. The lifetime is equal to ServiceWorkerRegistry that is
// an owner of this class. When a storage operation fails, this is marked as
// disabled and all subsequent requests are aborted until the registry is
// restarted.
class CONTENT_EXPORT ServiceWorkerStorage {
 public:
  using OriginState = storage::mojom::ServiceWorkerStorageOriginState;

  using RegistrationList =
      std::vector<storage::mojom::ServiceWorkerRegistrationDataPtr>;
  using ResourceList =
      std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>;
  using GetRegisteredOriginsCallback =
      base::OnceCallback<void(const std::vector<url::Origin>& origins)>;
  using FindRegistrationDataCallback = base::OnceCallback<void(
      storage::mojom::ServiceWorkerRegistrationDataPtr data,
      std::unique_ptr<ResourceList> resources,
      ServiceWorkerDatabase::Status status)>;
  using GetRegistrationsDataCallback = base::OnceCallback<void(
      ServiceWorkerDatabase::Status status,
      std::unique_ptr<RegistrationList> registrations,
      std::unique_ptr<std::vector<ResourceList>> resource_lists)>;
  using GetUsageForOriginCallback =
      base::OnceCallback<void(ServiceWorkerDatabase::Status status,
                              int64_t usage)>;
  using GetAllRegistrationsCallback =
      base::OnceCallback<void(ServiceWorkerDatabase::Status status,
                              std::unique_ptr<RegistrationList> registrations)>;
  using StoreRegistrationDataCallback = base::OnceCallback<void(
      ServiceWorkerDatabase::Status status,
      int64_t deleted_version_id,
      const std::vector<int64_t>& newly_purgeable_resources)>;
  using DeleteRegistrationCallback = base::OnceCallback<void(
      ServiceWorkerDatabase::Status status,
      OriginState origin_state,
      int64_t deleted_version_id,
      const std::vector<int64_t>& newly_purgeable_resources)>;

  using ResponseWriterCreationCallback = base::OnceCallback<void(
      int64_t resource_id,
      std::unique_ptr<ServiceWorkerResponseWriter> response_writer)>;

  using DatabaseStatusCallback =
      base::OnceCallback<void(ServiceWorkerDatabase::Status status)>;
  using GetUserDataInDBCallback =
      storage::mojom::ServiceWorkerStorageControl::GetUserDataCallback;
  using GetUserKeysAndDataInDBCallback = storage::mojom::
      ServiceWorkerStorageControl::GetUserKeysAndDataByKeyPrefixCallback;
  using GetUserDataForAllRegistrationsInDBCallback = base::OnceCallback<void(
      ServiceWorkerDatabase::Status,
      std::vector<storage::mojom::ServiceWorkerUserDataPtr>)>;

  ~ServiceWorkerStorage();

  static std::unique_ptr<ServiceWorkerStorage> Create(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      storage::QuotaManagerProxy* quota_manager_proxy);

  // Used for DeleteAndStartOver. Creates new storage based on |old_storage|.
  static std::unique_ptr<ServiceWorkerStorage> Create(
      ServiceWorkerStorage* old_storage);

  // Returns all origins which have service worker registrations.
  void GetRegisteredOrigins(GetRegisteredOriginsCallback callback);

  // Reads stored registrations for |client_url| or |scope| or
  // |registration_id|. Returns ServiceWorkerDatabase::Status::kOk with
  // non-null RegistrationData and ResourceList if registration is found, or
  // returns ServiceWorkerDatabase::Status::kErrorNotFound if no matching
  // registration is found.
  void FindRegistrationForClientUrl(const GURL& client_url,
                                    FindRegistrationDataCallback callback);
  void FindRegistrationForScope(const GURL& scope,
                                FindRegistrationDataCallback callback);
  void FindRegistrationForId(int64_t registration_id,
                             const url::Origin& origin,
                             FindRegistrationDataCallback callback);
  void FindRegistrationForIdOnly(int64_t registration_id,
                                 FindRegistrationDataCallback callback);

  // Returns all stored registrations for a given origin.
  void GetRegistrationsForOrigin(const url::Origin& origin,
                                 GetRegistrationsDataCallback callback);

  // Reads the total resource size stored in the storage for a given origin.
  void GetUsageForOrigin(const url::Origin& origin,
                         GetUsageForOriginCallback callback);

  // Returns all stored registrations.
  void GetAllRegistrations(GetAllRegistrationsCallback callback);

  // Stores |registration_data| and |resources| on persistent storage.
  void StoreRegistrationData(
      storage::mojom::ServiceWorkerRegistrationDataPtr registration_data,
      ResourceList resources,
      StoreRegistrationDataCallback callback);

  // Updates the state of the registration's stored version to active.
  void UpdateToActiveState(int64_t registration_id,
                           const GURL& origin,
                           DatabaseStatusCallback callback);

  // Updates the stored time to match the value of
  // registration->last_update_check().
  void UpdateLastUpdateCheckTime(int64_t registration_id,
                                 const GURL& origin,
                                 base::Time last_update_check_time,
                                 DatabaseStatusCallback callback);

  // Updates the specified registration's navigation preload state in storage.
  // The caller is responsible for mutating the live registration's state.
  void UpdateNavigationPreloadEnabled(int64_t registration_id,
                                      const GURL& origin,
                                      bool enable,
                                      DatabaseStatusCallback callback);
  void UpdateNavigationPreloadHeader(int64_t registration_id,
                                     const GURL& origin,
                                     const std::string& value,
                                     DatabaseStatusCallback callback);

  // Deletes the registration specified by |registration_id|. This should be
  // called only from ServiceWorkerRegistry.
  void DeleteRegistration(int64_t registration_id,
                          const GURL& origin,
                          DeleteRegistrationCallback callback);

  // Removes traces of deleted data on disk.
  void PerformStorageCleanup(base::OnceClosure callback);

  // Creates a resource accessor. Never returns nullptr but an accessor may be
  // associated with the disabled disk cache if the storage is disabled.
  std::unique_ptr<ServiceWorkerResourceReaderImpl> CreateResourceReader(
      int64_t resource_id);
  std::unique_ptr<ServiceWorkerResponseWriter> CreateResponseWriter(
      int64_t resource_id);
  std::unique_ptr<ServiceWorkerResponseMetadataWriter>
  CreateResponseMetadataWriter(int64_t resource_id);

  // Adds |resource_id| to the set of resources that are in the disk cache
  // but not yet stored with a registration.
  void StoreUncommittedResourceId(int64_t resource_id,
                                  const GURL& origin,
                                  DatabaseStatusCallback callback);

  // Removes resource ids from uncommitted list, adds them to the purgeable list
  // and purges them.
  void DoomUncommittedResources(const std::vector<int64_t>& resource_ids,
                                DatabaseStatusCallback callback);

  // Provide a storage mechanism to read/write arbitrary data associated with
  // a registration. Each registration has its own key namespace.
  // GetUserData responds OK only if all keys are found; otherwise NOT_FOUND,
  // and the callback's data will be empty.
  void GetUserData(int64_t registration_id,
                   const std::vector<std::string>& keys,
                   GetUserDataInDBCallback callback);
  // GetUserDataByKeyPrefix responds OK with a vector containing data rows that
  // had matching keys assuming the database was read successfully.
  void GetUserDataByKeyPrefix(int64_t registration_id,
                              const std::string& key_prefix,
                              GetUserDataInDBCallback callback);
  // GetUserKeysAndDataByKeyPrefix responds OK with a flat_map containing
  // matching keys and their data assuming the database was read successfully.
  // The map keys have |key_prefix| stripped from them.
  void GetUserKeysAndDataByKeyPrefix(int64_t registration_id,
                                     const std::string& key_prefix,
                                     GetUserKeysAndDataInDBCallback callback);

  // Stored data is deleted when the associated registraton is deleted.
  void StoreUserData(
      int64_t registration_id,
      const url::Origin& origin,
      std::vector<storage::mojom::ServiceWorkerUserDataPtr> user_data,
      DatabaseStatusCallback callback);
  // Responds OK if all are successfully deleted or not found in the database.
  void ClearUserData(int64_t registration_id,
                     const std::vector<std::string>& keys,
                     DatabaseStatusCallback callback);
  // Responds OK if all are successfully deleted or not found in the database.
  // Neither |key_prefixes| nor the prefixes within can be empty.
  void ClearUserDataByKeyPrefixes(int64_t registration_id,
                                  const std::vector<std::string>& key_prefixes,
                                  DatabaseStatusCallback callback);
  // Responds with all registrations that have user data with a particular key,
  // as well as that user data.
  void GetUserDataForAllRegistrations(
      const std::string& key,
      GetUserDataForAllRegistrationsInDBCallback callback);
  // Responds with all registrations that have user data with a particular key,
  // as well as that user data.
  void GetUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      GetUserDataForAllRegistrationsInDBCallback callback);
  // Responds OK if all are successfully deleted or not found in the database.
  // |key_prefix| cannot be empty.
  void ClearUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      DatabaseStatusCallback callback);

  // Deletes the storage and starts over. This should be called only from
  // ServiceWorkerRegistry other than tests.
  void DeleteAndStartOver(DatabaseStatusCallback callback);

  // Returns a new registration id which is guaranteed to be unique in the
  // storage. Returns blink::mojom::kInvalidServiceWorkerRegistrationId if the
  // storage is disabled.
  void GetNewRegistrationId(
      base::OnceCallback<void(int64_t registration_id)> callback);

  // Returns a new version id which is guaranteed to be unique in the storage.
  // Returns kInvalidServiceWorkerVersionId if the storage is disabled.
  void GetNewVersionId(base::OnceCallback<void(int64_t version_id)> callback);

  // Returns a new resource id which is guaranteed to be unique in the storage.
  // Returns blink::mojom::kInvalidServiceWorkerResourceId if the storage
  // is disabled.
  void GetNewResourceId(base::OnceCallback<void(int64_t resource_id)> callback);

  void Disable();

  // Schedules deleting |resources| from the disk cache and removing their keys
  // as purgeable resources from the service worker database. It's OK to call
  // this for resources that don't have purgeable resource keys, like
  // uncommitted resources, as long as the caller does its own cleanup to remove
  // the uncommitted resource keys.
  void PurgeResources(const ResourceList& resources);
  void PurgeResources(const std::vector<int64_t>& resource_ids);

  // Applies |policy_updates|.
  void ApplyPolicyUpdates(
      const std::vector<storage::mojom::LocalStoragePolicyUpdatePtr>&
          policy_updates);

  void LazyInitializeForTest();

  void SetPurgingCompleteCallbackForTest(base::OnceClosure callback);

 private:
  friend class ServiceWorkerStorageControlImplTest;
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
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_storage_unittest::ServiceWorkerStorageTest,
      DisabledStorage);

  struct InitialData {
    int64_t next_registration_id;
    int64_t next_version_id;
    int64_t next_resource_id;
    std::set<url::Origin> origins;

    InitialData();
    ~InitialData();
  };

  // Because there are too many params for base::Bind to wrap a closure around.
  struct DidDeleteRegistrationParams {
    int64_t registration_id;
    GURL origin;
    DeleteRegistrationCallback callback;

    DidDeleteRegistrationParams(int64_t registration_id,
                                GURL origin,
                                DeleteRegistrationCallback callback);
    ~DidDeleteRegistrationParams();
  };

  using InitializeCallback =
      base::OnceCallback<void(std::unique_ptr<InitialData> data,
                              ServiceWorkerDatabase::Status status)>;
  using WriteRegistrationCallback = base::OnceCallback<void(
      const GURL& origin,
      const ServiceWorkerDatabase::DeletedVersion& deleted_version_data,
      ServiceWorkerDatabase::Status status)>;
  using DeleteRegistrationInDBCallback = base::OnceCallback<void(
      OriginState origin_state,
      const ServiceWorkerDatabase::DeletedVersion& deleted_version_data,
      ServiceWorkerDatabase::Status status)>;
  using FindInDBCallback = base::OnceCallback<void(
      storage::mojom::ServiceWorkerRegistrationDataPtr data,
      std::unique_ptr<ResourceList> resources,
      ServiceWorkerDatabase::Status status)>;
  using GetResourcesCallback =
      base::OnceCallback<void(const std::vector<int64_t>& resource_ids,
                              ServiceWorkerDatabase::Status status)>;

  ServiceWorkerStorage(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      storage::QuotaManagerProxy* quota_manager_proxy);

  base::FilePath GetDatabasePath();
  base::FilePath GetDiskCachePath();

  void LazyInitialize(base::OnceClosure callback);
  void DidReadInitialData(std::unique_ptr<InitialData> data,
                          ServiceWorkerDatabase::Status status);
  void DidGetRegistrationsForOrigin(
      GetRegistrationsDataCallback callback,
      std::unique_ptr<RegistrationList> registrations,
      std::unique_ptr<std::vector<ResourceList>> resource_lists,
      ServiceWorkerDatabase::Status status);
  void DidGetAllRegistrations(
      GetAllRegistrationsCallback callback,
      std::unique_ptr<RegistrationList> registration_data_list,
      ServiceWorkerDatabase::Status status);
  void DidStoreRegistrationData(
      StoreRegistrationDataCallback callback,
      uint64_t new_resources_total_size_bytes,
      const GURL& origin,
      const ServiceWorkerDatabase::DeletedVersion& deleted_version,
      ServiceWorkerDatabase::Status status);
  void DidUpdateToActiveState(DatabaseStatusCallback callback,
                              const GURL& origin,
                              ServiceWorkerDatabase::Status status);
  void DidDeleteRegistration(
      std::unique_ptr<DidDeleteRegistrationParams> params,
      OriginState origin_state,
      const ServiceWorkerDatabase::DeletedVersion& deleted_version,
      ServiceWorkerDatabase::Status status);
  void DidWriteUncommittedResourceIds(DatabaseStatusCallback callback,
                                      const GURL& origin,
                                      ServiceWorkerDatabase::Status status);
  void DidDoomUncommittedResourceIds(const std::vector<int64_t>& resource_ids,
                                     DatabaseStatusCallback callback,
                                     ServiceWorkerDatabase::Status status);
  void DidStoreUserData(DatabaseStatusCallback callback,
                        const url::Origin& origin,
                        ServiceWorkerDatabase::Status status);

  // Lazy disk_cache getter.
  ServiceWorkerDiskCache* disk_cache();
  void InitializeDiskCache();
  void OnDiskCacheInitialized(int rv);

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

  bool IsDisabled() const { return state_ == STORAGE_STATE_DISABLED; }

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
      DeleteRegistrationInDBCallback callback);
  static void WriteRegistrationInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      storage::mojom::ServiceWorkerRegistrationDataPtr registration,
      ResourceList resources,
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
      const url::Origin& origin,
      FindInDBCallback callback);
  static void FindForIdOnlyInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      int64_t registration_id,
      FindInDBCallback callback);
  static void GetUsageForOriginInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      url::Origin origin,
      GetUsageForOriginCallback callback);
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
  static void DeleteAllDataForOriginsFromDB(ServiceWorkerDatabase* database,
                                            const std::set<GURL>& origins);
  static void PerformStorageCleanupInDB(ServiceWorkerDatabase* database);

  // Posted by the underlying cache implementation after it finishes making
  // disk changes upon its destruction.
  void DiskCacheImplDoneWithDisk();
  void DidDeleteDatabase(DatabaseStatusCallback callback,
                         ServiceWorkerDatabase::Status status);
  // Posted when we finish deleting the cache directory.
  void DidDeleteDiskCache(DatabaseStatusCallback callback, bool result);

  // Origins having registations.
  std::set<url::Origin> registered_origins_;
  // The set of origins whose storage should be cleaned on shutdown.
  std::set<GURL> origins_to_purge_on_shutdown_;

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
  DatabaseStatusCallback delete_and_start_over_callback_;

  // This is set when we know that a call to Disable() will result in
  // DiskCacheImplDoneWithDisk() eventually called. This might not happen
  // for many reasons:
  // 1) A previous call to Disable() may have already triggered that.
  // 2) We may be using a memory backend.
  // 3) |disk_cache_| might not have been created yet.
  // ... so it's easier to keep track of the case when it will happen.
  bool expecting_done_with_disk_on_disable_;

  base::FilePath user_data_directory_;

  // |database_| is only accessed using |database_task_runner_|.
  std::unique_ptr<ServiceWorkerDatabase> database_;
  scoped_refptr<base::SequencedTaskRunner> database_task_runner_;

  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

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
