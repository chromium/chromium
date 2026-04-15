// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_BUCKET_CONTEXT_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_BUCKET_CONTEXT_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <tuple>

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
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
#include "content/browser/indexed_db/instance/blob_reader.h"
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
class Database;

// Used as a feature param by `kIdbSqliteOnDiskRollout`. Adding, removing and
// reordering values is fine; just make sure to update
// `kIdbSqliteOnDiskRolloutStages` when adding new values.
enum class SqliteRolloutStage {
  // Use LevelDB exclusively; delete SQLite stores if found.
  // All on-disk stores emit metrics to the "OnDisk" variant.
  kUseLevelDbOnly,
  // Functionally, the same as `kUseLevelDbOnly`.
  // On-disk stores created during this stage emit metrics to the "Experimental"
  // variant and previously existing stores emit to the "OnDisk" variant.
  kUseLevelDbAsControl,
  // Use SQLite for new stores and corrupted LevelDB stores.
  // On-disk SQLite stores emit metrics to the "Experimental" variant and
  // on-disk LevelDB stores emit to the "OnDisk" variant.
  kUseSqliteForNewStores,
  // Use SQLite exclusively; delete LevelDB stores if found.
  // All on-disk stores emit metrics to the "OnDisk" variant.
  kUseSqliteOnly,
};

