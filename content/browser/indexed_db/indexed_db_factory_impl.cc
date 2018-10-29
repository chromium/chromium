// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_factory_impl.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_metadata_coding.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
#include "content/browser/indexed_db/indexed_db_tombstone_sweeper.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"
#include "third_party/leveldatabase/env_chromium.h"

using base::ASCIIToUTF16;
using url::Origin;

namespace content {

namespace {

using PreCloseTask = IndexedDBPreCloseTaskQueue::PreCloseTask;

// Time after the last connection to a database is closed and when we destroy
// the backing store.
const int64_t kBackingStoreGracePeriodSeconds = 2;
// Total time we let pre-close tasks run.
const int64_t kPreCloseTasksMaxRunPeriodSeconds = 60;
// The number of iterations for every 'round' of the tombstone sweeper.
const int kTombstoneSweeperRoundIterations = 1000;
// The maximum total iterations for the tombstone sweeper.
const int kTombstoneSweeperMaxIterations = 10 * 1000 * 1000;

constexpr const base::TimeDelta kMinEarliestOriginSweepFromNow =
    base::TimeDelta::FromDays(1);
static_assert(kMinEarliestOriginSweepFromNow <
                  IndexedDBFactoryImpl::kMaxEarliestOriginSweepFromNow,
              "Min < Max");

constexpr const base::TimeDelta kMinEarliestGlobalSweepFromNow =
    base::TimeDelta::FromMinutes(10);
static_assert(kMinEarliestGlobalSweepFromNow <
                  IndexedDBFactoryImpl::kMaxEarliestGlobalSweepFromNow,
              "Min < Max");

base::Time GenerateNextOriginSweepTime(base::Time now) {
  uint64_t range =
      IndexedDBFactoryImpl::kMaxEarliestOriginSweepFromNow.InMilliseconds() -
      kMinEarliestOriginSweepFromNow.InMilliseconds();
  int64_t rand_millis = kMinEarliestOriginSweepFromNow.InMilliseconds() +
                        static_cast<int64_t>(base::RandGenerator(range));
  return now + base::TimeDelta::FromMilliseconds(rand_millis);
}

base::Time GenerateNextGlobalSweepTime(base::Time now) {
  uint64_t range =
      IndexedDBFactoryImpl::kMaxEarliestGlobalSweepFromNow.InMilliseconds() -
      kMinEarliestGlobalSweepFromNow.InMilliseconds();
  int64_t rand_millis = kMinEarliestGlobalSweepFromNow.InMilliseconds() +
                        static_cast<int64_t>(base::RandGenerator(range));
  return now + base::TimeDelta::FromMilliseconds(rand_millis);
}

leveldb::Status GetDBSizeFromEnv(leveldb::Env* env,
                                 const std::string& path,
                                 int64_t* total_size_out) {
  *total_size_out = 0;
  // Root path should be /, but in MemEnv, a path name is not tailed with '/'
  DCHECK_EQ(path.back(), '/');
  const std::string path_without_slash = path.substr(0, path.length() - 1);

  // This assumes that leveldb will not put a subdirectory into the directory
  std::vector<std::string> file_names;
  leveldb::Status s = env->GetChildren(path_without_slash, &file_names);
  if (!s.ok())
    return s;

  for (std::string& file_name : file_names) {
    file_name.insert(0, path);
    uint64_t file_size;
    s = env->GetFileSize(file_name, &file_size);
    if (!s.ok())
      return s;
    else
      *total_size_out += static_cast<int64_t>(file_size);
  }
  return s;
}

}  // namespace

const base::Feature kIDBTombstoneStatistics{"IDBTombstoneStatistics",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIDBTombstoneDeletion{"IDBTombstoneDeletion",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

constexpr const base::TimeDelta
    IndexedDBFactoryImpl::kMaxEarliestGlobalSweepFromNow;
constexpr const base::TimeDelta
    IndexedDBFactoryImpl::kMaxEarliestOriginSweepFromNow;

IndexedDBFactoryImpl::IndexedDBFactoryImpl(IndexedDBContextImpl* context,
                                           base::Clock* clock)
    : context_(context),
      clock_(clock),
      earliest_sweep_(GenerateNextGlobalSweepTime(clock_->Now())) {}

IndexedDBFactoryImpl::~IndexedDBFactoryImpl() {
}

void IndexedDBFactoryImpl::RemoveDatabaseFromMaps(
    const IndexedDBDatabase::Identifier& identifier) {
  const auto& it = database_map_.find(identifier);
  DCHECK(it != database_map_.end());
  IndexedDBDatabase* database = it->second;
  database_map_.erase(it);

  std::pair<OriginDBMap::iterator, OriginDBMap::iterator> range =
      origin_dbs_.equal_range(database->identifier().first);
  DCHECK(range.first != range.second);
  for (auto it2 = range.first; it2 != range.second; ++it2) {
    if (it2->second == database) {
      origin_dbs_.erase(it2);
      break;
    }
  }
}

void IndexedDBFactoryImpl::ReleaseDatabase(
    const IndexedDBDatabase::Identifier& identifier,
    bool forced_close) {
  DCHECK(!database_map_.find(identifier)->second->backing_store());

  RemoveDatabaseFromMaps(identifier);

  // No grace period on a forced-close, as the initiator is
  // assuming the backing store will be released once all
  // connections are closed.
  ReleaseBackingStore(identifier.first, forced_close);
}

void IndexedDBFactoryImpl::ReleaseBackingStore(const Origin& origin,
                                               bool immediate) {
  if (immediate) {
    const auto& it = backing_stores_with_active_blobs_.find(origin);
    if (it != backing_stores_with_active_blobs_.end()) {
      it->second->active_blob_registry()->ForceShutdown();
      backing_stores_with_active_blobs_.erase(it);
    }
  }

  // Only close if this is the last reference.
  if (!HasLastBackingStoreReference(origin))
    return;

  // If this factory does hold the last reference to the backing store, it can
  // be closed - but unless requested to close it immediately, keep it around
  // for a short period so that a re-open is fast.
  if (immediate) {
    CloseBackingStore(origin);
    return;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kIDBCloseImmediatelySwitch)) {
    MaybeCloseBackingStore(origin);
    return;
  }

  // Start a timer to close the backing store, unless something else opens it
  // in the mean time.
  DCHECK(!backing_store_map_[origin]->close_timer()->IsRunning());
  backing_store_map_[origin]->close_timer()->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kBackingStoreGracePeriodSeconds),
      base::BindOnce(&IndexedDBFactoryImpl::MaybeStartPreCloseTasks, this,
                     origin));
}

