// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_CONTEXT_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_CONTEXT_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "content/browser/indexed_db/indexed_db_bucket_context_handle.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace storage {
class QuotaManagerProxy;
}

namespace content {
class IndexedDBBackingStore;
class IndexedDBDatabase;
class IndexedDBFactory;
class IndexedDBPreCloseTaskQueue;
class TransactionalLevelDBFactory;

constexpr const char kIDBCloseImmediatelySwitch[] = "idb-close-immediately";

// IndexedDBBucketContext manages the per-bucket IndexedDB state, and other
// important context like the backing store and lock manager.
//
// IndexedDBBucketContext will keep itself alive while any of these is true:
// * There are handles referencing the factory,
// * There are outstanding blob references to this database's blob files, or
// * The factory is in an incognito profile.
//
// When these qualities are no longer true, `RunTasks()` will return
// `kCanBeDestroyed` which lets the owning `IndexedDBFactory` know it's time to
// delete this object.
//
// TODO(crbug.com/1474996): it's intended that each bucket gets its own
// IndexedDB task runner. To facilitate IndexedDB code running on multiple task
// runners, `IndexedDBBucketContext` is in the process of becoming the single
// point of communication between classes running on the main task runner, such
// as `IndexedDBFactory`, and those that pertain to a specific bucket and
// therefore run on a bucket's IDB task runner, such as `IndexedDBDatabase` or
// `IndexedDBCursor`.
class CONTENT_EXPORT IndexedDBBucketContext {
 public:
  using DBMap =
      base::flat_map<std::u16string, std::unique_ptr<IndexedDBDatabase>>;

  // Represents a method of `IndexedDBBucketContext` which is not yet bound to a
  // particular instance of `IndexedDBBucketContext`. This is used for the
  // `for_each_bucket_context` delegate callback.
  using InstanceClosure =
      base::RepeatingCallback<void(IndexedDBBucketContext&)>;

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
    // IndexedDBBucketContext isn't closing.
    kNotClosing,
    // IndexedDBBucketContext is pausing for kBackingStoreGracePeriodSeconds
    // to allow new references to open before closing the backing store.
    kPreCloseGracePeriod,
    // The `pre_close_task_queue` is running any pre-close tasks.
    kRunningPreCloseTasks,
    kClosed,
  };

  // This structure defines the interface between `IndexedDBBucketContext` and
  // the broader context that exists per Storage Partition (i.e.
  // BrowserContext).
  // TODO(crbug.com/1474996): for now these callbacks execute on the current
  // sequence, but in the future they should be bound to the main IDB sequence.
  struct CONTENT_EXPORT Delegate {
    Delegate();
    Delegate(Delegate&&);
    ~Delegate();

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Called to pump the IDB task queue (generally results in `RunTasks()`
    // being called).
    base::RepeatingClosure on_tasks_available;

    // Called when a fatal error has occurred that should result in tearing down
    // the backing store. `IndexedDBBucketContext` *may* be synchronously
    // destroyed after this is invoked.
    base::RepeatingCallback<void(leveldb::Status)> on_fatal_error;

    // Called when database content has changed.
    base::RepeatingCallback<void(const std::u16string& /*database_name*/,
                                 const std::u16string& /*object_store_name*/)>
        on_content_changed;

    // Called to run a given callback on every bucket context (including the one
    // in the current sequence and those in other sequences/associated with
    // other buckets). This method will also be called on every subsequently
    // created bucket context (see `initialization_closure` in constructor),
    // until it is replaced by another initialization closure.
    base::RepeatingCallback<void(InstanceClosure)> for_each_bucket_context;
  };

  // If non-null, `initialization_closure` is immediately run on `this`. If it
  // is null, `this` will generate a new initialization closure and return it to
  // the delegate via `for_each_bucket_context`. The delegate, i.e.
  // `IDBFactory`, will pass a null `InstanceClosure` to the first
  // `IndexedDBBucketContext` it creates.
  IndexedDBBucketContext(
      storage::BucketInfo bucket_info,
      bool persist_for_incognito,
      base::Clock* clock,
      TransactionalLevelDBFactory* transactional_leveldb_factory,
      std::unique_ptr<PartitionedLockManager> lock_manager,
      Delegate&& delegate,
      std::unique_ptr<IndexedDBBackingStore> backing_store,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      InstanceClosure initialization_closure);

