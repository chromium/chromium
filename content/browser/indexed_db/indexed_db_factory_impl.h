// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "content/browser/indexed_db/indexed_db_factory.h"

namespace base {
struct Feature;
}

namespace url {
class Origin;
}

namespace content {

class IndexedDBContextImpl;

CONTENT_EXPORT extern const base::Feature kIDBTombstoneStatistics;
CONTENT_EXPORT extern const base::Feature kIDBTombstoneDeletion;

constexpr const char kIDBCloseImmediatelySwitch[] = "idb-close-immediately";

class CONTENT_EXPORT IndexedDBFactoryImpl : public IndexedDBFactory {
 public:
  // Maximum time interval between runs of the IndexedDBSweeper. Sweeping only
  // occurs after backing store close.
  // Visible for testing.
  static constexpr const base::TimeDelta kMaxEarliestGlobalSweepFromNow =
      base::TimeDelta::FromHours(2);
  // Maximum time interval between runs of the IndexedDBSweeper for a given
  // origin. Sweeping only occurs after backing store close.
  // Visible for testing.
  static constexpr const base::TimeDelta kMaxEarliestOriginSweepFromNow =
      base::TimeDelta::FromDays(7);

  IndexedDBFactoryImpl(IndexedDBContextImpl* context, base::Clock* clock);

  // content::IndexedDBFactory overrides:
  void ReleaseDatabase(const IndexedDBDatabase::Identifier& identifier,
                       bool forced_close) override;
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

  void DeleteDatabase(
      const base::string16& name,
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

  OriginDBs GetOpenDatabasesForOrigin(const url::Origin& origin) const override;

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

  // Called by an IndexedDBDatabase when it is actually deleted.
  void DatabaseDeleted(
      const IndexedDBDatabase::Identifier& identifier) override;

  // Called by IndexedDBBackingStore when blob files have been cleaned.
  void BlobFilesCleaned(const url::Origin& origin) override;

  size_t GetConnectionCount(const url::Origin& origin) const override;

  void NotifyIndexedDBContentChanged(
      const url::Origin& origin,
      const base::string16& database_name,
      const base::string16& object_store_name) override;

  int64_t GetInMemoryDBSize(const url::Origin& origin) const override;

  base::Time GetLastModified(const url::Origin& origin) const override;

 protected:
  ~IndexedDBFactoryImpl() override;

  scoped_refptr<IndexedDBBackingStore> OpenBackingStore(
      const url::Origin& origin,
      const base::FilePath& data_directory,
      IndexedDBDataLossInfo* data_loss_info,
      bool* disk_full,
      leveldb::Status* s) override;

  scoped_refptr<IndexedDBBackingStore> OpenBackingStoreHelper(
      const url::Origin& origin,
      const base::FilePath& data_directory,
      IndexedDBDataLossInfo* data_loss_info,
      bool* disk_full,
      bool first_time,
      leveldb::Status* s) override;

  void ReleaseBackingStore(const url::Origin& origin, bool immediate);
  void CloseBackingStore(const url::Origin& origin);
  IndexedDBContextImpl* context() const { return context_; }

 private:
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

  leveldb::Status AbortTransactions(const url::Origin& origin);

  // Called internally after a database is closed, with some delay. If this
  // factory has the last reference it will start running pre-close tasks.
  void MaybeStartPreCloseTasks(const url::Origin& origin);
  // Called internally after pre-close tasks. If this factory has the last
  // reference it will be released.
  void MaybeCloseBackingStore(const url::Origin& origin);
  bool HasLastBackingStoreReference(const url::Origin& origin) const;

  // Testing helpers, so unit tests don't need to grovel through internal state.
  bool IsDatabaseOpen(const url::Origin& origin,
                      const base::string16& name) const;
  bool IsBackingStoreOpen(const url::Origin& origin) const;
  bool IsBackingStorePendingClose(const url::Origin& origin) const;
  void RemoveDatabaseFromMaps(const IndexedDBDatabase::Identifier& identifier);

  IndexedDBContextImpl* context_;

  std::map<IndexedDBDatabase::Identifier, IndexedDBDatabase*> database_map_;
  OriginDBMap origin_dbs_;
  std::map<url::Origin, scoped_refptr<IndexedDBBackingStore>>
      backing_store_map_;

  // In-memory (incognito) backing stores should live as long as the
  // StoragePartition which owns the IndexedDBContext which owns this
  // IndexedDBFactory.
  std::set<scoped_refptr<IndexedDBBackingStore>> in_memory_backing_stores_;
  std::map<url::Origin, scoped_refptr<IndexedDBBackingStore>>
      backing_stores_with_active_blobs_;
  std::set<url::Origin> backends_opened_since_boot_;

  base::Clock* clock_;
  base::Time earliest_sweep_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_
