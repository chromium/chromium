// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_factory.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom-forward.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom-forward.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace content {
class IndexedDBBucketState;
class IndexedDBBucketStateHandle;
class IndexedDBClassFactory;
class IndexedDBContextImpl;
class TransactionalLevelDBFactory;
class TransactionalLevelDBDatabase;

class CONTENT_EXPORT IndexedDBFactory : base::trace_event::MemoryDumpProvider {
 public:
  IndexedDBFactory(IndexedDBContextImpl* context,
                   IndexedDBClassFactory* indexed_db_class_factory,
                   base::Clock* clock);

  IndexedDBFactory(const IndexedDBFactory&) = delete;
  IndexedDBFactory& operator=(const IndexedDBFactory&) = delete;

  ~IndexedDBFactory() override;

  void GetDatabaseInfo(
      const storage::BucketLocator& bucket_locator,
      const base::FilePath& data_directory,
      blink::mojom::IDBFactory::GetDatabaseInfoCallback callback);
  void Open(const std::u16string& name,
            std::unique_ptr<IndexedDBPendingConnection> connection,
            const storage::BucketLocator& bucket_locator,
            const base::FilePath& data_directory,
            scoped_refptr<IndexedDBClientStateCheckerWrapper>
                client_state_checker);

  void DeleteDatabase(const std::u16string& name,
                      scoped_refptr<IndexedDBCallbacks> callbacks,
                      const storage::BucketLocator& bucket_locator,
                      const base::FilePath& data_directory,
                      bool force_close);

  void HandleBackingStoreFailure(const storage::BucketLocator& bucket_locator);
  void HandleBackingStoreCorruption(
      const storage::BucketLocator& bucket_locator,
      const IndexedDBDatabaseError& error);

  std::vector<IndexedDBDatabase*> GetOpenDatabasesForBucket(
      const storage::BucketLocator& bucket_locator) const;

  // Close all connections to all databases within the bucket. If
  // `delete_in_memory_store` is true, references to in-memory databases will be
  // dropped thereby allowing their deletion (otherwise they are retained for
  // the lifetime of the factory).
  //
  // TODO(dmurph): This eventually needs to be async, to support scopes
  // multithreading.
  void ForceClose(storage::BucketId bucket_id, bool delete_in_memory_store);

  void ForceSchemaDowngrade(const storage::BucketLocator& bucket_locator);
  V2SchemaCorruptionStatus HasV2SchemaCorruption(
      const storage::BucketLocator& bucket_locator);

  // Called by the IndexedDBContext destructor so the factory can do cleanup.
  void ContextDestroyed();

  // Called by the IndexedDBActiveBlobRegistry.
  void ReportOutstandingBlobs(const storage::BucketLocator& bucket_locator,
                              bool blobs_outstanding);

  // Called by IndexedDBBackingStore when blob files have been cleaned.
  void BlobFilesCleaned(const storage::BucketLocator& bucket_locator);

  size_t GetConnectionCount(storage::BucketId bucket_id) const;

  void NotifyIndexedDBContentChanged(
      const storage::BucketLocator& bucket_locator,
      const std::u16string& database_name,
      const std::u16string& object_store_name);

  int64_t GetInMemoryDBSize(const storage::BucketLocator& bucket_locator) const;

  base::Time GetLastModified(
      const storage::BucketLocator& bucket_locator) const;

  std::vector<storage::BucketId> GetOpenBuckets() const;

  IndexedDBBucketState* GetBucketFactory(const storage::BucketId& id) const;

  // On an OK status, the factory handle is populated. Otherwise (when status is
  // not OK), the `IndexedDBDatabaseError` will be populated. If the status was
  // corruption, the `IndexedDBDataLossInfo` will also be populated.
  std::tuple<IndexedDBBucketStateHandle,
             leveldb::Status,
             IndexedDBDatabaseError,
             IndexedDBDataLossInfo,
             /*was_cold_open=*/bool>
  GetOrOpenBucketFactory(const storage::BucketLocator& bucket_locator,
                         const base::FilePath& data_directory,
                         bool create_if_missing);

