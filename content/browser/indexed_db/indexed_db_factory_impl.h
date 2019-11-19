// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_factory.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_origin_state_handle.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "url/origin.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}
namespace url {
class Origin;
}

namespace content {
class TransactionalLevelDBFactory;
class TransactionalLevelDBDatabase;
class IndexedDBClassFactory;
class IndexedDBContextImpl;
class IndexedDBFactoryImpl;
class IndexedDBOriginState;

class CONTENT_EXPORT IndexedDBFactoryImpl : public IndexedDBFactory {
 public:
  IndexedDBFactoryImpl(IndexedDBContextImpl* context,
                       IndexedDBClassFactory* indexed_db_class_factory,
                       base::Clock* clock);
  ~IndexedDBFactoryImpl() override;

  // content::IndexedDBFactory overrides:
  void GetDatabaseInfo(scoped_refptr<IndexedDBCallbacks> callbacks,
                       const url::Origin& origin,
                       const base::FilePath& data_directory) override;
  void GetDatabaseNames(scoped_refptr<IndexedDBCallbacks> callbacks,
                        const url::Origin& origin,
                        const base::FilePath& data_directory) override;
  void Open(const base::string16& name,
            std::unique_ptr<IndexedDBPendingConnection> connection,
            const url::Origin& origin,
            const base::FilePath& data_directory) override;

  void DeleteDatabase(const base::string16& name,
                      scoped_refptr<IndexedDBCallbacks> callbacks,
                      const url::Origin& origin,
                      const base::FilePath& data_directory,
                      bool force_close) override;

  void AbortTransactionsAndCompactDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const url::Origin& origin) override;
  void AbortTransactionsForDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const url::Origin& origin) override;

  void HandleBackingStoreFailure(const url::Origin& origin) override;
  void HandleBackingStoreCorruption(
      const url::Origin& origin,
      const IndexedDBDatabaseError& error) override;

  std::vector<IndexedDBDatabase*> GetOpenDatabasesForOrigin(
      const url::Origin& origin) const override;

  // TODO(dmurph): This eventually needs to be async, to support scopes
  // multithreading.
  void ForceClose(const url::Origin& origin,
                  bool delete_in_memory_store) override;

  void ForceSchemaDowngrade(const url::Origin& origin) override;
  V2SchemaCorruptionStatus HasV2SchemaCorruption(
      const url::Origin& origin) override;

  // Called by the IndexedDBContext destructor so the factory can do cleanup.
  void ContextDestroyed() override;

  // Called by the IndexedDBActiveBlobRegistry.
  void ReportOutstandingBlobs(const url::Origin& origin,
                              bool blobs_outstanding) override;

  // Called by IndexedDBBackingStore when blob files have been cleaned.
  void BlobFilesCleaned(const url::Origin& origin) override;

  size_t GetConnectionCount(const url::Origin& origin) const override;

  void NotifyIndexedDBContentChanged(
      const url::Origin& origin,
      const base::string16& database_name,
      const base::string16& object_store_name) override;

  int64_t GetInMemoryDBSize(const url::Origin& origin) const override;

  base::Time GetLastModified(const url::Origin& origin) const override;

  std::vector<url::Origin> GetOpenOrigins() const;

  IndexedDBOriginState* GetOriginFactory(const url::Origin& origin) const;

  // On an OK status, the factory handle is populated. Otherwise (when status is
  // not OK), the |IndexedDBDatabaseError| will be populated. If the status was
  // corruption, the |IndexedDBDataLossInfo| will also be populated.
  std::tuple<IndexedDBOriginStateHandle,
             leveldb::Status,
             IndexedDBDatabaseError,
             IndexedDBDataLossInfo,
             /*was_cold_open=*/bool>
  GetOrOpenOriginFactory(const url::Origin& origin,
                         const base::FilePath& data_directory,
                         bool create_if_missing);

  void OnDatabaseError(const url::Origin& origin,
                       leveldb::Status s,
                       const char* message);

 protected:
  // Used by unittests to allow subclassing of IndexedDBBackingStore.
  virtual std::unique_ptr<IndexedDBBackingStore> CreateBackingStore(
      IndexedDBBackingStore::Mode backing_store_mode,
      TransactionalLevelDBFactory* leveldb_factory,
      const url::Origin& origin,
      const base::FilePath& blob_path,
      std::unique_ptr<TransactionalLevelDBDatabase> db,
      IndexedDBBackingStore::BlobFilesCleanedCallback blob_files_cleaned,
      IndexedDBBackingStore::ReportOutstandingBlobsCallback
          report_outstanding_blobs,
      base::SequencedTaskRunner* task_runner);

  IndexedDBContextImpl* context() const { return context_; }

 private:
  friend class IndexedDBBrowserTest;
  friend class IndexedDBOriginState;

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
  OpenAndVerifyIndexedDBBackingStore(const url::Origin& origin,
                                     base::FilePath data_directory,
                                     base::FilePath database_path,
                                     base::FilePath blob_path,
                                     LevelDBScopesOptions scopes_options,
                                     LevelDBScopesFactory* scopes_factory,
                                     bool is_first_attempt,
                                     bool create_if_missing);

  void RemoveOriginState(const url::Origin& origin);

  // Called when the database has been deleted on disk.
  void OnDatabaseDeleted(const url::Origin& origin);

  void MaybeRunTasksForOrigin(const url::Origin& origin);
  void RunTasksForOrigin(base::WeakPtr<IndexedDBOriginState> origin_state);

  // Testing helpers, so unit tests don't need to grovel through internal
  // state.
  bool IsDatabaseOpen(const url::Origin& origin,
                      const base::string16& name) const;
  bool IsBackingStoreOpen(const url::Origin& origin) const;
  bool IsBackingStorePendingClose(const url::Origin& origin) const;

  SEQUENCE_CHECKER(sequence_checker_);
  // Raw pointer is safe because IndexedDBContextImpl owns this object.
  IndexedDBContextImpl* context_;
  IndexedDBClassFactory* const class_factory_;
  base::Clock* const clock_;
  base::Time earliest_sweep_;

  base::flat_map<url::Origin, std::unique_ptr<IndexedDBOriginState>>
      factories_per_origin_;

  std::set<url::Origin> backends_opened_since_startup_;

  // Weak pointers from this factory are used to bind the RemoveOriginState()
  // function, which deletes the IndexedDBOriginState object. This allows those
  // weak pointers to be invalidated during force close & shutdown to prevent
  // re-entry (see ContextDestroyed()).
  base::WeakPtrFactory<IndexedDBFactoryImpl>
      origin_state_destruction_weak_factory_{this};
  base::WeakPtrFactory<IndexedDBFactoryImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(IndexedDBFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_
