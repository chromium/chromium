// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_CONTEXT_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_CONTEXT_H_

#include <stdint.h>
#include <memory>
#include <queue>
#include <string>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom.h"
#include "content/browser/indexed_db/indexed_db_bucket_context_handle.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace storage {
class QuotaManagerProxy;
}

namespace content {
class IndexedDBBackingStore;
class IndexedDBDatabase;
class IndexedDBDataItemReader;
class IndexedDBFactory;
class IndexedDBPreCloseTaskQueue;

constexpr const char kIDBCloseImmediatelySwitch[] = "idb-close-immediately";

// IndexedDBBucketContext manages the per-bucket IndexedDB state, and other
// important context like the backing store and lock manager.
//
// IndexedDBBucketContext will keep itself alive while any of these is true:
// * There are handles referencing the factory,
// * There are outstanding blob references to this database's blob files, or
// * The factory is in-memory (i.e. an incognito profile).
//
// When these qualities are no longer true, `RunTasks()` will invoke
// `on_ready_for_destruction`, which lets the owner (`IndexedDBFactory`) know
// it's time to destroy this.
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

  // `CheckCanUseDiskSpace` fudges quota values a little. If there is excess
  // free space, QuotaManager may not be checked the next time a transaction
  // requests space. The decays over this time period.
  static constexpr const base::TimeDelta kBucketSpaceCacheTimeLimit =
      base::Seconds(30);

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

    // Called when a fatal error has occurred that should result in tearing down
    // the backing store. `IndexedDBBucketContext` *may* be synchronously
    // destroyed after this is invoked.
    base::RepeatingCallback<void(leveldb::Status)> on_fatal_error;

    // Called when the bucket context is ready to be destroyed.
    base::RepeatingCallback<void()> on_ready_for_destruction;

    // Called when database content has changed. Technically this is called when
    // the content *probably will* change --- it's invoked before a transaction
    // is actually committed --- but it's only used for devtools so accuracy
    // isn't that important.
    base::RepeatingCallback<void(const std::u16string& /*database_name*/,
                                 const std::u16string& /*object_store_name*/)>
        on_content_changed;

