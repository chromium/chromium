// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_storage_key_state.h"

#include <list>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"
#include "content/browser/indexed_db/indexed_db_active_blob_registry.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_compaction_task.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
#include "content/browser/indexed_db/indexed_db_tombstone_sweeper.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {
namespace {
// Time after the last connection to a database is closed and when we destroy
// the backing store.
const int64_t kBackingStoreGracePeriodSeconds = 2;
// Total time we let pre-close tasks run.
const int64_t kRunningPreCloseTasksMaxRunPeriodSeconds = 60;
// The number of iterations for every 'round' of the tombstone sweeper.
const int kTombstoneSweeperRoundIterations = 1000;
// The maximum total iterations for the tombstone sweeper.
const int kTombstoneSweeperMaxIterations = 10 * 1000 * 1000;

constexpr const base::TimeDelta kMinEarliestStorageKeySweepFromNow =
    base::Days(1);
static_assert(kMinEarliestStorageKeySweepFromNow <
                  IndexedDBStorageKeyState::kMaxEarliestStorageKeySweepFromNow,
              "Min < Max");

constexpr const base::TimeDelta kMinEarliestGlobalSweepFromNow =
    base::Minutes(5);
static_assert(kMinEarliestGlobalSweepFromNow <
                  IndexedDBStorageKeyState::kMaxEarliestGlobalSweepFromNow,
              "Min < Max");

base::Time GenerateNextStorageKeySweepTime(base::Time now) {
  uint64_t range = IndexedDBStorageKeyState::kMaxEarliestStorageKeySweepFromNow
                       .InMilliseconds() -
                   kMinEarliestStorageKeySweepFromNow.InMilliseconds();
  int64_t rand_millis = kMinEarliestStorageKeySweepFromNow.InMilliseconds() +
                        static_cast<int64_t>(base::RandGenerator(range));
  return now + base::Milliseconds(rand_millis);
}

base::Time GenerateNextGlobalSweepTime(base::Time now) {
  uint64_t range = IndexedDBStorageKeyState::kMaxEarliestGlobalSweepFromNow
                       .InMilliseconds() -
                   kMinEarliestGlobalSweepFromNow.InMilliseconds();
  int64_t rand_millis = kMinEarliestGlobalSweepFromNow.InMilliseconds() +
                        static_cast<int64_t>(base::RandGenerator(range));
  return now + base::Milliseconds(rand_millis);
}

constexpr const base::TimeDelta kMinEarliestStorageKeyCompactionFromNow =
    base::Days(1);
static_assert(
    kMinEarliestStorageKeyCompactionFromNow <
        IndexedDBStorageKeyState::kMaxEarliestStorageKeyCompactionFromNow,
    "Min < Max");

constexpr const base::TimeDelta kMinEarliestGlobalCompactionFromNow =
    base::Minutes(5);
static_assert(kMinEarliestGlobalCompactionFromNow <
                  IndexedDBStorageKeyState::kMaxEarliestGlobalCompactionFromNow,
              "Min < Max");

base::Time GenerateNextStorageKeyCompactionTime(base::Time now) {
  uint64_t range =
      IndexedDBStorageKeyState::kMaxEarliestStorageKeyCompactionFromNow
          .InMilliseconds() -
      kMinEarliestStorageKeyCompactionFromNow.InMilliseconds();
  int64_t rand_millis =
      kMinEarliestStorageKeyCompactionFromNow.InMilliseconds() +
      static_cast<int64_t>(base::RandGenerator(range));
  return now + base::Milliseconds(rand_millis);
}

base::Time GenerateNextGlobalCompactionTime(base::Time now) {
  uint64_t range = IndexedDBStorageKeyState::kMaxEarliestGlobalCompactionFromNow
                       .InMilliseconds() -
                   kMinEarliestGlobalCompactionFromNow.InMilliseconds();
  int64_t rand_millis = kMinEarliestGlobalCompactionFromNow.InMilliseconds() +
                        static_cast<int64_t>(base::RandGenerator(range));
  return now + base::Milliseconds(rand_millis);
}

}  // namespace