void IndexedDBFactoryImpl::MaybeStartPreCloseTasks(const Origin& origin) {
  // Another reference may have been created since the maybe-close was posted,
  // so it is necessary to check again.
  if (!HasLastBackingStoreReference(origin))
    return;

  base::ScopedClosureRunner maybe_close_backing_store_runner(
      base::BindOnce(&IndexedDBFactoryImpl::MaybeCloseBackingStore,
                     base::Unretained(this), origin));

  base::Time now = clock_->Now();

  // Check that the last sweep hasn't run too recently.
  if (earliest_sweep_ > now)
    return;

  bool tombstone_stats_enabled =
      base::FeatureList::IsEnabled(kIDBTombstoneStatistics);
  bool tombstone_deletion_enabled =
      base::FeatureList::IsEnabled(kIDBTombstoneDeletion);

  // After this check, exactly one of the flags must be true.
  if (tombstone_stats_enabled == tombstone_deletion_enabled)
    return;

  scoped_refptr<IndexedDBBackingStore> store = backing_store_map_[origin];

  base::Time origin_earliest_sweep;
  leveldb::Status s =
      indexed_db::GetEarliestSweepTime(store->db(), &origin_earliest_sweep);
  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok())
    return;

  // This origin hasn't been swept too recently.
  if (origin_earliest_sweep > now)
    return;

  // A sweep will happen now, so reset the sweep timers.
  earliest_sweep_ = GenerateNextGlobalSweepTime(now);
  scoped_refptr<LevelDBTransaction> txn =
      IndexedDBClassFactory::Get()->CreateLevelDBTransaction(store->db());
  indexed_db::SetEarliestSweepTime(txn.get(), GenerateNextOriginSweepTime(now));
  s = txn->Commit();

  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok())
    return;

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  IndexedDBTombstoneSweeper::Mode mode =
      tombstone_stats_enabled ? IndexedDBTombstoneSweeper::Mode::STATISTICS
                              : IndexedDBTombstoneSweeper::Mode::DELETION;
  tasks.push_back(std::make_unique<IndexedDBTombstoneSweeper>(
      mode, kTombstoneSweeperRoundIterations, kTombstoneSweeperMaxIterations,
      store->db()->db()));
  // TODO(dmurph): Add compaction task that compacts all indexes if we have
  // more than X deletions.

  store->SetPreCloseTaskList(std::make_unique<IndexedDBPreCloseTaskQueue>(
      std::move(tasks), maybe_close_backing_store_runner.Release(),
      base::TimeDelta::FromSeconds(kPreCloseTasksMaxRunPeriodSeconds),
      std::make_unique<base::OneShotTimer>()));
  store->StartPreCloseTasks();
}

