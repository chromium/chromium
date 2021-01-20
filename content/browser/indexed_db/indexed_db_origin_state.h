// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_ORIGIN_STATE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_ORIGIN_STATE_H_

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "components/services/storage/indexed_db/scopes/disjoint_range_lock_manager.h"
#include "content/browser/indexed_db/indexed_db_origin_state_handle.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "url/origin.h"

namespace content {
class IndexedDBBackingStore;
class IndexedDBDatabase;
class IndexedDBFactoryImpl;
class IndexedDBPreCloseTaskQueue;
class TransactionalLevelDBFactory;

constexpr const char kIDBCloseImmediatelySwitch[] = "idb-close-immediately";

// This is an emergency kill switch to use with Finch if the feature needs to be
// shut off.
CONTENT_EXPORT extern const base::Feature kCompactIDBOnClose;

// IndexedDBOriginState manages the per-origin IndexedDB state, and contains the
// backing store for the origin.
//
// This class is expected to manage its own lifetime by using the
// |destruct_myself_| closure, which is expected to destroy this object in the
// parent IndexedDBFactoryImpl (and remove it from any collections, etc).
// However, IndexedDBOriginState should still handle destruction without the use
// of that closure when the storage partition is destructed.
//
// IndexedDBOriginState will keep itself alive while:
// * There are handles referencing the factory,
// * There are outstanding blob references to this database's blob files, and
// * The factory is in an incognito profile.
class CONTENT_EXPORT IndexedDBOriginState {
 public:
  using TearDownCallback = base::RepeatingCallback<void(leveldb::Status)>;
  using OriginDBMap =
      base::flat_map<base::string16, std::unique_ptr<IndexedDBDatabase>>;

  // Maximum time interval between runs of the IndexedDBSweeper. Sweeping only
  // occurs after backing store close.
  // Visible for testing.
  static constexpr const base::TimeDelta kMaxEarliestGlobalSweepFromNow =
      base::TimeDelta::FromHours(1);
  // Maximum time interval between runs of the IndexedDBSweeper for a given
  // origin. Sweeping only occurs after backing store close.
  // Visible for testing.
  static constexpr const base::TimeDelta kMaxEarliestOriginSweepFromNow =
      base::TimeDelta::FromDays(3);

  enum class ClosingState {
    // IndexedDBOriginState isn't closing.
    kNotClosing,
    // IndexedDBOriginState is pausing for kBackingStoreGracePeriodSeconds
    // to
    // allow new references to open before closing the backing store.
    kPreCloseGracePeriod,
    // The |pre_close_task_queue| is running any pre-close tasks.
    kRunningPreCloseTasks,
    kClosed,
  };

  // Calling |destruct_myself| should destruct this object.
  // |earliest_global_sweep_time| is expected to outlive this object.
  IndexedDBOriginState(
      url::Origin origin,
      bool persist_for_incognito,
      base::Clock* clock,
      TransactionalLevelDBFactory* transactional_leveldb_factory,
      base::Time* earliest_global_sweep_time,
      std::unique_ptr<DisjointRangeLockManager> lock_manager,
      TasksAvailableCallback notify_tasks_callback,
      TearDownCallback tear_down_callback,
      std::unique_ptr<IndexedDBBackingStore> backing_store);
  ~IndexedDBOriginState();

  void AbortAllTransactions(bool compact);

  void ForceClose();

  bool IsClosing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return closing_stage_ != ClosingState::kNotClosing;
  }

  ClosingState closing_stage() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return closing_stage_;
  }

  void ReportOutstandingBlobs(bool blobs_outstanding);

  void StopPersistingForIncognito();

  const url::Origin& origin() { return origin_; }
  IndexedDBBackingStore* backing_store() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return backing_store_.get();
  }
  const OriginDBMap& databases() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return databases_;
  }
  DisjointRangeLockManager* lock_manager() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return lock_manager_.get();
  }
  IndexedDBPreCloseTaskQueue* pre_close_task_queue() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return pre_close_task_queue_.get();
  }
  TasksAvailableCallback notify_tasks_callback() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return notify_tasks_callback_;
  }

  // Note: calling this callback will destroy the IndexedDBOriginState.
  const TearDownCallback& tear_down_callback() { return tear_down_callback_; }

  bool is_running_tasks() const { return running_tasks_; }
  bool is_task_run_scheduled() const { return task_run_scheduled_; }
  void set_task_run_scheduled() { task_run_scheduled_ = true; }

  base::OneShotTimer* close_timer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &close_timer_;
  }

  enum class RunTasksResult { kDone, kError, kCanBeDestroyed };
  std::tuple<RunTasksResult, leveldb::Status> RunTasks();

  base::WeakPtr<IndexedDBOriginState> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend IndexedDBFactoryImpl;
  friend IndexedDBOriginStateHandle;

  // Test needs access to ShouldRunTombstoneSweeper.
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTestWithMockTime,
                           TombstoneSweeperTiming);

  // Test needs access to CompactionKillSwitchWorks.
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, CompactionKillSwitchWorks);

  IndexedDBDatabase* AddDatabase(const base::string16& name,
                                 std::unique_ptr<IndexedDBDatabase> database);

  // Returns a new handle to this factory. If this object was in its closing
  // sequence, then that sequence will be halted by this call.
  IndexedDBOriginStateHandle CreateHandle() WARN_UNUSED_RESULT;

  void OnHandleDestruction();

  // Returns true if this factory can be closed (no references, no blobs, and
  // not persisting for incognito).
  bool CanCloseFactory();

  void MaybeStartClosing();
  void StartClosing();
  void StartPreCloseTasks();

  // Executes database operations, and if |true| is returned by this function,
  // then the current time will be written to the database as the last sweep
  // time.
  bool ShouldRunTombstoneSweeper();

  bool ShouldRunCompaction();

  SEQUENCE_CHECKER(sequence_checker_);

  url::Origin origin_;

  // True if this factory should be remain alive due to the storage partition
  // being for incognito mode, and our backing store being in-memory. This is
  // used as closing criteria for this object, see CanCloseFactory.
  bool persist_for_incognito_;
  // True if there are blobs referencing this backing store that are still
  // alive. This is used as closing criteria for this object, see
  // CanCloseFactory.
  bool has_blobs_outstanding_ = false;
  bool skip_closing_sequence_ = false;
  base::Clock* const clock_;
  TransactionalLevelDBFactory* const transactional_leveldb_factory_;

  bool running_tasks_ = false;
  bool task_run_scheduled_ = false;

  // This is safe because it is owned by IndexedDBFactoryImpl, which owns this
  // object.
  base::Time* earliest_global_sweep_time_;
  ClosingState closing_stage_ = ClosingState::kNotClosing;
  base::OneShotTimer close_timer_;
  const std::unique_ptr<DisjointRangeLockManager> lock_manager_;
  std::unique_ptr<IndexedDBBackingStore> backing_store_;

  OriginDBMap databases_;
  // This is the refcount for the number of IndexedDBOriginStateHandle's given
  // out for this factory using OpenReference. This is used as closing
  // criteria for this object, see CanCloseFactory.
  int64_t open_handles_ = 0;

  std::unique_ptr<IndexedDBPreCloseTaskQueue> pre_close_task_queue_;

  TasksAvailableCallback notify_tasks_callback_;
  TearDownCallback tear_down_callback_;

  base::WeakPtrFactory<IndexedDBOriginState> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IndexedDBOriginState);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_ORIGIN_STATE_H_