  IndexedDBBucketContext(const IndexedDBBucketContext&) = delete;
  IndexedDBBucketContext& operator=(const IndexedDBBucketContext&) = delete;

  ~IndexedDBBucketContext();

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

  // Runs `method` on `this`. This exists to facilitate running the setter on
  // the correct sequence.
  void RunInstanceClosure(InstanceClosure method);

  const storage::BucketInfo& bucket_info() { return bucket_info_; }
  storage::BucketLocator bucket_locator() {
    return bucket_info_.ToBucketLocator();
  }
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
  const PartitionedLockManager* lock_manager() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return lock_manager_.get();
  }
  IndexedDBPreCloseTaskQueue* pre_close_task_queue() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return pre_close_task_queue_.get();
  }

  Delegate& delegate() { return delegate_; }

  bool is_running_tasks() const { return running_tasks_; }
  bool is_task_run_scheduled() const { return task_run_scheduled_; }
  void set_task_run_scheduled() { task_run_scheduled_ = true; }

  base::OneShotTimer* close_timer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &close_timer_;
  }

  enum class RunTasksResult { kDone, kError, kCanBeDestroyed };
  std::tuple<RunTasksResult, leveldb::Status> RunTasks();

  base::WeakPtr<IndexedDBBucketContext> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  storage::QuotaManagerProxy* quota_manager() {
    return quota_manager_proxy_.get();
  }

 private:
  friend IndexedDBFactory;
  friend IndexedDBBucketContextHandle;

  // Test needs access to ShouldRunTombstoneSweeper.
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTestWithMockTime,
                           TombstoneSweeperTiming);

  // Test needs access to ShouldRunCompaction.
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTestWithMockTime,
                           CompactionTaskTiming);

  // Test needs access to CompactionKillSwitchWorks.
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, CompactionKillSwitchWorks);

  // Used to synchronize the global throttling of LevelDB cleanup operations.
  // See `for_each_bucket_context`.
  static void SetInternalState(base::Time earliest_global_sweep_time,
                               base::Time earliest_global_compaction_time,
                               IndexedDBBucketContext& context);

  IndexedDBDatabase* AddDatabase(const std::u16string& name,
                                 std::unique_ptr<IndexedDBDatabase> database);

  void OnHandleCreated();
  void OnHandleDestruction();

  // Returns true if this factory can be closed (no references, no blobs, and
  // not persisting for incognito).
  bool CanClose();

  void MaybeStartClosing();
  void StartClosing();
  void CloseNow();
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

  storage::BucketInfo bucket_info_;

  // True if this factory should be remain alive due to the storage partition
  // being for incognito mode, and our backing store being in-memory. This is
  // used as closing criteria for this object, see CanClose.
  bool persist_for_incognito_;
  // True if there are blobs referencing this backing store that are still
  // alive. This is used as closing criteria for this object, see
  // CanClose.
  bool has_blobs_outstanding_ = false;
  bool skip_closing_sequence_ = false;
  const raw_ptr<base::Clock> clock_;
  const raw_ptr<TransactionalLevelDBFactory> transactional_leveldb_factory_;

  bool running_tasks_ = false;
  bool task_run_scheduled_ = false;

  base::Time earliest_global_sweep_time_;
  base::Time earliest_global_compaction_time_;
  ClosingState closing_stage_ = ClosingState::kNotClosing;
  base::OneShotTimer close_timer_;
  const std::unique_ptr<PartitionedLockManager> lock_manager_;
  std::unique_ptr<IndexedDBBackingStore> backing_store_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  DBMap databases_;
  // This is the refcount for the number of IndexedDBBucketContextHandle's
  // given out for this factory using OpenReference. This is used as closing
  // criteria for this object, see CanClose.
  int64_t open_handles_ = 0;

  std::unique_ptr<IndexedDBPreCloseTaskQueue> pre_close_task_queue_;

  Delegate delegate_;

  base::WeakPtrFactory<IndexedDBBucketContext> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_CONTEXT_H_
