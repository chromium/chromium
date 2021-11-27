// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_factory.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom-forward.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom-forward.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_storage_key_state_handle.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace content {
class TransactionalLevelDBFactory;
class TransactionalLevelDBDatabase;
class IndexedDBClassFactory;
class IndexedDBContextImpl;
class IndexedDBFactoryImpl;
class IndexedDBStorageKeyState;

class CONTENT_EXPORT IndexedDBFactoryImpl
    : public IndexedDBFactory,
      base::trace_event::MemoryDumpProvider {
 public:
  IndexedDBFactoryImpl(IndexedDBContextImpl* context,
                       IndexedDBClassFactory* indexed_db_class_factory,
                       base::Clock* clock);

  IndexedDBFactoryImpl(const IndexedDBFactoryImpl&) = delete;
  IndexedDBFactoryImpl& operator=(const IndexedDBFactoryImpl&) = delete;

  ~IndexedDBFactoryImpl() override;

  // content::IndexedDBFactory overrides:
  void GetDatabaseInfo(scoped_refptr<IndexedDBCallbacks> callbacks,
                       const blink::StorageKey& storage_key,
                       const base::FilePath& data_directory) override;
  void Open(const std::u16string& name,
            std::unique_ptr<IndexedDBPendingConnection> connection,
            const blink::StorageKey& storage_key,
            const base::FilePath& data_directory) override;

  void DeleteDatabase(const std::u16string& name,
                      scoped_refptr<IndexedDBCallbacks> callbacks,
                      const blink::StorageKey& storage_key,
                      const base::FilePath& data_directory,
                      bool force_close) override;

  void AbortTransactionsAndCompactDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const blink::StorageKey& storage_key) override;
  void AbortTransactionsForDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const blink::StorageKey& storage_key) override;

  void HandleBackingStoreFailure(const blink::StorageKey& storage_key) override;
  void HandleBackingStoreCorruption(
      const blink::StorageKey& storage_key,
      const IndexedDBDatabaseError& error) override;

  std::vector<IndexedDBDatabase*> GetOpenDatabasesForStorageKey(
      const blink::StorageKey& storage_key) const override;

  // TODO(dmurph): This eventually needs to be async, to support scopes
  // multithreading.
  void ForceClose(const blink::StorageKey& storage_key,
                  bool delete_in_memory_store) override;

  void ForceSchemaDowngrade(const blink::StorageKey& storage_key) override;
  V2SchemaCorruptionStatus HasV2SchemaCorruption(
      const blink::StorageKey& storage_key) override;

  // Called by the IndexedDBContext destructor so the factory can do cleanup.
  void ContextDestroyed() override;

  // Called by the IndexedDBActiveBlobRegistry.
  void ReportOutstandingBlobs(const blink::StorageKey& storage_key,
                              bool blobs_outstanding) override;

  // Called by IndexedDBBackingStore when blob files have been cleaned.
  void BlobFilesCleaned(const blink::StorageKey& storage_key) override;

  size_t GetConnectionCount(
      const blink::StorageKey& storage_key) const override;

  void NotifyIndexedDBContentChanged(
      const blink::StorageKey& storage_key,
      const std::u16string& database_name,
      const std::u16string& object_store_name) override;

  int64_t GetInMemoryDBSize(
      const blink::StorageKey& storage_key) const override;

  base::Time GetLastModified(
      const blink::StorageKey& storage_key) const override;

  std::vector<blink::StorageKey> GetOpenStorageKeys() const;

  IndexedDBStorageKeyState* GetStorageKeyFactory(
      const blink::StorageKey& storage_key) const;

  // On an OK status, the factory handle is populated. Otherwise (when status is
  // not OK), the |IndexedDBDatabaseError| will be populated. If the status was
  // corruption, the |IndexedDBDataLossInfo| will also be populated.
  std::tuple<IndexedDBStorageKeyStateHandle,
             leveldb::Status,
             IndexedDBDatabaseError,
             IndexedDBDataLossInfo,
             /*was_cold_open=*/bool>
  GetOrOpenStorageKeyFactory(const blink::StorageKey& storage_key,
                             const base::FilePath& data_directory,
                             bool create_if_missing);

  void OnDatabaseError(const blink::StorageKey& storage_key,
                       leveldb::Status s,
                       const char* message);

  using OnDatabaseDeletedCallback = base::RepeatingCallback<void(
      const blink::StorageKey& deleted_storage_key)>;
  void CallOnDatabaseDeletedForTesting(OnDatabaseDeletedCallback callback);

 protected:
  // Used by unittests to allow subclassing of IndexedDBBackingStore.
  virtual std::unique_ptr<IndexedDBBackingStore> CreateBackingStore(
      IndexedDBBackingStore::Mode backing_store_mode,
      TransactionalLevelDBFactory* leveldb_factory,
      const blink::StorageKey& storage_key,
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
  friend class IndexedDBBrowserTest;
  friend class IndexedDBStorageKeyState;

  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           BackingStoreReleasedOnForcedClose);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           BackingStoreReleaseDelayedOnClose);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, BackingStoreRunPreCloseTasks);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           BackingStoreCloseImmediatelySwitch);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, BackingStoreNoSweeping);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, DatabaseFailedOpen);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           DeleteDatabaseClosesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           ForceCloseReleasesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           GetDatabaseNamesClosesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest,
                           ForceCloseOpenDatabasesOnCommitFailure);

  // |path_base| is the directory that will contain the database directory, the
  // blob directory, and any data loss info. |database_path| is the directory
  // for the leveldb database, and |blob_path| is the directory to store blob
  // files. If |path_base| is empty, then an in-memory database is opened.
  std::tuple<std::unique_ptr<IndexedDBBackingStore>,
             leveldb::Status,
             IndexedDBDataLossInfo,
             bool /* is_disk_full */>
  OpenAndVerifyIndexedDBBackingStore(
      const blink::StorageKey& storage_key,
      base::FilePath data_directory,
      base::FilePath database_path,
      base::FilePath blob_path,
      LevelDBScopesOptions scopes_options,
      LevelDBScopesFactory* scopes_factory,
      std::unique_ptr<storage::FilesystemProxy> filesystem_proxy,
      bool is_first_attempt,
      bool create_if_missing);

  void RemoveStorageKeyState(const blink::StorageKey& storage_key);

  // Called when the database has been deleted on disk.
  void OnDatabaseDeleted(const blink::StorageKey& storage_key);

  void MaybeRunTasksForStorageKey(const blink::StorageKey& storage_key);
  void RunTasksForStorageKey(
      base::WeakPtr<IndexedDBStorageKeyState> storage_key_state);

  // Testing helpers, so unit tests don't need to grovel through internal
  // state.
  bool IsDatabaseOpen(const blink::StorageKey& storage_key,
                      const std::u16string& name) const;
  bool IsBackingStoreOpen(const blink::StorageKey& storage_key) const;
  bool IsBackingStorePendingClose(const blink::StorageKey& storage_key) const;

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  SEQUENCE_CHECKER(sequence_checker_);
  // Raw pointer is safe because IndexedDBContextImpl owns this object.
  raw_ptr<IndexedDBContextImpl> context_;
  const raw_ptr<IndexedDBClassFactory> class_factory_;
  const raw_ptr<base::Clock> clock_;
  base::Time earliest_sweep_;
  base::Time earliest_compaction_;

  base::flat_map<blink::StorageKey, std::unique_ptr<IndexedDBStorageKeyState>>
      factories_per_storage_key_;

  std::set<blink::StorageKey> backends_opened_since_startup_;

  OnDatabaseDeletedCallback call_on_database_deleted_for_testing_;

  // Weak pointers from this factory are used to bind the
  // RemoveStorageKeyState() function, which deletes the
  // IndexedDBStorageKeyState object. This allows those weak pointers to be
  // invalidated during force close & shutdown to prevent re-entry (see
  // ContextDestroyed()).
  base::WeakPtrFactory<IndexedDBFactoryImpl>
      storage_key_state_destruction_weak_factory_{this};
  base::WeakPtrFactory<IndexedDBFactoryImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_