const base::Feature kCompactIDBOnClose{"CompactIndexedDBOnClose",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

constexpr const base::TimeDelta
    IndexedDBStorageKeyState::kMaxEarliestGlobalSweepFromNow;
constexpr const base::TimeDelta
    IndexedDBStorageKeyState::kMaxEarliestStorageKeySweepFromNow;

constexpr const base::TimeDelta
    IndexedDBStorageKeyState::kMaxEarliestGlobalCompactionFromNow;
constexpr const base::TimeDelta
    IndexedDBStorageKeyState::kMaxEarliestStorageKeyCompactionFromNow;

IndexedDBStorageKeyState::IndexedDBStorageKeyState(
    blink::StorageKey storage_key,
    bool persist_for_incognito,
    base::Clock* clock,
    TransactionalLevelDBFactory* transactional_leveldb_factory,
    base::Time* earliest_global_sweep_time,
    base::Time* earliest_global_compaction_time,
    std::unique_ptr<DisjointRangeLockManager> lock_manager,
    TasksAvailableCallback notify_tasks_callback,
    TearDownCallback tear_down_callback,
    std::unique_ptr<IndexedDBBackingStore> backing_store)
    : storage_key_(std::move(storage_key)),
      persist_for_incognito_(persist_for_incognito),
      clock_(clock),
      transactional_leveldb_factory_(transactional_leveldb_factory),
      earliest_global_sweep_time_(earliest_global_sweep_time),
      earliest_global_compaction_time_(earliest_global_compaction_time),
      lock_manager_(std::move(lock_manager)),
      backing_store_(std::move(backing_store)),
      notify_tasks_callback_(std::move(notify_tasks_callback)),
      tear_down_callback_(std::move(tear_down_callback)) {
  DCHECK(clock_);
  DCHECK(earliest_global_sweep_time_);
  if (*earliest_global_sweep_time_ == base::Time())
    *earliest_global_sweep_time_ = GenerateNextGlobalSweepTime(clock_->Now());
  DCHECK(earliest_global_compaction_time_);
  if (*earliest_global_compaction_time_ == base::Time())
    *earliest_global_compaction_time_ =
        GenerateNextGlobalCompactionTime(clock_->Now());
}

IndexedDBStorageKeyState::~IndexedDBStorageKeyState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backing_store_)
    return;
  if (backing_store_->IsBlobCleanupPending())
    backing_store_->ForceRunBlobCleanup();

  base::WaitableEvent leveldb_destruct_event;
  backing_store_->db()->leveldb_state()->RequestDestruction(
      &leveldb_destruct_event);
  backing_store_.reset();
  leveldb_destruct_event.Wait();
}

void IndexedDBStorageKeyState::AbortAllTransactions(bool compact) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Because finishing all transactions could cause a database to be destructed
  // (which would mutate the database_ map), save the keys beforehand and use
  // those.
  std::vector<std::u16string> database_names;
  database_names.reserve(databases_.size());
  for (const auto& pair : databases_) {
    database_names.push_back(pair.first);
  }

  base::WeakPtr<IndexedDBStorageKeyState> weak_ptr = AsWeakPtr();
  for (const std::u16string& database_name : database_names) {
    auto it = databases_.find(database_name);
    if (it == databases_.end())
      continue;

    // Calling FinishAllTransactions can destruct the IndexedDBConnection &
    // modify the IndexedDBDatabase::connection() list. To prevent UAFs, start
    // by taking a WeakPtr of all connections, and then iterate that list.
    std::vector<base::WeakPtr<IndexedDBConnection>> weak_connections;
    weak_connections.reserve(it->second->connections().size());
    for (IndexedDBConnection* connection : it->second->connections())
      weak_connections.push_back(connection->GetWeakPtr());

    for (base::WeakPtr<IndexedDBConnection> connection : weak_connections) {
      if (connection) {
        leveldb::Status status =
            connection->AbortAllTransactions(IndexedDBDatabaseError(
                blink::mojom::IDBException::kUnknownError,
                "Aborting all transactions for the origin."));
        if (!status.ok()) {
          // This call should delete this object.
          tear_down_callback().Run(status);
          return;
        }
      }
    }
  }

  if (compact)
    backing_store_->Compact();
}