// BucketContext manages the per-bucket IndexedDB state, and other important
// context like the backing store and lock manager.
//
// BucketContext will keep its backing store around while any of these is true:
// * There are `Database` objects,
// * There are outstanding blob references to this database's blob files, or
// * The bucket context is in-memory (i.e. an incognito profile).
//
// When these qualities are no longer true, `RunTasks()` will invoke
// `ResetBackingStore()`, which returns `this` to an uninitialized state.
//
// Each instance of this class is created and run on a unique thread pool
// SequencedTaskRunner, i.e. the "bucket sequence".
class CONTENT_EXPORT BucketContext
    : public blink::mojom::IDBFactory,
      public base::trace_event::MemoryDumpProvider {
 public:
  using DBMap = base::flat_map<std::u16string, std::unique_ptr<Database>>;

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
  // The callbacks are invoked on the bucket sequence, but almost all are bound
  // to run on the main IDB sequence (see IndexedDBContextImpl).
  struct CONTENT_EXPORT Delegate {
    Delegate();
    Delegate(Delegate&&);
    ~Delegate();

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    base::OnceClosure on_destroyed;

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
  };

  BucketContext(storage::BucketInfo bucket_info,
                const base::FilePath& data_path,
                Delegate&& delegate,
                scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
                mojo::PendingRemote<storage::mojom::BlobStorageContext>
                    blob_storage_context,
                mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
                    file_system_access_context);

  BucketContext(const BucketContext&) = delete;
  BucketContext& operator=(const BucketContext&) = delete;

  ~BucketContext() override;

  // Calculate the usage of the bucket by directly examining the disk. Should be
  // used in lieu of `GetUsage()` only when there is no live `BucketContext` for
  // the given bucket.
  static uint64_t ReadUsageFromDisk(
      const storage::BucketLocator& bucket_locator,
      const base::FilePath& data_path);

  // All `BucketContext` instances created during the lifetime of the returned
  // object will use SQLite iff `use_sqlite` is true, unless overridden for a
  // specific instance with `SetSqliteRolloutStageForTesting()`.
  static base::AutoReset<std::optional<bool>> OverrideShouldUseSqliteForTesting(
      bool use_sqlite);

  // Inserts an extra step during BackingStore teardown; used for testing
  // crbug.com/340398745.
  static void InsertTeardownStepForTesting(base::OnceClosure on_teardown);

  static base::TimeDelta GetBackingStoreGracePeriodForTesting();
  static base::TimeDelta GetIdleTimeoutForTesting();

  // Whether the backing store is using SQLite. `CHECK`s that the backing store
  // exists.
  bool IsUsingSqlite() const;

  // Returns the suffix to append to histogram names based on the backing store
  // type. `CHECK`s that the backing store exists.
  std::string_view GetHistogramSuffix() const;

  void QueueRunTasks();

  // Returns true if a RunTask invocation is queued. To be used by metrics.
  bool task_run_queued() const { return task_run_queued_; }

  // Closes the bucket context, i.e. closes the backing store and closes Mojo
  // connections to renderers. When `doom` is true, the directories containing
  // data will also be deleted. Normally, in-memory bucket contexts never close.
  // If this is called with `doom` set to true, they will close. Note that if
  // `doom` is true, it's expected that `this` will be deleted soon after. To
  // prevent races, `on_ready_for_destruction` is NOT called in this case.
  void ForceClose(bool doom);

  // Starts capturing state data for indexeddb-internals. The data will be
  // returned the next time `StopMetadataRecording()` is invoked.
  void StartMetadataRecording();
  std::vector<storage::mojom::IdbBucketMetadataPtr> StopMetadataRecording();

  // Returns the current usage of the bucket, in bytes. `write_in_progress` is
  // true iff the last readwrite transaction did not flush changes to disk
  // (i.e., had relaxed durability).
  uint64_t GetUsage(bool write_in_progress);

  bool IsClosing() const { return closing_stage_ != ClosingState::kNotClosing; }

  ClosingState closing_stage() const { return closing_stage_; }

  void ReportOutstandingBlobs(bool blobs_outstanding);

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
  // Only used for LevelDB.
  void CreateAllExternalObjects(
      const std::vector<IndexedDBExternalObject>& objects,
      std::vector<blink::mojom::IDBExternalObjectPtr>* mojo_objects);

  const storage::BucketInfo& bucket_info() { return bucket_info_; }
  storage::BucketLocator bucket_locator() {
    return bucket_info_.ToBucketLocator();
  }
  BackingStore* backing_store() {
    return backing_store_ ? std::get<0>(*backing_store_).get() : nullptr;
  }
  const DBMap& GetDatabasesForTesting() const { return databases_; }
  PartitionedLockManager& lock_manager() { return *lock_manager_; }
  const PartitionedLockManager& lock_manager() const { return *lock_manager_; }

  Delegate& delegate() { return delegate_; }

  base::OneShotTimer* close_timer() { return &close_timer_; }

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

  void FlushBackingStoreForTesting();
  void BindMockFailureSingletonForTesting(
      mojo::PendingReceiver<storage::mojom::MockFailureInjector> receiver);

  // Called when a fatal error has occurred that should result in tearing down
  // the backing store. `BucketContext` *may* be synchronously destroyed after
  // this is invoked. The string, if non-empty, is used as an error message.
  // `database` is used in SQLite mode only.
  void OnDatabaseError(Database* database,
                       Status status,
                       const std::string& message);

  // Called when the backing store has been corrupted.
  void HandleBackingStoreCorruption(const std::string& error_message);

  // base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  bool in_memory() const { return data_path_.empty(); }

 private:
  friend class BackingStoreTestBase;
  friend class DatabaseTest;
  friend class IndexedDBTest;
  friend class IndexedDBTestBase;
  friend class IndexedDBTestForSqliteMigration;
  friend class TransactionTestBase;

  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, CompactionKillSwitchWorks);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, PreCloseTasksStart);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, TooLongOrigin);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, BasicFactoryCreationAndTearDown);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBSqliteTest, BlobReadPutsOffIdleWork);
  FRIEND_TEST_ALL_PREFIXES(BucketContextTest, BucketSpaceDecay);
  FRIEND_TEST_ALL_PREFIXES(BucketContextTest, MetadataRecordingStateHistory);
  FRIEND_TEST_ALL_PREFIXES(BucketContextTest,
                           OverrideShouldUseSqliteForTesting);

  // Overrides the rollout stage for this instance only. Must be called before
  // backing store initialization.
  void SetSqliteRolloutStageForTesting(SqliteRolloutStage stage);

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

  void DoForceClose(bool doom, const std::string& message);

  Database* CreateAndAddDatabase(const std::u16string& name);

  // Returns true if the backing store can be closed (no references, no blobs,
  // and not persisting for incognito).
  bool CanClose();

  // This should be called any time `this` begins handling an `IDBFactory`
  // message. It resets the backing store close timer and prevents the backing
  // store from closing, then potentially initiates the pre-close period when
  // the message handler completes.
  [[nodiscard]] base::ScopedClosureRunner ScopedHandlingRequest();

  // Starts the pre-close grace period for the backing store, if appropriate.
  void MaybeStartClosing();
  void MaybeStopClosing();
  // Queues closing the backing store.
  void CloseSoon();
  void StartPreCloseTasks();
  void RunTasks();

  // Called when there is any activity that should reset the idle timer.
  void OnActivity();
  // Called after a period of inactivity.
  void RunIdleTasks();

  void OnGotBucketSpaceRemaining(storage::QuotaErrorOr<int64_t> space_left);

  // Returns the amount of bucket space `this` has the authority to approve by
  // decaying `bucket_space_remaining_` according to the amount of time passed
  // since `bucket_space_remaining_timestamp_`.
  int64_t GetBucketSpaceToAllot();

  // Hooks up a `BlobReader` to `receiver` for the blob described by
  // `blob_info`.
  void BindBlobReader(const IndexedDBExternalObject& blob_info,
                      mojo::PendingReceiver<blink::mojom::Blob> receiver);
  // Removes all readers for this file path.
  void RemoveBoundReaders(const base::FilePath& path);

  std::tuple<Status, DatabaseError, IndexedDBDataLossInfo> InitBackingStore(
      bool create_if_missing);

  // Destroys `backing_store_` and all associated state. If there are no
  // receivers remaining, it will also destroy `this`.
  void ResetBackingStore();

  // Called when a receiver from `receiver_set_` has been disconnected. If there
  // are no receivers left and the backing store is already destroyed, this will
  // initiate destruction of `this`.
  void OnReceiverDisconnected();

  // Records one tick of Metadata during a metadata recording session.
  void RecordInternalsSnapshot();

  std::string SanitizeErrorMessage(const std::string& message);

  // Called when a Web Blob is being read from SQLite. `final_result` will hold
  // a value IFF the read operation has completed.
  void OnSqliteBlobActivity(std::optional<net::Error> final_result);

  const storage::BucketInfo bucket_info_;

  // Base directory for blobs and backing store files.
  const base::FilePath data_path_;

  // Set at construction. Can be overridden by
  // `SetSqliteRolloutStageForTesting()`.
  SqliteRolloutStage sqlite_rollout_stage_;

  // True if there are blobs referencing this backing store that are still
  // alive. This is used as closing criteria for this object, see CanClose.
  bool has_blobs_outstanding_ = false;

  bool running_tasks_ = false;

  ClosingState closing_stage_ = ClosingState::kNotClosing;
  base::RetainingOneShotTimer idle_timer_;
  std::optional<base::TimeTicks> last_idle_tasks_completion_time_;
  base::OneShotTimer close_timer_;
  std::unique_ptr<PartitionedLockManager> lock_manager_;
  // <BackingStore, is_sqlite, histogram_suffix>. Set only after a successful
  // call to `InitBackingStore()`.
  std::optional<
      std::tuple<std::unique_ptr<BackingStore>, bool, std::string_view>>
      backing_store_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // Databases in the backing store which are already loaded/represented by
  // Database objects. The backing store may have other databases which
  // have not yet been loaded.
  DBMap databases_;

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
           std::tuple<std::unique_ptr<BlobReader>,
                      base::ScopedClosureRunner /*release_callback*/>>
      file_reader_map_;

  Delegate delegate_;

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
