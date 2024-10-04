// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_control_test.mojom.h"
#include "components/services/storage/public/cpp/quota_client_callback_wrapper.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/schemeful_site.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace storage {
struct BucketLocator;
class QuotaClientCallbackWrapper;
}  // namespace storage

namespace content::indexed_db {

// This class manages all the active/open backing stores for IndexedDB, of which
// there is at most one per bucket. It also serves as the central liaison to
// other Chromium components such as the quota manager. It runs on its own
// thread, so almost all interaction is declared via a mojo interface.
class CONTENT_EXPORT IndexedDBContextImpl
    : public storage::mojom::IndexedDBControl,
      public storage::mojom::IndexedDBControlTest,
      public storage::mojom::QuotaClient {
 public:
  // If `base_data_path` is empty, nothing will be saved to disk.
  // This is *not* called on the IDBTaskRunner, unlike most other functions.
  IndexedDBContextImpl(
      const base::FilePath& base_data_path,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      mojo::PendingRemote<storage::mojom::BlobStorageContext>
          blob_storage_context,
      mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
          file_system_access_context,
      scoped_refptr<base::SequencedTaskRunner> custom_task_runner);

  ~IndexedDBContextImpl() override;

  // Called to initiate shutdown. This is *not* called on the IDBTaskRunner.
  static void Shutdown(std::unique_ptr<IndexedDBContextImpl> context);

  IndexedDBContextImpl(const IndexedDBContextImpl&) = delete;
  IndexedDBContextImpl& operator=(const IndexedDBContextImpl&) = delete;

  void BindControl(
      mojo::PendingReceiver<storage::mojom::IndexedDBControl> control);

  // mojom::IndexedDBControl implementation:
  void BindIndexedDB(
      const storage::BucketLocator& bucket_locator,
      const storage::BucketClientInfo& client_info,
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
          client_state_checker_remote,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void ForceClose(storage::BucketId bucket_id,
                  storage::mojom::ForceCloseReason reason,
                  base::OnceClosure callback) override;
  void StartMetadataRecording(storage::BucketId bucket_id,
                              StartMetadataRecordingCallback callback) override;
  void StopMetadataRecording(storage::BucketId bucket_id,
                             StopMetadataRecordingCallback callback) override;

  void DownloadBucketData(storage::BucketId bucket_id,
                          DownloadBucketDataCallback callback) override;
  void GetAllBucketsDetails(GetAllBucketsDetailsCallback callback) override;
  void SetForceKeepSessionState() override;
  void ApplyPolicyUpdates(std::vector<storage::mojom::StoragePolicyUpdatePtr>
                              policy_updates) override;
  void BindTestInterface(
      mojo::PendingReceiver<storage::mojom::IndexedDBControlTest> receiver)
      override;
  void AddObserver(
      mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) override;

  // mojom::IndexedDBControlTest implementation:
  void GetBaseDataPathForTesting(
      GetBaseDataPathForTestingCallback callback) override;
  void GetFilePathForTesting(const storage::BucketLocator& bucket_locator,
                             GetFilePathForTestingCallback callback) override;
  void ResetCachesForTesting(base::OnceClosure callback) override;
  void WriteToIndexedDBForTesting(const storage::BucketLocator& bucket_locator,
                                  const std::string& key,
                                  const std::string& value,
                                  base::OnceClosure callback) override;
  void GetPathForBlobForTesting(
      const storage::BucketLocator& bucket_locator,
      int64_t database_id,
      int64_t blob_number,
      GetPathForBlobForTestingCallback callback) override;
  void CompactBackingStoreForTesting(
      const storage::BucketLocator& bucket_locator,
      base::OnceClosure callback) override;
  void GetUsageForTesting(GetUsageForTestingCallback) override;
  void BindMockFailureSingletonForTesting(
      mojo::PendingReceiver<storage::mojom::MockFailureInjector> receiver)
      override;
  void GetDatabaseKeysForTesting(
      GetDatabaseKeysForTestingCallback callback) override;
  void ForceInitializeFromFilesForTesting(
      ForceInitializeFromFilesForTestingCallback callback) override;

  // storage::mojom::QuotaClient implementation:
  void GetBucketUsage(const storage::BucketLocator& bucket,
                      GetBucketUsageCallback callback) override;
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysForTypeCallback callback) override;
  void DeleteBucketData(const storage::BucketLocator& bucket,
                        DeleteBucketDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

  // Exposed for testing.
  bool BucketContextExists(storage::BucketId bucket_id);

  // Exposed for testing.
  const scoped_refptr<base::SequencedTaskRunner>& IDBTaskRunner() const {
    return idb_task_runner_;
  }

  const base::FilePath GetFirstPartyDataPathForTesting() const;
  base::SequenceBound<BucketContext>* GetBucketContextForTesting(
      const storage::BucketId& id);

 private:
  friend class IndexedDBTest;
  friend class FactoryTest;
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, BasicFactoryCreationAndTearDown);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, TooLongOrigin);