void IndexedDBFactoryImpl::MaybeCloseBackingStore(const Origin& origin) {
  backing_store_map_[origin]->SetPreCloseTaskList(nullptr);
  // Another reference may have opened since the maybe-close was posted, so it
  // is necessary to check again.
  if (HasLastBackingStoreReference(origin))
    CloseBackingStore(origin);
}

void IndexedDBFactoryImpl::CloseBackingStore(const Origin& origin) {
  const auto& it = backing_store_map_.find(origin);
  DCHECK(it != backing_store_map_.end());
  // Stop the timer and pre close tasks (if they are running) - this may happen
  // if the timer was started and then a forced close occurs.
  scoped_refptr<IndexedDBBackingStore>& backing_store = it->second;
  backing_store->close_timer()->Stop();
  backing_store->SetPreCloseTaskList(nullptr);

  if (backing_store->IsBlobCleanupPending())
    backing_store->ForceRunBlobCleanup();

  backing_store_map_.erase(it);
}

bool IndexedDBFactoryImpl::HasLastBackingStoreReference(
    const Origin& origin) const {
  IndexedDBBackingStore* ptr;
  {
    // Scope so that the implicit scoped_refptr<> is freed.
    const auto& it = backing_store_map_.find(origin);
    DCHECK(it != backing_store_map_.end());
    ptr = it->second.get();
  }
  return ptr->HasOneRef();
}

leveldb::Status IndexedDBFactoryImpl::AbortTransactions(const Origin& origin) {
  const scoped_refptr<IndexedDBBackingStore>& backing_store =
      backing_store_map_[origin];
  if (!backing_store) {
    return leveldb::Status::IOError(
        "Internal error opening backing store for "
        "indexedDB.abortTransactions.");
  }

  leveldb::Status get_names_status;
  IndexedDBMetadataCoding metadata_coding;
  std::vector<base::string16> db_names;
  get_names_status = metadata_coding.ReadDatabaseNames(
      backing_store->db(), backing_store->origin_identifier(), &db_names);
  if (!get_names_status.ok()) {
    return leveldb::Status::IOError(
        "Internal error getting origin database names for "
        "indexedDB.abortTransactions.");
  }

  for (base::string16& name : db_names) {
    const scoped_refptr<IndexedDBDatabase>& db =
        database_map_[std::make_pair(origin, name)];
    db->AbortAllTransactionsForConnections();
  }

  return leveldb::Status::OK();
}

