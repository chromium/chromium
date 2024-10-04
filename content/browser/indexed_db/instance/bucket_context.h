// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_BUCKET_CONTEXT_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_BUCKET_CONTEXT_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_control_test.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_internals_types.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/status.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace storage {
class QuotaManagerProxy;
}

namespace content::indexed_db {

class BackingStore;
class BackingStorePreCloseTaskQueue;
class BucketContextHandle;
class Database;
class IndexedDBDataItemReader;

// BucketContext manages the per-bucket IndexedDB state, and other important
// context like the backing store and lock manager.
//
// BucketContext will keep its backing store around while any of these is true:
// * There are handles referencing the bucket context,
// * There are outstanding blob references to this database's blob files, or
// * The bucket context is in-memory (i.e. an incognito profile).
//
// When these qualities are no longer true, `RunTasks()` will invoke
// `ResetBackingStore()`, which returns `this` to an uninitialized state.
//
// Each instance of this class is created and run on a unique thread pool
// SequencedTaskRunner, i.e. the "bucket thread".
class CONTENT_EXPORT BucketContext
    : public blink::mojom::IDBFactory,
      public base::trace_event::MemoryDumpProvider {
 public:
  using DBMap = base::flat_map<std::u16string, std::unique_ptr<Database>>;

  // Represents a method of `BucketContext` which is not yet bound to a
  // particular instance of `BucketContext`. This is used for the
  // `for_each_bucket_context` delegate callback.
  using InstanceClosure = base::RepeatingCallback<void(BucketContext&)>;

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
    // BucketContext isn't closing.
    kNotClosing,
    // BucketContext is pausing for kBackingStoreGracePeriodSeconds
    // to allow new references to open before closing the backing store.
    kPreCloseGracePeriod,
    // The `pre_close_task_queue` is running any pre-close tasks.
    kRunningPreCloseTasks,
    kClosed,
  };

  // This structure defines the interface between `BucketContext` and the
  // broader context that exists per Storage Partition (i.e. BrowserContext).
  // TODO(crbug.com/40279485): for now these callbacks execute on the current
  // sequence, but in the future they should be bound to the main IDB sequence.
  struct CONTENT_EXPORT Delegate {
    Delegate();
    Delegate(Delegate&&);
    ~Delegate();

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Called when the bucket context is ready to be destroyed. After this is
    // called, the bucket context will no longer accept new IDBFactory
    // connections (receivers in `receivers_`).
    base::OnceCallback<void()> on_ready_for_destruction;

    // Called when `BucketContext` can't handle an `AddReceiver()` call:
    // specifically, if destruction has already been initiated by calling
    // `on_ready_for_destruction`.
    base::RepeatingCallback<void(
        const storage::BucketClientInfo& /*client_info*/,
        mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        /*client_state_checker_remote*/,
        mojo::PendingReceiver<blink::mojom::IDBFactory> /*pending_receiver*/)>
        on_receiver_bounced;

    // Called when database content has changed. Technically this is called when
    // the content *probably will* change --- it's invoked before a transaction
    // is actually committed --- but it's only used for devtools so accuracy
    // isn't that important.
    base::RepeatingCallback<void(const std::u16string& /*database_name*/,
                                 const std::u16string& /*object_store_name*/)>
        on_content_changed;

    // Called to inform the quota system that an action which may have updated
    // the amount of disk space used has completed. The parameter is true for
    // transactions that caused the backing store to flush.
    base::RepeatingCallback<void(bool /*did_sync*/)> on_files_written;

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
  // `BucketContext` it creates.
  BucketContext(storage::BucketInfo bucket_info,
                const base::FilePath& data_path,
                Delegate&& delegate,
                scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
                mojo::PendingRemote<storage::mojom::BlobStorageContext>
                    blob_storage_context,
                mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
                    file_system_access_context,
                InstanceClosure initialization_closure);

  BucketContext(const BucketContext&) = delete;
  BucketContext& operator=(const BucketContext&) = delete;