  void BindControlOnIDBSequence(
      mojo::PendingReceiver<storage::mojom::IndexedDBControl> control);

  void BindPipesOnIDBSequence(
      mojo::PendingReceiver<storage::mojom::QuotaClient>
          pending_quota_client_receiver,
      mojo::PendingRemote<storage::mojom::BlobStorageContext>
          pending_blob_storage_context,
      mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
          pending_file_system_access_context);

  // mojom::IndexedDBControl internal implementation:
  void BindIndexedDBImpl(
      const storage::BucketClientInfo& client_info,
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
          client_state_checker_remote,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
      storage::QuotaErrorOr<storage::BucketInfo> bucket_info);
  void ForceCloseImpl(
      const storage::mojom::ForceCloseReason reason,
      base::OnceClosure closure,
      const std::optional<storage::BucketLocator>& bucket_locator);

  // Always run immediately before destruction.
  void ShutdownOnIDBSequence(base::TimeTicks start_time);

  base::FilePath GetDataPath(
      const storage::BucketLocator& bucket_locator) const;
  const base::FilePath GetLegacyDataPath() const;
  base::FilePath GetBlobStorePath(
      const storage::BucketLocator& bucket_locator) const;
  base::FilePath GetLevelDBPath(
      const storage::BucketLocator& bucket_locator) const;

  int64_t ReadUsageFromDisk(const storage::BucketLocator& bucket_locator,
                            bool write_in_progress) const;
  void NotifyOfBucketModification(const storage::BucketLocator& bucket_locator);
  base::Time GetBucketLastModified(
      const storage::BucketLocator& bucket_locator);

  // We need to initialize the buckets already stored to the disk, but this
  // cannot be done in the constructor as it might block destruction.
  void InitializeFromFilesIfNeeded(base::OnceClosure callback);
  bool did_initialize_from_files_{false};
  std::vector<base::OnceClosure> on_initialize_from_files_callbacks_;

  using DidGetBucketLocatorCallback = base::OnceCallback<void(
      const std::optional<storage::BucketLocator>& bucket_locator)>;

  void GetOrCreateDefaultBucket(const blink::StorageKey& storage_key,
                                DidGetBucketLocatorCallback callback);

  // Finds IDB files in their legacy location, which is currently used for
  // default buckets in first party contexts. Non-default buckets and default
  // buckets in third party contexts, when partitioning is enabled, are returned
  // by `FindIndexedDBFiles`.
  std::map<blink::StorageKey, base::FilePath> FindLegacyIndexedDBFiles() const;

  // Reads IDB files from disk, looking in the directories where
  // third-party-context IDB files are stored.
  std::map<storage::BucketId, base::FilePath> FindIndexedDBFiles() const;