void IndexedDBFactoryImpl::ForceClose(const Origin& origin,
                                      bool delete_in_memory_store) {
  OriginDBs range = GetOpenDatabasesForOrigin(origin);

  while (range.first != range.second) {
    IndexedDBDatabase* db = range.first->second;
    ++range.first;
    db->ForceClose();
  }

  auto it = backing_store_map_.find(origin);
  if (it != backing_store_map_.end()) {
    if (delete_in_memory_store)
      in_memory_backing_stores_.erase(it->second);

    ReleaseBackingStore(origin, true /* immediate */);
  }
}

void IndexedDBFactoryImpl::ForceSchemaDowngrade(const Origin& origin) {
  auto it = backing_store_map_.find(origin);
  if (it == backing_store_map_.end())
    return;

  IndexedDBBackingStore* backing_store = it->second.get();
  leveldb::Status s = backing_store->RevertSchemaToV2();
  DLOG_IF(ERROR, !s.ok()) << "Unable to force downgrade: " << s.ToString();
}

V2SchemaCorruptionStatus IndexedDBFactoryImpl::HasV2SchemaCorruption(
    const Origin& origin) {
  auto it = backing_store_map_.find(origin);
  if (it == backing_store_map_.end())
    return V2SchemaCorruptionStatus::kUnknown;

  IndexedDBBackingStore* backing_store = it->second.get();
  return backing_store->HasV2SchemaCorruption();
}

void IndexedDBFactoryImpl::ContextDestroyed() {
  // Timers on backing stores hold a reference to this factory. When the
  // context (which nominally owns this factory) is destroyed during thread
  // termination the timers must be stopped so that this factory and the
  // stores can be disposed of.
  for (const auto& it : backing_store_map_) {
    it.second->close_timer()->Stop();
    it.second->SetPreCloseTaskList(nullptr);
  }
  backing_store_map_.clear();
  backing_stores_with_active_blobs_.clear();
  context_ = nullptr;
}

void IndexedDBFactoryImpl::ReportOutstandingBlobs(const Origin& origin,
                                                  bool blobs_outstanding) {
  if (!context_)
    return;
  if (blobs_outstanding) {
    DCHECK(!backing_stores_with_active_blobs_.count(origin));
    const auto& it = backing_store_map_.find(origin);
    if (it != backing_store_map_.end())
      backing_stores_with_active_blobs_.insert(*it);
    else
      DCHECK(false);
  } else {
    const auto& it = backing_stores_with_active_blobs_.find(origin);
    if (it != backing_stores_with_active_blobs_.end()) {
      backing_stores_with_active_blobs_.erase(it);
      ReleaseBackingStore(origin, false /* immediate */);
    }
  }
}

void IndexedDBFactoryImpl::GetDatabaseInfo(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const Origin& origin,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactoryImpl::GetDatabaseInfo");
  // TODO(dmurph): Plumb data_loss back to script eventually?
  IndexedDBDataLossInfo data_loss_info;
  bool disk_full;
  leveldb::Status s;
  // TODO(dmurph): Handle this error
  scoped_refptr<IndexedDBBackingStore> backing_store =
      OpenBackingStore(origin, data_directory, &data_loss_info, &disk_full, &s);
  if (!backing_store.get()) {
    IndexedDBDatabaseError error(
        blink::kWebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16("Internal error opening backing store for "
                     "indexedDB.databases()."));
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }

  IndexedDBMetadataCoding metadata_coding;
  std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions;
  s = metadata_coding.ReadDatabaseNamesAndVersions(
      backing_store->db(), backing_store->origin_identifier(),
      &names_and_versions);
  if (!s.ok()) {
    DLOG(ERROR) << "Internal error getting database info";
    IndexedDBDatabaseError error(blink::kWebIDBDatabaseExceptionUnknownError,
                                 "Internal error opening backing store for "
                                 "indexedDB.databases().");
    callbacks->OnError(error);
    backing_store = nullptr;
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }
  callbacks->OnSuccess(std::move(names_and_versions));
  backing_store = nullptr;
  ReleaseBackingStore(origin, false /* immediate */);
}