void IndexedDBStorageKeyState::ForceClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // To avoid re-entry, the |db_destruction_weak_factory_| is invalidated so
  // that of the deletions closures returned by CreateDatabaseDeleteClosure will
  // no-op. This allows force closing all of the databases without having the
  // map mutate. Afterwards the map is manually deleted.
  IndexedDBStorageKeyStateHandle handle = CreateHandle();
  for (const auto& pair : databases_) {
    // Note: We purposefully ignore the result here as force close needs to
    // continue tearing things down anyways.
    pair.second->ForceCloseAndRunTasks();
  }
  databases_.clear();
  if (has_blobs_outstanding_) {
    backing_store_->active_blob_registry()->ForceShutdown();
    has_blobs_outstanding_ = false;
  }

  // Don't run the preclosing tasks after a ForceClose, whether or not we've
  // started them.  Compaction in particular can run long and cannot be
  // interrupted, so it can cause shutdown hangs.
  close_timer_.AbandonAndStop();
  if (pre_close_task_queue_) {
    pre_close_task_queue_->Stop(
        IndexedDBPreCloseTaskQueue::StopReason::FORCE_CLOSE);
    pre_close_task_queue_.reset();
  }
  skip_closing_sequence_ = true;
}

void IndexedDBStorageKeyState::ReportOutstandingBlobs(bool blobs_outstanding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_blobs_outstanding_ = blobs_outstanding;
  MaybeStartClosing();
}

void IndexedDBStorageKeyState::StopPersistingForIncognito() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  persist_for_incognito_ = false;
  MaybeStartClosing();
}

std::tuple<IndexedDBStorageKeyState::RunTasksResult, leveldb::Status>
IndexedDBStorageKeyState::RunTasks() {
  task_run_scheduled_ = false;
  running_tasks_ = true;
  leveldb::Status status;
  for (auto db_it = databases_.begin(); db_it != databases_.end();) {
    IndexedDBDatabase& db = *db_it->second;

    IndexedDBDatabase::RunTasksResult tasks_result;
    std::tie(tasks_result, status) = db.RunTasks();
    switch (tasks_result) {
      case IndexedDBDatabase::RunTasksResult::kDone:
        ++db_it;
        continue;
      case IndexedDBDatabase::RunTasksResult::kError:
        running_tasks_ = false;
        return {RunTasksResult::kError, status};
      case IndexedDBDatabase::RunTasksResult::kCanBeDestroyed:
        db_it = databases_.erase(db_it);
        break;
    }
  }
  running_tasks_ = false;
  if (CanCloseFactory() && closing_stage_ == ClosingState::kClosed)
    return {RunTasksResult::kCanBeDestroyed, leveldb::Status::OK()};
  return {RunTasksResult::kDone, leveldb::Status::OK()};
}

IndexedDBDatabase* IndexedDBStorageKeyState::AddDatabase(
    const std::u16string& name,
    std::unique_ptr<IndexedDBDatabase> database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(databases_, name));
  return databases_.emplace(name, std::move(database)).first->second.get();
}

IndexedDBStorageKeyStateHandle IndexedDBStorageKeyState::CreateHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++open_handles_;
  if (closing_stage_ != ClosingState::kNotClosing) {
    closing_stage_ = ClosingState::kNotClosing;
    close_timer_.AbandonAndStop();
    if (pre_close_task_queue_) {
      pre_close_task_queue_->Stop(
          IndexedDBPreCloseTaskQueue::StopReason::NEW_CONNECTION);
      pre_close_task_queue_.reset();
    }
  }
  return IndexedDBStorageKeyStateHandle(weak_factory_.GetWeakPtr());
}

void IndexedDBStorageKeyState::OnHandleDestruction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(open_handles_, 0ll);
  --open_handles_;
  MaybeStartClosing();
}

bool IndexedDBStorageKeyState::CanCloseFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(open_handles_, 0);
  return !has_blobs_outstanding_ && open_handles_ <= 0 &&
         !persist_for_incognito_;
}

void IndexedDBStorageKeyState::MaybeStartClosing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsClosing() && CanCloseFactory())
    StartClosing();
}

void IndexedDBStorageKeyState::StartClosing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CanCloseFactory());
  DCHECK(!IsClosing());

  if (skip_closing_sequence_ ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          kIDBCloseImmediatelySwitch)) {
    closing_stage_ = ClosingState::kClosed;
    close_timer_.AbandonAndStop();
    pre_close_task_queue_.reset();
    notify_tasks_callback_.Run();
    return;
  }

  // Start a timer to close the backing store, unless something else opens it
  // in the mean time.
  DCHECK(!close_timer_.IsRunning());
  closing_stage_ = ClosingState::kPreCloseGracePeriod;
  close_timer_.Start(
      FROM_HERE, base::Seconds(kBackingStoreGracePeriodSeconds),
      base::BindOnce(
          [](base::WeakPtr<IndexedDBStorageKeyState> factory) {
            if (!factory ||
                factory->closing_stage_ != ClosingState::kPreCloseGracePeriod)
              return;
            factory->StartPreCloseTasks();
          },
          weak_factory_.GetWeakPtr()));
}

