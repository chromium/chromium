// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_STATE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_STATE_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_bucket_state_handle.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {
class IndexedDBBackingStore;
class IndexedDBDatabase;
class IndexedDBFactoryImpl;
class IndexedDBPreCloseTaskQueue;
class TransactionalLevelDBFactory;

constexpr const char kIDBCloseImmediatelySwitch[] = "idb-close-immediately";

// This is an emergency kill switch to use with Finch if the feature needs to be
// shut off.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCompactIDBOnClose);

// IndexedDBBucketState manages the per-bucket IndexedDB state, and
// contains the backing store for the bucket.
//
// This class is expected to manage its own lifetime by using the
// `destruct_myself_` closure, which is expected to destroy this object in the
// parent IndexedDBFactoryImpl (and remove it from any collections, etc).
// However, IndexedDBBucketState should still handle destruction without the
// use of that closure when the storage partition is destructed.
//
// IndexedDBBucketState will keep itself alive while:
// * There are handles referencing the factory,
// * There are outstanding blob references to this database's blob files, and
// * The factory is in an incognito profile.
class CONTENT_EXPORT IndexedDBBucketState {
 public:
  using TearDownCallback = base::RepeatingCallback<void(leveldb::Status)>;
  using DBMap =
      base::flat_map<std::u16string, std::unique_ptr<IndexedDBDatabase>>;

  // Maximum time interval between runs of the IndexedDBSweeper. Sweeping only
  // occurs after backing store close.
  // Visible for testing.
  static constexpr const base::TimeDelta kMaxEarliestGlobalSweepFromNow =
      base::Hours(1);
  // Maximum time interval between runs of the IndexedDBSweeper for a given
  // bucket. Sweeping only occurs after backing store close.
  // Visible for testing.
  static constexpr const base::TimeDelta kMaxEarliestBucketSweepFromNow =
      base::Days(3);

  // Maximum time interval between runs of the IndexedDBCompactionTask.
  // Compaction only occurs after backing store close.
  // Visible for testing.
  static constexpr const base::TimeDelta kMaxEarliestGlobalCompactionFromNow =
      base::Hours(1);
  // Maximum time interval between runs of the IndexedDBCompactionTask for a
  // given bucket. Compaction only occurs after backing store close.
  // Visible for testing.
  static constexpr const base::TimeDelta kMaxEarliestBucketCompactionFromNow =
      base::Days(3);

  enum class ClosingState {
    // IndexedDBBucketState isn't closing.
    kNotClosing,
    // IndexedDBBucketState is pausing for kBackingStoreGracePeriodSeconds
    // to allow new references to open before closing the backing store.
    kPreCloseGracePeriod,
    // The `pre_close_task_queue` is running any pre-close tasks.
    kRunningPreCloseTasks,
    kClosed,
  };

  // Calling `destruct_myself` should destruct this object.
  // `earliest_global_sweep_time` and `earliest_global_compaction_time` are
  // expected to outlive this object.
  IndexedDBBucketState(
      storage::BucketLocator bucket_locator,
      bool persist_for_incognito,
      base::Clock* clock,
      TransactionalLevelDBFactory* transactional_leveldb_factory,
      base::Time* earliest_global_sweep_time,
      base::Time* earliest_global_compaction_time,
      std::unique_ptr<PartitionedLockManager> lock_manager,
      TasksAvailableCallback notify_tasks_callback,
      TearDownCallback tear_down_callback,
      std::unique_ptr<IndexedDBBackingStore> backing_store);

  IndexedDBBucketState(const IndexedDBBucketState&) = delete;
  IndexedDBBucketState& operator=(const IndexedDBBucketState&) = delete;

  ~IndexedDBBucketState();

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

  const storage::BucketLocator& bucket_locator() { return bucket_locator_; }
  IndexedDBBackingStore* backing_store() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return backing_store_.get();
  }
  const DBMap& databases() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return databases_;
  }
  PartitionedLockManager* lock_manager() {
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

  // Note: calling this callback will destroy the IndexedDBBucketState.
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

  base::WeakPtr<IndexedDBBucketState> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend IndexedDBFactoryImpl;
  friend IndexedDBBucketStateHandle;

  // Test needs access to ShouldRunTombstoneSweeper.
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTestWithMockTime,
                           TombstoneSweeperTiming);

  // Test needs access to ShouldRunCompaction.
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTestWithMockTime,
                           CompactionTaskTiming);

  // Test needs access to CompactionKillSwitchWorks.
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, CompactionKillSwitchWorks);

  IndexedDBDatabase* AddDatabase(const std::u16string& name,
                                 std::unique_ptr<IndexedDBDatabase> database);

  // Returns a new handle to this factory. If this object was in its closing
  // sequence, then that sequence will be halted by this call.
  [[nodiscard]] IndexedDBBucketStateHandle CreateHandle();

  void OnHandleDestruction();

  // Returns true if this factory can be closed (no references, no blobs, and
  // not persisting for incognito).
  bool CanCloseFactory();

  void MaybeStartClosing();
  void StartClosing();
  void StartPreCloseTasks();

  // Executes database operations, and if `true` is returned by this function,
  // then the current time will be written to the database as the last sweep
  // time.
  bool ShouldRunTombstoneSweeper();

  // Executes database operations, and if `true` is returned by this function,
  // then the current time will be written to the database as the last
  // compaction time.
  bool ShouldRunCompaction();

  SEQUENCE_CHECKER(sequence_checker_);

  storage::BucketLocator bucket_locator_;

  // True if this factory should be remain alive due to the storage partition
  // being for incognito mode, and our backing store being in-memory. This is
  // used as closing criteria for this object, see CanCloseFactory.
  bool persist_for_incognito_;
  // True if there are blobs referencing this backing store that are still
  // alive. This is used as closing criteria for this object, see
  // CanCloseFactory.
  bool has_blobs_outstanding_ = false;
  bool skip_closing_sequence_ = false;
  const raw_ptr<base::Clock> clock_;
  const raw_ptr<TransactionalLevelDBFactory> transactional_leveldb_factory_;

  bool running_tasks_ = false;
  bool task_run_scheduled_ = false;

  // This is safe because it is owned by IndexedDBFactoryImpl, which owns this
  // object.
  raw_ptr<base::Time> earliest_global_sweep_time_;
  raw_ptr<base::Time> earliest_global_compaction_time_;
  ClosingState closing_stage_ = ClosingState::kNotClosing;
  base::OneShotTimer close_timer_;
  const std::unique_ptr<PartitionedLockManager> lock_manager_;
  std::unique_ptr<IndexedDBBackingStore> backing_store_;

  DBMap databases_;
  // This is the refcount for the number of IndexedDBBucketStateHandle's
  // given out for this factory using OpenReference. This is used as closing
  // criteria for this object, see CanCloseFactory.
  int64_t open_handles_ = 0;

  std::unique_ptr<IndexedDBPreCloseTaskQueue> pre_close_task_queue_;

  TasksAvailableCallback notify_tasks_callback_;
  TearDownCallback tear_down_callback_;

  base::WeakPtrFactory<IndexedDBBucketState> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_STATE_H_