  ~BucketContext() override;

  void QueueRunTasks();

  // Normally, in-memory bucket contexts never self-close. If this is called
  // with `doom` set to true, they will self-close.
  void ForceClose(bool doom);

  // Starts capturing state data for indexeddb-internals. The data will be
  // returned the next time `StopMetadataRecording()` is invoked.
  void StartMetadataRecording();
  std::vector<storage::mojom::IdbBucketMetadataPtr> StopMetadataRecording();

  int64_t GetInMemorySize();

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
  BackingStore* backing_store() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return backing_store_.get();
  }
  const DBMap& GetDatabasesForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return databases_;
  }
  PartitionedLockManager& lock_manager() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return *lock_manager_;
  }
  const PartitionedLockManager& lock_manager() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return *lock_manager_;
  }
  BackingStorePreCloseTaskQueue* pre_close_task_queue() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return pre_close_task_queue_.get();
  }

  Delegate& delegate() { return delegate_; }

  base::OneShotTimer* close_timer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &close_timer_;
  }

  base::WeakPtr<BucketContext> AsWeakPtr() {
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

  TransactionalLevelDBFactory* transactional_leveldb_factory() {
    return transactional_leveldb_factory_.get();
  }

  void AddReceiver(
      const storage::BucketClientInfo& client_info,
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
          client_state_checker_remote,
      mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver);

  // blink::mojom::IDBFactory implementation:
  void GetDatabaseInfo(GetDatabaseInfoCallback callback) override;
  void Open(mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
                factory_client,
            mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
                database_callbacks_remote,
            const std::u16string& name,
            int64_t version,
            mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
                transaction_receiver,
            int64_t transaction_id,
            int scheduling_priority) override;
  void DeleteDatabase(mojo::PendingAssociatedRemote<
                          blink::mojom::IDBFactoryClient> factory_client,
                      const std::u16string& name,
                      bool force_close) override;

  // Finishes filling in `info` with data relevant to idb-internals and passes
  // the result back via the return value.
  storage::mojom::IdbBucketMetadataPtr FillInMetadata(
      storage::mojom::IdbBucketMetadataPtr info);
  // Called when the data used to populate the struct in `FillInMetadata` is
  // changed in a significant way.
  void NotifyOfIdbInternalsRelevantChange();

  // This exists to facilitate unit tests. Since `this` is owned via a
  // `SequenceBound`, it's not possible to directly grab pointer to `this`.
  BucketContext* GetReferenceForTesting();

  void CompactBackingStoreForTesting();
  void WriteToIndexedDBForTesting(const std::string& key,
                                  const std::string& value);
  void BindMockFailureSingletonForTesting(
      mojo::PendingReceiver<storage::mojom::MockFailureInjector> receiver);

  // Called when a fatal error has occurred that should result in tearing down
  // the backing store. `BucketContext` *may* be synchronously destroyed after
  // this is invoked. The string, if non-empty, is used as an error message.
  void OnDatabaseError(Status status, const std::string& message);

  // Called when the backing store has been corrupted.
  void HandleBackingStoreCorruption(const DatabaseError& error);

  // base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend BucketContextHandle;
  friend class BackingStoreTest;
  friend class DatabaseTest;
  friend class IndexedDBTest;
  friend class TransactionTest;

  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, CompactionKillSwitchWorks);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, CompactionTaskTiming);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, TombstoneSweeperTiming);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, TooLongOrigin);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, BasicFactoryCreationAndTearDown);
  FRIEND_TEST_ALL_PREFIXES(BucketContextTest, BucketSpaceDecay);
  FRIEND_TEST_ALL_PREFIXES(BucketContextTest, MetadataRecordingStateHistory);

  // The data structure that stores everything bound to the receiver. This will
  // be stored together with the receiver in the `mojo::ReceiverSet`.
  struct ReceiverContext {
    ReceiverContext(
        const storage::BucketClientInfo& client_info,
        mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
            client_state_checker_remote);

    ~ReceiverContext();

    ReceiverContext(const ReceiverContext&) = delete;
    ReceiverContext(ReceiverContext&&) noexcept;
    ReceiverContext& operator=(const ReceiverContext&) = delete;
    ReceiverContext& operator=(ReceiverContext&&) = delete;

    const storage::BucketClientInfo client_info;
    mojo::Remote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote;
  };

  // Used to synchronize the global throttling of LevelDB cleanup operations.
  // See `for_each_bucket_context`.
  static void SetInternalState(base::Time earliest_global_sweep_time,
                               base::Time earliest_global_compaction_time,
                               BucketContext& context);

  Database* AddDatabase(const std::u16string& name,
                        std::unique_ptr<Database> database);

  void OnHandleCreated();
  void OnHandleDestruction();

  // Returns true if this bucket context can be closed (no references, no blobs,
  // and not persisting for incognito).
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

  std::tuple<Status, DatabaseError, IndexedDBDataLossInfo>
  InitBackingStoreIfNeeded(bool create_if_missing);

  // Destroys `backing_store_` and all associated state. If there are no
  // receivers remaining, it will also destroy `this`.
  void ResetBackingStore();

  // Called when a receiver from `receiver_set_` has been disconnected. If there
  // are no receivers left and the backing store is already destroyed, this will
  // initiate destruction of `this`.
  void OnReceiverDisconnected();

  // Records one tick of Metadata during a metadata recording session.
  void RecordInternalsSnapshot();

  SEQUENCE_CHECKER(sequence_checker_);

  const storage::BucketInfo bucket_info_;

  // Base directory for blobs and backing store files.
  const base::FilePath data_path_;

  // True if there are blobs referencing this backing store that are still
  // alive. This is used as closing criteria for this object, see CanClose.
  bool has_blobs_outstanding_ = false;
  bool skip_closing_sequence_ = false;

  bool running_tasks_ = false;

  base::Time earliest_global_sweep_time_;
  base::Time earliest_global_compaction_time_;
  ClosingState closing_stage_ = ClosingState::kNotClosing;
  base::OneShotTimer close_timer_;
  std::unique_ptr<PartitionedLockManager> lock_manager_;
  std::unique_ptr<TransactionalLevelDBFactory> transactional_leveldb_factory_;
  std::unique_ptr<BackingStore> backing_store_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // Databases in the backing store which are already loaded/represented by
  // Database objects. The backing store may have other databases which
  // have not yet been loaded.
  DBMap databases_;
  // This is the refcount for the number of BucketContextHandle's given out for
  // this bucket context using OpenReference. This is used as closing criteria
  // for this object, see CanClose.
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
  // Mojo connection to `BlobStorageContext`, which runs on the IO thread.
  mojo::Remote<storage::mojom::BlobStorageContext> blob_storage_context_;
  // Mojo connection to `FileSystemAccessContextImpl`, which runs on the UI
  // thread.
  mojo::Remote<storage::mojom::FileSystemAccessContext>
      file_system_access_context_;
  // This map's value type contains a closure which will run on destruction.
  std::map<base::FilePath,
           std::tuple<std::unique_ptr<IndexedDBDataItemReader>,
                      base::ScopedClosureRunner /*release_callback*/>>
      file_reader_map_;

  std::unique_ptr<BackingStorePreCloseTaskQueue> pre_close_task_queue_;

  Delegate delegate_;

  // In-memory contexts will not self-close until this bit is flipped to true.
  bool is_doomed_ = false;

  // True if there's already a task queued to call `RunTasks()`.
  bool task_run_queued_ = false;

  std::vector<storage::mojom::IdbBucketMetadataPtr> metadata_recording_buffer_;
  bool metadata_recording_enabled_ = false;
  base::Time metadata_recording_start_time_;

  mojo::ReceiverSet<blink::mojom::IDBFactory, ReceiverContext> receivers_;

  base::WeakPtrFactory<BucketContext> weak_factory_{this};
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_BUCKET_CONTEXT_H_