    // Called to inform the quota system that a transaction which may have
    // updated the amount of disk space used has completed. The parameter is
    // true for transactions that caused the backing store to flush.
    base::RepeatingCallback<void(bool /*did_sync*/)>
        on_writing_transaction_complete;

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
      std::unique_ptr<PartitionedLockManager> lock_manager,
      Delegate&& delegate,
      std::unique_ptr<IndexedDBBackingStore> backing_store,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<base::TaskRunner> io_task_runner,
      mojo::PendingRemote<storage::mojom::BlobStorageContext>
          blob_storage_context,
      mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
          file_system_access_context,
      InstanceClosure initialization_closure);

  IndexedDBBucketContext(const IndexedDBBucketContext&) = delete;
  IndexedDBBucketContext& operator=(const IndexedDBBucketContext&) = delete;

  ~IndexedDBBucketContext();

  void QueueRunTasks();

  // Normally, in-memory bucket contexts never self-close. If this is called
  // with `doom` set to true, they will self-close.
  void ForceClose(bool doom);

  bool IsClosing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return closing_stage_ != ClosingState::kNotClosing;
  }

  ClosingState closing_stage() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return closing_stage_;
  }

  void ReportOutstandingBlobs(bool blobs_outstanding);

  // Runs `method` on `this`. This exists to facilitate running the setter on
  // the correct sequence.
  void RunInstanceClosure(InstanceClosure method);

  // Called when `space_requested` bytes are about to be used by committing a
  // transaction. Will invoke `disk_space_check_callback` if this usage is
  // approved, or false if there's insufficient space as per the `QuotaManager`.
  // If `disk-space_check_callback` is non-null, it will be invoked with the
  // response. If it is null, the check is considered a dry-run, which warms up
  // the space cache but doesn't decrement from it.
  void CheckCanUseDiskSpace(
      int64_t space_requested,
      base::OnceCallback<void(bool)> disk_space_check_callback);

  // Create external objects from |objects| and store the results in
  // |mojo_objects|. |mojo_objects| must be the same length as |objects|.
  void CreateAllExternalObjects(
      const std::vector<IndexedDBExternalObject>& objects,
      std::vector<blink::mojom::IDBExternalObjectPtr>* mojo_objects);

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

  base::OneShotTimer* close_timer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &close_timer_;
  }

  base::WeakPtr<IndexedDBBucketContext> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  storage::QuotaManagerProxy* quota_manager() {
    return quota_manager_proxy_.get();
  }

  storage::mojom::BlobStorageContext* blob_storage_context() {
    return blob_storage_context_.get();
  }
  storage::mojom::FileSystemAccessContext* file_system_access_context() {
    return file_system_access_context_.get();
  }

 private:
  friend IndexedDBFactory;
  friend IndexedDBBucketContextHandle;
  friend class IndexedDBDatabaseTest;
  friend class IndexedDBTransactionTest;

  // Test needs access to ShouldRunTombstoneSweeper.
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, TombstoneSweeperTiming);

  // Test needs access to ShouldRunCompaction.
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, CompactionTaskTiming);

  // Test needs access to CompactionKillSwitchWorks.
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, CompactionKillSwitchWorks);

  FRIEND_TEST_ALL_PREFIXES(IndexedDBBucketContextTest, BucketSpaceDecay);

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

  void RunTasks();

  // Executes database operations, and if `true` is returned by this function,
  // then the current time will be written to the database as the last sweep
  // time.
  bool ShouldRunTombstoneSweeper();

  // Executes database operations, and if `true` is returned by this function,
  // then the current time will be written to the database as the last
  // compaction time.
  bool ShouldRunCompaction();

  void OnGotBucketSpaceRemaining(storage::QuotaErrorOr<int64_t> space_left);

  // Returns the amount of bucket space `this` has the authority to approve by
  // decaying `bucket_space_remaining_` according to the amount of time passed
  // since `bucket_space_remaining_timestamp_`.
  int64_t GetBucketSpaceToAllot();

  // Bind `receiver` to read from the file at `path`.
  void BindFileReader(
      const base::FilePath& path,
      base::Time expected_modification_time,
      base::OnceClosure release_callback,
      mojo::PendingReceiver<storage::mojom::BlobDataItemReader> receiver);
  // Removes all readers for this file path.
  void RemoveBoundReaders(const base::FilePath& path);

  SEQUENCE_CHECKER(sequence_checker_);

  storage::BucketInfo bucket_info_;

  // True if there are blobs referencing this backing store that are still
  // alive. This is used as closing criteria for this object, see
  // CanClose.
  bool has_blobs_outstanding_ = false;
  bool skip_closing_sequence_ = false;

  bool running_tasks_ = false;

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

  // A queue of callbacks representing `CheckCanUseDiskSpace()` requests.
  std::queue<std::tuple<int64_t /*space_requested*/,
                        base::OnceCallback<void(bool /*allowed*/)>>>
      bucket_space_check_callbacks_;
  // The number of bytes `this` has the authority to approve in response to
  // `CheckCanUseDiskSpace` requests before the QuotaManager will be consulted
  // once more. This is a performance optimization.
  int64_t bucket_space_remaining_ = 0;
  // Timestamp when `bucket_space_remaining_` was last fetched from the quota
  // manager.
  base::TimeTicks bucket_space_remaining_timestamp_;

  // Members in the following block are used for `CreateAllExternalObjects`.
  // Shared task runner used to read blob files on.
  const scoped_refptr<base::TaskRunner> file_task_runner_;
  // Shared task runner used for async I/O while reading blob files.
  const scoped_refptr<base::TaskRunner> io_task_runner_;
  // Mojo connection to `BlobStorageContext`, which runs on the IO thread.
  mojo::Remote<storage::mojom::BlobStorageContext> blob_storage_context_;
  // Mojo connection to `FileSystemAccessContextImpl`, which runs on the UI
  // thread.
  mojo::Remote<storage::mojom::FileSystemAccessContext>
      file_system_access_context_;
  std::map<base::FilePath, std::unique_ptr<IndexedDBDataItemReader>>
      file_reader_map_;

  std::unique_ptr<IndexedDBPreCloseTaskQueue> pre_close_task_queue_;

  Delegate delegate_;

  // In-memory contexts will not self-close until this bit is flipped to true.
  bool is_doomed_ = false;

  // True if there's already a task queued to call `RunTasks()`.
  bool task_run_queued_ = false;

  base::WeakPtrFactory<IndexedDBBucketContext> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_CONTEXT_H_