void IndexedDBStorageKeyState::StartPreCloseTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(closing_stage_ == ClosingState::kPreCloseGracePeriod);
  closing_stage_ = ClosingState::kRunningPreCloseTasks;

  // The callback will run on all early returns in this function.
  base::ScopedClosureRunner maybe_close_backing_store_runner(base::BindOnce(
      [](base::WeakPtr<IndexedDBStorageKeyState> factory) {
        if (!factory ||
            factory->closing_stage_ != ClosingState::kRunningPreCloseTasks)
          return;
        factory->closing_stage_ = ClosingState::kClosed;
        factory->pre_close_task_queue_.reset();
        factory->close_timer_.AbandonAndStop();
        factory->notify_tasks_callback_.Run();
      },
      weak_factory_.GetWeakPtr()));

  std::list<std::unique_ptr<IndexedDBPreCloseTaskQueue::PreCloseTask>> tasks;

  if (ShouldRunTombstoneSweeper()) {
    tasks.push_back(std::make_unique<IndexedDBTombstoneSweeper>(
        kTombstoneSweeperRoundIterations, kTombstoneSweeperMaxIterations,
        backing_store_->db()->db()));
  }

  if (ShouldRunCompaction()) {
    tasks.push_back(
        std::make_unique<IndexedDBCompactionTask>(backing_store_->db()->db()));
  }

  if (!tasks.empty()) {
    pre_close_task_queue_ = std::make_unique<IndexedDBPreCloseTaskQueue>(
        std::move(tasks), maybe_close_backing_store_runner.Release(),
        base::Seconds(kRunningPreCloseTasksMaxRunPeriodSeconds),
        std::make_unique<base::OneShotTimer>());
    pre_close_task_queue_->Start(
        base::BindOnce(&IndexedDBBackingStore::GetCompleteMetadata,
                       base::Unretained(backing_store_.get())));
  }
}

bool IndexedDBStorageKeyState::ShouldRunTombstoneSweeper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time now = clock_->Now();
  // Check that the last sweep hasn't run too recently.
  if (*earliest_global_sweep_time_ > now)
    return false;

  base::Time storage_key_earliest_sweep;
  leveldb::Status s = indexed_db::GetEarliestSweepTime(
      backing_store_->db(), &storage_key_earliest_sweep);
  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok() && !s.IsNotFound())
    return false;

  if (storage_key_earliest_sweep > now)
    return false;

  // A sweep will happen now, so reset the sweep timers.
  *earliest_global_sweep_time_ = GenerateNextGlobalSweepTime(now);
  std::unique_ptr<LevelDBDirectTransaction> txn =
      transactional_leveldb_factory_->CreateLevelDBDirectTransaction(
          backing_store_->db());
  s = indexed_db::SetEarliestSweepTime(txn.get(),
                                       GenerateNextStorageKeySweepTime(now));
  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok())
    return false;
  s = txn->Commit();

  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok())
    return false;
  return true;
}

bool IndexedDBStorageKeyState::ShouldRunCompaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(kCompactIDBOnClose))
    return false;

  base::Time now = clock_->Now();
  // Check that the last compaction hasn't run too recently.
  if (*earliest_global_compaction_time_ > now)
    return false;

  base::Time storage_key_earliest_compaction;
  leveldb::Status s = indexed_db::GetEarliestCompactionTime(
      backing_store_->db(), &storage_key_earliest_compaction);
  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok() && !s.IsNotFound())
    return false;

  if (storage_key_earliest_compaction > now)
    return false;

  // A compaction will happen now, so reset the compaction timers.
  *earliest_global_compaction_time_ = GenerateNextGlobalCompactionTime(now);
  std::unique_ptr<LevelDBDirectTransaction> txn =
      transactional_leveldb_factory_->CreateLevelDBDirectTransaction(
          backing_store_->db());
  s = indexed_db::SetEarliestCompactionTime(
      txn.get(), GenerateNextStorageKeyCompactionTime(now));
  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok())
    return false;
  s = txn->Commit();

  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok())
    return false;
  return true;
}

}  // namespace content