void IndexedDBFactoryImpl::GetDatabaseNames(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const Origin& origin,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactoryImpl::GetDatabaseNames");
  // TODO(dgrogan): Plumb data_loss back to script eventually?
  IndexedDBDataLossInfo data_loss_info;
  bool disk_full;
  leveldb::Status s;
  // TODO(cmumford): Handle this error
  scoped_refptr<IndexedDBBackingStore> backing_store =
      OpenBackingStore(origin, data_directory, &data_loss_info, &disk_full, &s);
  if (!backing_store.get()) {
    IndexedDBDatabaseError error(
        blink::kWebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16("Internal error opening backing store for "
                     "indexedDB.webkitGetDatabaseNames."));
    callbacks->OnError(error);
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }

  IndexedDBMetadataCoding metadata_coding;
  std::vector<base::string16> names;
  s = metadata_coding.ReadDatabaseNames(
      backing_store->db(), backing_store->origin_identifier(), &names);
  if (!s.ok()) {
    DLOG(ERROR) << "Internal error getting database names";
    IndexedDBDatabaseError error(blink::kWebIDBDatabaseExceptionUnknownError,
                                 "Internal error opening backing store for "
                                 "indexedDB.webkitGetDatabaseNames.");
    callbacks->OnError(error);
    backing_store = nullptr;
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }
  callbacks->OnSuccess(names);
  backing_store = nullptr;
  ReleaseBackingStore(origin, false /* immediate */);
}

void IndexedDBFactoryImpl::DeleteDatabase(
    const base::string16& name,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const Origin& origin,
    const base::FilePath& data_directory,
    bool force_close) {
  IDB_TRACE("IndexedDBFactoryImpl::DeleteDatabase");
  IndexedDBDatabase::Identifier unique_identifier(origin, name);
  const auto& it = database_map_.find(unique_identifier);
  if (it != database_map_.end()) {
    // If there are any connections to the database, directly delete the
    // database.
    it->second->DeleteDatabase(callbacks, force_close);
    return;
  }

  // TODO(dgrogan): Plumb data_loss back to script eventually?
  IndexedDBDataLossInfo data_loss_info;
  bool disk_full = false;
  leveldb::Status s;
  scoped_refptr<IndexedDBBackingStore> backing_store =
      OpenBackingStore(origin, data_directory, &data_loss_info, &disk_full, &s);
  if (!backing_store.get()) {
    IndexedDBDatabaseError error(
        blink::kWebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16("Internal error opening backing store "
                     "for indexedDB.deleteDatabase."));
    callbacks->OnError(error);
    if (s.IsCorruption()) {
      HandleBackingStoreCorruption(origin, error);
    }
    return;
  }

  IndexedDBMetadataCoding metadata_coding;
  std::vector<base::string16> names;
  s = metadata_coding.ReadDatabaseNames(
      backing_store->db(), backing_store->origin_identifier(), &names);
  if (!s.ok()) {
    DLOG(ERROR) << "Internal error getting database names";
    IndexedDBDatabaseError error(blink::kWebIDBDatabaseExceptionUnknownError,
                                 "Internal error opening backing store for "
                                 "indexedDB.deleteDatabase.");
    callbacks->OnError(error);
    backing_store = nullptr;
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin, error);
    return;
  }
  if (!base::ContainsValue(names, name)) {
    const int64_t version = 0;
    callbacks->OnSuccess(version);
    backing_store = nullptr;
    ReleaseBackingStore(origin, false /* immediate */);
    return;
  }

  scoped_refptr<IndexedDBDatabase> database;
  std::tie(database, s) = IndexedDBDatabase::Create(
      name, backing_store.get(), this,
      std::make_unique<IndexedDBMetadataCoding>(), unique_identifier);
  if (!database.get()) {
    IndexedDBDatabaseError error(
        blink::kWebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16("Internal error creating database backend for "
                     "indexedDB.deleteDatabase."));
    callbacks->OnError(error);
    if (s.IsCorruption()) {
      backing_store = nullptr;
      HandleBackingStoreCorruption(origin, error);
    }
    return;
  }

  database_map_[unique_identifier] = database.get();
  origin_dbs_.insert(std::make_pair(origin, database.get()));
  database->DeleteDatabase(callbacks, force_close);
  RemoveDatabaseFromMaps(unique_identifier);
  database = nullptr;
  backing_store = nullptr;
  ReleaseBackingStore(origin, false /* immediate */);
}