  void DidForceCloseForDeleteBucketData(
      const storage::BucketLocator& bucket_locator,
      DeleteBucketDataCallback callback);

  // Invoked after asynchronously retrieving buckets from the quota manager in
  // service of `GetAllBucketsDetails()`.
  void ContinueGetAllBucketsDetails(
      GetAllBucketsDetailsCallback callback,
      std::vector<storage::QuotaErrorOr<storage::BucketInfo>> bucket_infos);

  // Applies the given `callback` to all bucket contexts.
  void ForEachBucketContext(BucketContext::InstanceClosure callback);

  // Calculates in-memory/incognito usage for usage reporting.
  void GetInMemorySize(storage::BucketId bucket_id,
                       base::OnceCallback<void(int64_t)> on_got_size) const;

  std::vector<storage::BucketId> GetOpenBucketIdsForTesting() const;

  // Finishes filling in `info` with data relevant to idb-internals and passes
  // the result back via `result`. The bucket is described by
  // `info->bucket_locator`.
  void FillInBucketMetadata(
      storage::mojom::IdbBucketMetadataPtr info,
      base::OnceCallback<void(storage::mojom::IdbBucketMetadataPtr)> result);

  void EnsureBucketContext(const storage::BucketInfo& bucket,
                           const base::FilePath& data_directory);

  void CompactBackingStoreForTesting(
      const storage::BucketLocator& bucket_locator);

  int64_t GetBucketDiskUsage(const storage::BucketLocator& bucket_locator);

  // Returns all paths owned by this database, in arbitrary order.
  std::vector<base::FilePath> GetStoragePaths(
      const storage::BucketLocator& bucket_locator) const;

  // Called when files for the given bucket have been written. `flushed` is set
  // to true if the writes were flushed to disk already, as with a transaction
  // that has strict durability.
  void OnFilesWritten(const storage::BucketLocator& bucket_locator,
                      bool flushed);

  void NotifyIndexedDBContentChanged(
      const storage::BucketLocator& bucket_locator,
      const std::u16string& database_name,
      const std::u16string& object_store_name);

  void DestroyBucketContext(storage::BucketLocator bucket_locator);

  std::optional<storage::BucketLocator> LookUpBucket(
      storage::BucketId bucket_id);

  bool in_memory() const { return base_data_path_.empty(); }

  const scoped_refptr<base::SequencedTaskRunner> idb_task_runner_;

  // Bound and accessed on the `idb_task_runner_`.
  mojo::Remote<storage::mojom::BlobStorageContext> blob_storage_context_;
  mojo::Remote<storage::mojom::FileSystemAccessContext>
      file_system_access_context_;

  // If `base_data_path_` is empty then this is an incognito session and the
  // backing store will be held in-memory rather than on-disk.
  const base::FilePath base_data_path_;

  // If true, nothing (not even session-only data) should be deleted on exit.
  bool force_keep_session_state_ = false;
  // Will be null in unit tests.
  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // This set contains all buckets that have stored IDB data on disk. This is
  // distinct from `bucket_contexts_`, which are only created for active IDB
  // clients.
  std::set<storage::BucketLocator> bucket_set_;