  void OnDatabaseError(const storage::BucketLocator& bucket_locator,
                       leveldb::Status s,
                       const char* message);

  using OnDatabaseDeletedCallback = base::RepeatingCallback<void(
      const storage::BucketLocator& deleted_bucket_locator)>;
  void CallOnDatabaseDeletedForTesting(OnDatabaseDeletedCallback callback);

 protected:
  // Used by unittests to allow subclassing of IndexedDBBackingStore.
  virtual std::unique_ptr<IndexedDBBackingStore> CreateBackingStore(
      IndexedDBBackingStore::Mode backing_store_mode,
      TransactionalLevelDBFactory* leveldb_factory,
      const storage::BucketLocator& bucket_locator,
      const base::FilePath& blob_path,
      std::unique_ptr<TransactionalLevelDBDatabase> db,
      storage::mojom::BlobStorageContext* blob_storage_context,
      storage::mojom::FileSystemAccessContext* file_system_access_context,
      std::unique_ptr<storage::FilesystemProxy> filesystem_proxy,
      IndexedDBBackingStore::BlobFilesCleanedCallback blob_files_cleaned,
      IndexedDBBackingStore::ReportOutstandingBlobsCallback
          report_outstanding_blobs,
      scoped_refptr<base::SequencedTaskRunner> idb_task_runner);

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, BackingStoreNoSweeping);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, DatabaseFailedOpen);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           DeleteDatabaseClosesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           ForceCloseReleasesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest,
                           ForceCloseOpenDatabasesOnCommitFailureFirstParty);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest,
                           ForceCloseOpenDatabasesOnCommitFailureThirdParty);

  // `path_base` is the directory that will contain the database directory, the
  // blob directory, and any data loss info. `database_path` is the directory
  // for the leveldb database, and `blob_path` is the directory to store blob
  // files. If `path_base` is empty, then an in-memory database is opened.
  std::tuple<std::unique_ptr<IndexedDBBackingStore>,
             leveldb::Status,
             IndexedDBDataLossInfo,
             bool /* is_disk_full */>
  OpenAndVerifyIndexedDBBackingStore(
      const storage::BucketLocator& bucket_locator,
      base::FilePath data_directory,
      base::FilePath database_path,
      base::FilePath blob_path,
      LevelDBScopesOptions scopes_options,
      LevelDBScopesFactory* scopes_factory,
      std::unique_ptr<storage::FilesystemProxy> filesystem_proxy,
      bool is_first_attempt,
      bool create_if_missing);

  // Called when the database has been deleted on disk.
  void OnDatabaseDeleted(const storage::BucketLocator& bucket_locator);

  void MaybeRunTasksForBucket(const storage::BucketLocator& bucket_locator);
  void RunTasksForBucket(base::WeakPtr<IndexedDBBucketState> bucket_state);

  // Testing helpers, so unit tests don't need to grovel through internal state.
  bool IsDatabaseOpen(const storage::BucketLocator& bucket_locator,
                      const std::u16string& name) const;
  bool IsBackingStoreOpen(const storage::BucketLocator& bucket_locator) const;

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  SEQUENCE_CHECKER(sequence_checker_);
  // Raw pointer is safe because IndexedDBContextImpl owns this object.
  raw_ptr<IndexedDBContextImpl> context_;
  const raw_ptr<IndexedDBClassFactory> class_factory_;
  const raw_ptr<base::Clock> clock_;
  base::Time earliest_sweep_;
  base::Time earliest_compaction_;

  base::flat_map<storage::BucketId, std::unique_ptr<IndexedDBBucketState>>
      factories_per_bucket_;

  std::set<storage::BucketLocator> backends_opened_since_startup_;

  OnDatabaseDeletedCallback call_on_database_deleted_for_testing_;

  // Weak pointers from this factory are used to bind the
  // RunTaskForBucket() function, which deletes the
  // IndexedDBBucketState object. This allows those weak pointers to be
  // invalidated during force close & shutdown to prevent re-entry (see
  // ContextDestroyed()).
  base::WeakPtrFactory<IndexedDBFactory> bucket_state_destruction_weak_factory_{
      this};
  base::WeakPtrFactory<IndexedDBFactory> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_