void IndexedDBFactoryImpl::DatabaseDeleted(
    const IndexedDBDatabase::Identifier& identifier) {
  // NULL after ContextDestroyed() called, and in some unit tests.
  if (!context_)
    return;
  context_->DatabaseDeleted(identifier.first);
}

void IndexedDBFactoryImpl::BlobFilesCleaned(const url::Origin& origin) {
  // NULL after ContextDestroyed() called, and in some unit tests.
  if (!context_)
    return;
  context_->BlobFilesCleaned(origin);
}

void IndexedDBFactoryImpl::AbortTransactionsAndCompactDatabase(
    base::OnceCallback<void(leveldb::Status)> callback,
    const Origin& origin) {
  IDB_TRACE("IndexedDBFactoryImpl::AbortTransactionsAndCompactDatabase");
  const scoped_refptr<IndexedDBBackingStore>& backing_store =
      backing_store_map_[origin];
  if (!backing_store) {
    std::move(callback).Run(leveldb::Status::IOError(
        "Internal error opening backing store for "
        "indexedDB.abortTransactionsAndCompactDatabase."));
    return;
  }
  leveldb::Status status = AbortTransactions(origin);
  backing_store->Compact();
  std::move(callback).Run(status);
}

void IndexedDBFactoryImpl::AbortTransactionsForDatabase(
    base::OnceCallback<void(leveldb::Status)> callback,
    const Origin& origin) {
  IDB_TRACE("IndexedDBFactoryImpl::AbortTransactionsForDatabase");
  if (!backing_store_map_[origin]) {
    std::move(callback).Run(
        leveldb::Status::IOError("Internal error opening backing store for "
                                 "indexedDB.abortTransactionsForDatabase."));
    return;
  }
  std::move(callback).Run(AbortTransactions(origin));
}

void IndexedDBFactoryImpl::HandleBackingStoreFailure(const Origin& origin) {
  // NULL after ContextDestroyed() called, and in some unit tests.
  if (!context_)
    return;
  context_->ForceClose(origin,
                       IndexedDBContextImpl::FORCE_CLOSE_BACKING_STORE_FAILURE);
}

void IndexedDBFactoryImpl::HandleBackingStoreCorruption(
    const Origin& origin,
    const IndexedDBDatabaseError& error) {
  // Make a copy of origin as this is likely a reference to a member of a
  // backing store which this function will be deleting.
  Origin saved_origin(origin);
  DCHECK(context_);
  base::FilePath path_base = context_->data_path();

  // The message may contain the database path, which may be considered
  // sensitive data, and those strings are passed to the extension, so strip it.
  std::string sanitized_message = base::UTF16ToUTF8(error.message());
  base::ReplaceSubstringsAfterOffset(&sanitized_message, 0u,
                                     path_base.AsUTF8Unsafe(), "...");
  IndexedDBBackingStore::RecordCorruptionInfo(path_base, saved_origin,
                                              sanitized_message);
  HandleBackingStoreFailure(saved_origin);
  // Note: DestroyBackingStore only deletes LevelDB files, leaving all others,
  //       so our corruption info file will remain.
  leveldb::Status s =
      IndexedDBBackingStore::DestroyBackingStore(path_base, saved_origin);
  DLOG_IF(ERROR, !s.ok()) << "Unable to delete backing store: " << s.ToString();
  UMA_HISTOGRAM_ENUMERATION(
      "WebCore.IndexedDB.DestroyCorruptBackingStoreStatus",
      leveldb_env::GetLevelDBStatusUMAValue(s),
      leveldb_env::LEVELDB_STATUS_MAX);
}