  // This map is a cache of the size used by a given bucket. It's calculated by
  // summing the system-reported sizes of all blob and LevelDB files. This cache
  // is cleared after transactions that can change the size of the database
  // (i.e. those that are not readonly), and re-populated lazily. There are
  // three possible states for each bucket in this map:
  //
  // 1) Not present. This indicates that the `ReadUsageFromDisk()` should be
  //    called to calculate usage (and be stored in the map).
  // 2) Present, with a non-negative value. This indicates that the cache is, as
  //    far as we know, valid and up to date for the given bucket. This state
  //    persists until the next writing transaction occurs.
  // 3) Present, with a negative value. This indicates that the usage is not
  //    cached AND the last readwrite transaction did NOT flush changes to disk
  //    (i.e. durability was 'relaxed').
  //
  // On POSIX, the first and third states are treated equivalently. However, on
  // Windows, `base::FileEnumerator` (and therefore
  // `base::ComputeDirectorySize()`) will not report up-to-date sizes when a
  // file is currently being written. When transactions are not set to
  // "flush"/"sync" (terminology varies based on context), LevelDB will keep
  // open its file handles. Therefore, on Windows, `ReadUsageFromDisk()` may
  // not take into account recent writes, leading to situations where
  // `navigator.storage.estimate()` will not report updates when interleaved
  // with relaxed durability IDB transactions. The workaround for this is to
  // open and close new file handles for all the files in the LevelDB data
  // directory before calculating usage, as this updates the file system
  // directory entry's metadata. See crbug.com/1489517 and
  // https://devblogs.microsoft.com/oldnewthing/20111226-00/?p=8813
  //
  // TODO(crbug.com/40285925): use an abstract model for quota instead of real
  // world bytes.
  std::map<storage::BucketLocator, int64_t> bucket_size_map_;

  // The set of sites whose storage should be cleared on shutdown. These are
  // matched against the origin and top level site in each bucket's StorageKey.
  std::set<url::Origin> origins_to_purge_on_shutdown_;

  storage::QuotaClientCallbackWrapper quota_client_wrapper_{this};
  mojo::Receiver<storage::mojom::QuotaClient> quota_client_receiver_;

  mojo::ReceiverSet<storage::mojom::IndexedDBControl> control_receivers_;
  mojo::ReceiverSet<storage::mojom::IndexedDBControlTest> test_receivers_;
  // See comment above IDBFactory overrides.
  mojo::ReceiverSet<blink::mojom::IDBFactory> factory_receivers_;
  std::optional<mojo::Receiver<storage::mojom::MockFailureInjector>>
      mock_failure_injector_;
  mojo::RemoteSet<storage::mojom::IndexedDBObserver> observers_;

  // For testing: when non-null, this receiver will be passed off to the next
  // bucket context that's created.
  mojo::PendingReceiver<storage::mojom::MockFailureInjector>
      pending_failure_injector_;

  std::map<storage::BucketId, base::SequenceBound<BucketContext>>
      bucket_contexts_;

  // For the most part, every bucket gets its own SequencedTaskRunner. But each
  // "site", i.e. StorageKey's `top_level_site()`, has a cap on the number of
  // task runners its buckets will be allotted, which is equal to the number of
  // cores on the device. When creating a new BucketContext, it will get a
  // unique task runner that runs on the threadpool unless `active_bucket_count`
  // is over the number of cores, in which case the task runner will be shared
  // with other buckets.
  struct TaskRunnerLimiter {
    TaskRunnerLimiter();
    ~TaskRunnerLimiter();

    int active_bucket_count = 0;
    scoped_refptr<base::SequencedTaskRunner> overflow_task_runner;
  };
  std::map<net::SchemefulSite, TaskRunnerLimiter> task_runner_limiters_;

  BucketContext::InstanceClosure for_each_bucket_context_;

  // When true, run backing stores (and bucket contexts) on `idb_task_runner_`
  // to simplify unit tests. This is set to true when the ctor param
  // `custom_task_runner` is non null.
  bool force_single_thread_ = false;

  // If recording begins on a bucket ID that doesn't currently have a context,
  // add it to a pending set and actually begin once the context is created.
  std::set<storage::BucketId> pending_bucket_recording_;
  std::vector<storage::mojom::IdbBucketMetadataPtr> metadata_record_buffer_;
  // When `Shutdown()` was called, or null if it's not been called. Used for
  // UMA.
  base::TimeTicks shutdown_start_time_;

  // weak_factory_->GetWeakPtr() may be used on any thread, but the resulting
  // pointer must only be checked/used on idb_task_runner_.
  base::WeakPtrFactory<IndexedDBContextImpl> weak_factory_{this};
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_