bool IndexedDBFactoryImpl::IsDatabaseOpen(const Origin& origin,
                                          const base::string16& name) const {
  return base::ContainsKey(database_map_,
                           IndexedDBDatabase::Identifier(origin, name));
}

bool IndexedDBFactoryImpl::IsBackingStoreOpen(const Origin& origin) const {
  return base::ContainsKey(backing_store_map_, origin);
}

bool IndexedDBFactoryImpl::IsBackingStorePendingClose(
    const Origin& origin) const {
  const auto& it = backing_store_map_.find(origin);
  if (it == backing_store_map_.end())
    return false;
  return it->second->close_timer()->IsRunning() ||
         it->second->pre_close_task_queue();
}

scoped_refptr<IndexedDBBackingStore>
IndexedDBFactoryImpl::OpenBackingStoreHelper(
    const Origin& origin,
    const base::FilePath& data_directory,
    IndexedDBDataLossInfo* data_loss_info,
    bool* disk_full,
    bool first_time,
    leveldb::Status* status) {
  return IndexedDBBackingStore::Open(
      this, origin, data_directory, data_loss_info, disk_full,
      context_->TaskRunner(), first_time, status);
}

scoped_refptr<IndexedDBBackingStore> IndexedDBFactoryImpl::OpenBackingStore(
    const Origin& origin,
    const base::FilePath& data_directory,
    IndexedDBDataLossInfo* data_loss_info,
    bool* disk_full,
    leveldb::Status* status) {
  const bool open_in_memory = data_directory.empty();

  const auto& it2 = backing_store_map_.find(origin);
  if (it2 != backing_store_map_.end()) {
    // Grab a refptr so the completion of the preclose task list doesn't close
    // the backing store.
    scoped_refptr<IndexedDBBackingStore> backing_store = it2->second;
    backing_store->close_timer()->Stop();
    if (it2->second->pre_close_task_queue()) {
      backing_store->pre_close_task_queue()->StopForNewConnection();
      backing_store->SetPreCloseTaskList(nullptr);
    }
    return it2->second;
  }

  scoped_refptr<IndexedDBBackingStore> backing_store;
  bool first_time = false;
  if (open_in_memory) {
    backing_store = IndexedDBBackingStore::OpenInMemory(
        origin, context_->TaskRunner(), status);
  } else {
    first_time = !backends_opened_since_boot_.count(origin);

    backing_store = OpenBackingStoreHelper(
        origin, data_directory, data_loss_info, disk_full, first_time, status);
  }

  if (backing_store.get()) {
    if (first_time)
      backends_opened_since_boot_.insert(origin);
    backing_store_map_[origin] = backing_store;

    // If an in-memory database, bind lifetime to this factory instance.
    if (open_in_memory)
      in_memory_backing_stores_.insert(backing_store);

    // All backing stores associated with this factory should be of the same
    // type.
    DCHECK_NE(in_memory_backing_stores_.empty(), open_in_memory);

    return backing_store;
  }

  return nullptr;
}

void IndexedDBFactoryImpl::Open(
    const base::string16& name,
    std::unique_ptr<IndexedDBPendingConnection> connection,
    const Origin& origin,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactoryImpl::Open");
  scoped_refptr<IndexedDBDatabase> database;
  IndexedDBDatabase::Identifier unique_identifier(origin, name);
  const auto& it = database_map_.find(unique_identifier);
  IndexedDBDataLossInfo data_loss_info;
  bool disk_full = false;
  bool was_open = (it != database_map_.end());
  if (!was_open) {
    leveldb::Status s;
    scoped_refptr<IndexedDBBackingStore> backing_store = OpenBackingStore(
        origin, data_directory, &data_loss_info, &disk_full, &s);
    if (!backing_store.get()) {
      if (disk_full) {
        connection->callbacks->OnError(IndexedDBDatabaseError(
            blink::kWebIDBDatabaseExceptionQuotaError,
            ASCIIToUTF16("Encountered full disk while opening "
                         "backing store for indexedDB.open.")));
        return;
      }
      IndexedDBDatabaseError error(
          blink::kWebIDBDatabaseExceptionUnknownError,
          ASCIIToUTF16("Internal error opening backing store"
                       " for indexedDB.open."));
      connection->callbacks->OnError(error);
      if (s.IsCorruption()) {
        HandleBackingStoreCorruption(origin, error);
      }
      return;
    }

    std::tie(database, s) = IndexedDBDatabase::Create(
        name, backing_store.get(), this,
        std::make_unique<IndexedDBMetadataCoding>(), unique_identifier);
    if (!database.get()) {
      DLOG(ERROR) << "Unable to create the database";
      IndexedDBDatabaseError error(blink::kWebIDBDatabaseExceptionUnknownError,
                                   ASCIIToUTF16("Internal error creating "
                                                "database backend for "
                                                "indexedDB.open."));
      connection->callbacks->OnError(error);
      if (s.IsCorruption()) {
        backing_store =
            nullptr;  // Closes the LevelDB so that it can be deleted
        HandleBackingStoreCorruption(origin, error);
      }
      return;
    }
  } else {
    database = it->second;
  }

  connection->data_loss_info = data_loss_info;

  database->OpenConnection(std::move(connection));

  if (!was_open && database->ConnectionCount() > 0) {
    database_map_[unique_identifier] = database.get();
    origin_dbs_.insert(std::make_pair(origin, database.get()));
  }
}

std::pair<IndexedDBFactoryImpl::OriginDBMapIterator,
          IndexedDBFactoryImpl::OriginDBMapIterator>
IndexedDBFactoryImpl::GetOpenDatabasesForOrigin(const Origin& origin) const {
  return origin_dbs_.equal_range(origin);
}

size_t IndexedDBFactoryImpl::GetConnectionCount(const Origin& origin) const {
  size_t count(0);

  OriginDBs range = GetOpenDatabasesForOrigin(origin);
  for (auto it = range.first; it != range.second; ++it)
    count += it->second->ConnectionCount();

  return count;
}

int64_t IndexedDBFactoryImpl::GetInMemoryDBSize(const Origin& origin) const {
  const auto& it = backing_store_map_.find(origin);
  // Origin won't be present in map if it has been deleted.
  if (it == backing_store_map_.end())
    return 0;

  const scoped_refptr<IndexedDBBackingStore>& backing_store = it->second;
  int64_t level_db_size = 0;
  leveldb::Status s =
      GetDBSizeFromEnv(backing_store->db()->env(), "/", &level_db_size);
  if (!s.ok())
    LOG(ERROR) << "Failed to GetDBSizeFromEnv: " << s.ToString();

  return backing_store->GetInMemoryBlobSize() + level_db_size;
}

base::Time IndexedDBFactoryImpl::GetLastModified(
    const url::Origin& origin) const {
  const auto& it = backing_store_map_.find(origin);
  DCHECK(it != backing_store_map_.end());

  const scoped_refptr<IndexedDBBackingStore>& backing_store = it->second;
  return backing_store->db()->LastModified();
}

void IndexedDBFactoryImpl::NotifyIndexedDBContentChanged(
    const url::Origin& origin,
    const base::string16& database_name,
    const base::string16& object_store_name) {
  if (!context_)
    return;
  context_->NotifyIndexedDBContentChanged(origin, database_name,
                                          object_store_name);
}

}  // namespace content
