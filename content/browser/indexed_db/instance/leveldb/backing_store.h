// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_LEVELDB_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_LEVELDB_BACKING_STORE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/dcheck_is_on.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/privileged/mojom/indexed_db_control_test.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_external_object_storage.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/backing_store_pre_close_task_queue.h"
#include "content/browser/indexed_db/instance/leveldb/cleanup_scheduler.h"
#include "content/browser/indexed_db/instance/leveldb/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/status.h"
#include "content/common/content_export.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace blink {
class IndexedDBKeyRange;
struct IndexedDBDatabaseMetadata;
}  // namespace blink

namespace content::indexed_db {

class ActiveBlobRegistry;
class BucketContext;
class LevelDBWriteBatch;
class PartitionedLockManager;
class TransactionalLevelDBDatabase;
class TransactionalLevelDBIterator;
class TransactionalLevelDBTransaction;
struct IndexedDBDataLossInfo;
struct IndexedDBValue;

namespace level_db {

class AutoDidCommitTransaction;

class CONTENT_EXPORT BackingStore : public indexed_db::BackingStore,
                                    public LevelDBCleanupScheduler::Delegate {
 public:
  // This struct contains extra metadata only relevant to this implementation of
  // the backing store.
  struct CONTENT_EXPORT DatabaseMetadata
      : public blink::IndexedDBDatabaseMetadata {
   public:
    explicit DatabaseMetadata(const std::u16string& name);
    DatabaseMetadata(DatabaseMetadata&& metadata);
    DatabaseMetadata(const DatabaseMetadata& metadata);
    DatabaseMetadata& operator=(const DatabaseMetadata& metadata);
    DatabaseMetadata();
    ~DatabaseMetadata() override;

    // Uniquely identifies this database within the backing store. See
    // `GetNewDatabaseId()`. Null indicates that this object does not (yet)
    // represent a valid database.
    std::optional<int64_t> id;
  };

  class CONTENT_EXPORT Database : public indexed_db::BackingStore::Database {
   public:
    Database(BackingStore& backing_store,
             BackingStore::DatabaseMetadata metadata);
    ~Database() override;

    // indexed_db::BackingStore::Database:
    const blink::IndexedDBDatabaseMetadata& GetMetadata() const override;
    const IndexedDBDataLossInfo& GetDataLossInfo() const override;
    std::string GetObjectStoreLockIdKey(int64_t object_store_id) const override;
    std::unique_ptr<Transaction> CreateTransaction(
        blink::mojom::IDBTransactionDurability durability,
        blink::mojom::IDBTransactionMode mode) override;
    Status DeleteDatabase(std::vector<PartitionedLock> locks,
                          base::OnceClosure on_complete) override;

    DatabaseMetadata& metadata() { return metadata_; }
    base::WeakPtr<BackingStore> backing_store() { return backing_store_; }

   private:
    base::WeakPtr<BackingStore> backing_store_;
    DatabaseMetadata metadata_;

    base::WeakPtrFactory<Database> weak_factory_{this};
  };

  class Cursor;

  // This class could be moved to the implementation file, but it's left here to
  // avoid needless git churn.
  class CONTENT_EXPORT Transaction
      : public indexed_db::BackingStore::Transaction {
   public:
    struct BlobWriteState {
      BlobWriteState();
      explicit BlobWriteState(int calls_left, BlobWriteCallback on_complete);
      ~BlobWriteState();
      int calls_left = 0;
      BlobWriteCallback on_complete;
    };

    Transaction(base::WeakPtr<Database> database,
                blink::mojom::IDBTransactionDurability durability,
                blink::mojom::IDBTransactionMode mode);

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    ~Transaction() override;

    Status Begin(std::vector<PartitionedLock> locks) override;
    // The `serialize_fsa_handle` callback is not used.
    Status CommitPhaseOne(BlobWriteCallback callback,
                          SerializeFsaCallback serialize_fsa_handle) override;
    Status CommitPhaseTwo() override;
    void Rollback() override;
    Status SetDatabaseVersion(int64_t version) override;
    Status CreateObjectStore(int64_t object_store_id,
                             const std::u16string& name,
                             blink::IndexedDBKeyPath key_path,
                             bool auto_increment) override;
    Status DeleteObjectStore(int64_t object_store_id) override;
    Status RenameObjectStore(int64_t object_store_id,
                             const std::u16string& new_name) override;

    // Creates a new index metadata and writes it to the transaction.
    Status CreateIndex(int64_t object_store_id,
                       blink::IndexedDBIndexMetadata index) override;
    // Deletes the index metadata on the transaction (but not any index
    // entries).
    Status DeleteIndex(int64_t object_store_id, int64_t index_id) override;
    // Renames the given index and writes it to the transaction.
    Status RenameIndex(int64_t object_store_id,
                       int64_t index_id,
                       const std::u16string& new_name) override;
    StatusOr<IndexedDBValue> GetRecord(int64_t object_store_id,
                                       const blink::IndexedDBKey& key) override;
    StatusOr<RecordIdentifier> PutRecord(int64_t object_store_id,
                                         const blink::IndexedDBKey& key,
                                         IndexedDBValue value) override;
    Status ClearObjectStore(int64_t object_store_id) override;
    Status DeleteRange(int64_t object_store_id,
                       const blink::IndexedDBKeyRange&) override;
    StatusOr<int64_t> GetKeyGeneratorCurrentNumber(
        int64_t object_store_id) override;
    Status MaybeUpdateKeyGeneratorCurrentNumber(int64_t object_store_id,
                                                int64_t new_number,
                                                bool was_generated) override;
    StatusOr<std::optional<RecordIdentifier>> KeyExistsInObjectStore(
        int64_t object_store_id,
        const blink::IndexedDBKey& key) override;
    Status PutIndexDataForRecord(int64_t object_store_id,
                                 int64_t index_id,
                                 const blink::IndexedDBKey& key,
                                 const RecordIdentifier& record) override;
    StatusOr<blink::IndexedDBKey> GetFirstPrimaryKeyForIndexKey(
        int64_t object_store_id,
        int64_t index_id,
        const blink::IndexedDBKey& key) override;
    StatusOr<uint32_t> GetObjectStoreKeyCount(
        int64_t object_store_id,
        blink::IndexedDBKeyRange key_range) override;
    StatusOr<uint32_t> GetIndexKeyCount(
        int64_t object_store_id,
        int64_t index_id,
        blink::IndexedDBKeyRange key_range) override;
    StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>>
    OpenObjectStoreKeyCursor(int64_t object_store_id,
                             const blink::IndexedDBKeyRange& key_range,
                             blink::mojom::IDBCursorDirection) override;
    StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>>
    OpenObjectStoreCursor(int64_t object_store_id,
                          const blink::IndexedDBKeyRange& key_range,
                          blink::mojom::IDBCursorDirection) override;
    StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>>
    OpenIndexKeyCursor(int64_t object_store_id,
                       int64_t index_id,
                       const blink::IndexedDBKeyRange& key_range,
                       blink::mojom::IDBCursorDirection) override;
    StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>> OpenIndexCursor(
        int64_t object_store_id,
        int64_t index_id,
        const blink::IndexedDBKeyRange& key_range,
        blink::mojom::IDBCursorDirection) override;

    // `deserialize_fsa_handle` is not used in this implementation.
    blink::mojom::IDBValuePtr BuildMojoValue(
        IndexedDBValue value,
        DeserializeFsaCallback deserialize_fsa_handle) override;

    Status PutExternalObjectsIfNeeded(const std::string& object_store_data_key,
                                      std::vector<IndexedDBExternalObject>*);
    void PutExternalObjects(const std::string& object_store_data_key,
                            std::vector<IndexedDBExternalObject>*);

    TransactionalLevelDBTransaction* transaction() {
      return transaction_.get();
    }

    void SetTombstoneThresholdExceeded(bool tombstone_threshold_exceeded) {
      tombstone_threshold_exceeded_ = tombstone_threshold_exceeded;
    }

    Status GetExternalObjectsForRecord(const std::string& object_store_data_key,
                                       IndexedDBValue* value);

    base::WeakPtr<Transaction> AsWeakPtr();

    blink::mojom::IDBTransactionDurability durability() const {
      return durability_;
    }
    blink::mojom::IDBTransactionMode mode() const { return mode_; }

   private:
    int64_t database_id() const { return *database_->metadata().id; }

    Status FindKeyInIndex(int64_t object_store_id,
                          int64_t index_id,
                          const blink::IndexedDBKey& key,
                          std::string* found_encoded_primary_key,
                          bool* found);

    // Called by CommitPhaseOne: Identifies the blob entries to write and adds
    // them to the recovery blob journal directly (i.e. not as part of the
    // transaction). Populates blobs_to_write_.
    Status HandleBlobPreTransaction();

    // Called by CommitPhaseOne: Populates blob_files_to_remove_ by
    // determining which blobs are deleted as part of the transaction, and
    // adds blob entry cleanup operations to the transaction. Returns true on
    // success, false on failure.
    bool CollectBlobFilesToRemove();

    // Called by CommitPhaseOne: Kicks off the asynchronous writes of blobs
    // identified in HandleBlobPreTransaction. The callback will be called
    // eventually on success or failure.
    Status WriteNewBlobs(BlobWriteCallback callback);

    // Called by CommitPhaseTwo: Partition blob references in blobs_to_remove_
    // into live (active references) and dead (no references).
    void PartitionBlobsToRemove(BlobJournalType* dead_blobs,
                                BlobJournalType* live_blobs) const;

    // Prepares a cursor and returns it if successful, an error Status if
    // there's an error, or null if the cursor is empty.
    StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>> PrepareCursor(
        std::unique_ptr<Cursor> cursor);

    // This does NOT mean that this class can outlive the BackingStore.
    // This is only to protect against security issues before this class is
    // refactored away and this isn't necessary.
    // https://crbug.com/1012918
    base::WeakPtr<BackingStore> backing_store_;
    base::WeakPtr<Database> database_;

    scoped_refptr<TransactionalLevelDBTransaction> transaction_;

    std::map<std::string, std::unique_ptr<IndexedDBExternalObjectChangeRecord>>
        external_object_change_map_;
    std::map<std::string, std::unique_ptr<IndexedDBExternalObjectChangeRecord>>
        in_memory_external_object_map_;

    // List of blob files being newly written as part of this transaction.
    // These will be added to the recovery blob journal prior to commit, then
    // removed after a successful commit.
    BlobJournalType blobs_to_write_;

    // List of blob files being deleted as part of this transaction. These will
    // be added to either the recovery or live blob journal as appropriate
    // following a successful commit.
    BlobJournalType blobs_to_remove_;

    // Populated when blobs are being written to disk. This is saved here (as
    // opposed to being ephemeral and owned by the WriteBlobToFile callbacks)
    // because the transaction needs to be able to cancel this operation in
    // Rollback().
    std::optional<BlobWriteState> write_state_;

    // Set to true between CommitPhaseOne and CommitPhaseTwo/Rollback, to
    // indicate that the committing_transaction_count_ on the backing store
    // has been bumped, and journal cleaning should be deferred.
    bool committing_ = false;

    // This flag is passed to LevelDBScopes as `sync_on_commit`, converted
    // via ShouldSyncOnCommit.
    const blink::mojom::IDBTransactionDurability durability_;
    const blink::mojom::IDBTransactionMode mode_;

    // This flag is set when tombstones encountered during a read-only
    // cursor operation exceed `kCursorTombstoneThreshold`.
    bool tombstone_threshold_exceeded_ = false;

    std::optional<DatabaseMetadata> metadata_before_transaction_;

    base::WeakPtrFactory<Transaction> weak_ptr_factory_{this};
  };

  // This class could be moved to the implementation file, but it's left here to
  // avoid needless git churn.
  class Cursor : public indexed_db::BackingStore::Cursor {
   public:
    enum IteratorState { READY = 0, SEEK };

    struct CursorOptions {
      CursorOptions();
      CursorOptions(const CursorOptions& other);
      ~CursorOptions();

      int64_t database_id;
      int64_t object_store_id;
      int64_t index_id;
      std::string low_key;
      bool low_open;
      std::string high_key;
      bool high_open;
      bool forward;
      bool unique;
      blink::mojom::IDBTransactionMode mode =
          blink::mojom::IDBTransactionMode::ReadWrite;
    };

    Cursor(const Cursor&) = delete;
    Cursor& operator=(const Cursor&) = delete;

    ~Cursor() override;

    // indexed_db::BackingStore::Cursor:
    const blink::IndexedDBKey& GetKey() const override;
    const blink::IndexedDBKey& GetPrimaryKey() const override;
    blink::IndexedDBKey TakeKey() && override;
    StatusOr<bool> Continue() override;
    StatusOr<bool> Continue(const blink::IndexedDBKey& key,
                            const blink::IndexedDBKey& primary_key) override;
    StatusOr<bool> Advance(uint32_t count) override;
    void SavePosition() override;
    Status TryResetToLastSavedPosition() override;

    StatusOr<bool> Continue(const blink::IndexedDBKey& key,
                            const blink::IndexedDBKey& primary_key,
                            IteratorState state);
    StatusOr<bool> FirstSeek();

   protected:
    Cursor(base::WeakPtr<Transaction> transaction,
           int64_t database_id,
           const CursorOptions& cursor_options);

    // May return nullptr.
    static std::unique_ptr<TransactionalLevelDBIterator> CloneIterator(
        const Cursor* other);

    virtual bool LoadCurrentRow(Status* s) = 0;
    virtual std::string EncodeKey(const blink::IndexedDBKey& key) = 0;
    virtual std::string EncodeKey(const blink::IndexedDBKey& key,
                                  const blink::IndexedDBKey& primary_key) = 0;

    bool IsPastBounds() const;
    bool HaveEnteredRange() const;

    // If the version numbers don't match or if the value is missing, that
    // means this is an obsolete index entry (a 'tombstone') that can be
    // cleaned up. This removal can only happen in non-read-only transactions.
    void RemoveTombstoneOrIncrementCount(Status* s);

    // This does NOT mean that this class can outlive the Transaction.
    // This is only to protect against security issues before this class is
    // refactored away and this isn't necessary.
    // https://crbug.com/1012918
    const base::WeakPtr<Transaction> transaction_;
    const int64_t database_id_;
    const CursorOptions cursor_options_;
    std::unique_ptr<TransactionalLevelDBIterator> iterator_;
    blink::IndexedDBKey current_key_;

   private:
    enum class ContinueResult { DONE, OUT_OF_BOUNDS };

    // For cursors with direction Next or NextNoDuplicate.
    StatusOr<ContinueResult> ContinueNext(
        const blink::IndexedDBKey& key,
        const blink::IndexedDBKey& primary_key,
        IteratorState state);
    // For cursors with direction Prev or PrevNoDuplicate. The PrevNoDuplicate
    // case has additional complexity of not being symmetric with
    // NextNoDuplicate.
    StatusOr<ContinueResult> ContinuePrevious(
        const blink::IndexedDBKey& key,
        const blink::IndexedDBKey& primary_key,
        IteratorState state);

    int tombstones_count_ = 0;
    // `iterator_` and `current_key_` are saved when `SavePosition()` is called.
    std::optional<std::tuple<std::unique_ptr<TransactionalLevelDBIterator>,
                             blink::IndexedDBKey>>
        saved_members_;
    base::WeakPtrFactory<Cursor> weak_factory_{this};
  };

  using BlobFilesCleanedCallback = base::RepeatingClosure;
  using ReportOutstandingBlobsCallback =
      base::RepeatingCallback<void(/*outstanding_blobs=*/bool)>;

  enum class Mode { kInMemory, kOnDisk };

  // Schedule an immediate blob journal cleanup if we reach this number of
  // requests.
  static constexpr const int kMaxJournalCleanRequests = 50;
  // Wait for a maximum of 5 seconds from the first call to the timer since the
  // last journal cleaning.
  static constexpr const base::TimeDelta kMaxJournalCleaningWindowTime =
      base::Seconds(5);
  // Default to a 2 second timer delay before we clean up blobs.
  static constexpr const base::TimeDelta kInitialJournalCleaningWindowTime =
      base::Seconds(2);

  BackingStore(Mode backing_store_mode,
               const storage::BucketLocator& bucket_locator,
               const base::FilePath& blob_path,
               std::unique_ptr<TransactionalLevelDBDatabase> db,
               BlobFilesCleanedCallback blob_files_cleaned,
               ReportOutstandingBlobsCallback report_outstanding_blobs);

  BackingStore(const BackingStore&) = delete;
  BackingStore& operator=(const BackingStore&) = delete;

  ~BackingStore() override;

  // Initializes the backing store. This must be called before doing any
  // operations or method calls on this object.
  Status Initialize(bool clean_active_blob_journal);

  const storage::BucketLocator& bucket_locator() const {
    return bucket_locator_;
  }

  ActiveBlobRegistry* active_blob_registry() {
    return active_blob_registry_.get();
  }

  void OnTransactionComplete(bool tombstone_threshold_exceeded);

  static void HandleCorruption(const base::FilePath& path_base,
                               const storage::BucketLocator& bucket_locator,
                               const std::string& message);

  // BackingStore:
  bool CanOpportunisticallyClose() const override;
  void TearDown(base::WaitableEvent* signal_on_destruction) override;
  void InvalidateBlobReferences() override;
  void StartPreCloseTasks(base::OnceClosure on_done) override;
  void StopPreCloseTasks() override;
  StatusOr<std::unique_ptr<indexed_db::BackingStore::Database>>
  CreateOrOpenDatabase(const std::u16string& name) override;
  uintptr_t GetIdentifierForMemoryDump() override;
  void FlushForTesting() override;
  StatusOr<bool> DatabaseExists(std::u16string_view database_name) override;
  StatusOr<std::vector<blink::mojom::IDBNameAndVersionPtr>>
  GetDatabaseNamesAndVersions() override;
  int64_t GetInMemorySize() const override;

  // LevelDBCleanupScheduler::Delegate:
  void OnCleanupStarted() override;
  void OnCleanupDone() override;
  Status GetCompleteMetadata(
      std::vector<std::unique_ptr<blink::IndexedDBDatabaseMetadata>>* output)
      override;

  base::FilePath GetBlobFileName(int64_t database_id, int64_t key) const;

  TransactionalLevelDBDatabase* db() { return db_.get(); }

  const std::string& origin_identifier() { return origin_identifier_; }

#if DCHECK_IS_ON()
  int NumBlobFilesDeletedForTesting() { return num_blob_files_deleted_; }
#endif
  int NumAggregatedJournalCleaningRequestsForTesting() const {
    return num_aggregated_journal_cleaning_requests_;
  }
  void SetNumAggregatedJournalCleaningRequestsForTesting(int num_requests) {
    num_aggregated_journal_cleaning_requests_ = num_requests;
  }
  void SetExecuteJournalCleaningOnNoTransactionsForTesting() {
    execute_journal_cleaning_on_no_txns_ = true;
  }

  const LevelDBCleanupScheduler& GetLevelDBCleanupSchedulerForTesting() const {
    return level_db_cleanup_scheduler_;
  }

  bool in_memory() const { return backing_store_mode_ == Mode::kInMemory; }

  base::WeakPtr<BackingStore> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

  static bool ShouldSyncOnCommit(
      blink::mojom::IDBTransactionDurability durability);

  // Create and initialize a BackingStore; verify and report its status.
  static std::tuple<std::unique_ptr<indexed_db::BackingStore>,
                    Status,
                    IndexedDBDataLossInfo,
                    bool /* is_disk_full */>
  OpenAndVerify(BucketContext& bucket_context,
                base::FilePath data_directory,
                base::FilePath database_path,
                base::FilePath blob_path,
                PartitionedLockManager* lock_manager,
                bool is_first_attempt,
                bool create_if_missing);

  // LINT.IfChange(InSessionCleanupVerificationEvent)
  enum class InSessionCleanupVerificationEvent {
    kCleanupStarted = 0,
    kErrorOpeningBefore = 1,
    kErrorSnapshottingBefore = 2,
    kErrorOpeningAfter = 3,
    kErrorSnapshottingAfter = 4,
    kMatchedSnapshot = 5,
    kMismatchedSnapshot = 6,

    kMaxValue = kMismatchedSnapshot,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:IndexedDbLevelDbCleanupVerificationEvent)

 private:
  FRIEND_TEST_ALL_PREFIXES(LevelDbBackingStoreTestWithExternalObjects,
                           ActiveBlobJournal);
  FRIEND_TEST_ALL_PREFIXES(LevelDbBackingStoreTest, CompactionTaskTiming);
  FRIEND_TEST_ALL_PREFIXES(LevelDbBackingStoreTest, TombstoneSweeperTiming);

  friend class AutoDidCommitTransaction;
  friend class indexed_db::BucketContext;

  static std::tuple<std::unique_ptr<BackingStore>,
                    Status,
                    IndexedDBDataLossInfo,
                    bool /* is_disk_full */>
  DoOpenAndVerify(BucketContext& bucket_context,
                  base::FilePath data_directory,
                  base::FilePath database_path,
                  base::FilePath blob_path,
                  PartitionedLockManager* lock_manager,
                  bool is_first_attempt,
                  bool create_if_missing);

  // Fills in metadata for the database specified by `metadata->name` by reading
  // from disk. If no database is found, `metadata->id` will remain null.
  Status ReadMetadataForDatabaseName(DatabaseMetadata& metadata);

  StatusOr<std::vector<std::u16string>> GetDatabaseNames();

  // Updates the next run timestamp for the tombstone sweeper in the database
  // metadata. Returns if writing the update to the LevelDB db was successful.
  bool UpdateEarliestSweepTime();
  // Updates the next run timestamp for the level db compaction in the database
  // metadata. Returns if writing the update to the LevelDB db was successful.
  bool UpdateEarliestCompactionTime();

  // Dumps and returns all the databases in this store. If an error Status is
  // returned, this method will also log UMA to that effect.
  StatusOr<base::ListValue> SnapshotAllDatabases(bool before_cleanup);

  // A helper function for V4 schema migration.
  // It iterates through all blob files.  It will add to the db entry both the
  // size and modified date for the blob based on the written file.  If any blob
  // file in the db is missing on disk, it will return an inconsistency status.
  Status UpgradeBlobEntriesToV4(
      LevelDBWriteBatch* write_batch,
      std::vector<base::FilePath>* empty_blobs_to_delete);
  // A helper function for V5 schema miration.
  // Iterates through all blob files on disk and validates they exist,
  // returning an internal inconsistency corruption error if any are missing.
  Status ValidateBlobFiles();

  // Remove the referenced file on disk.
  bool RemoveBlobFile(int64_t database_id, int64_t key) const;

  // Schedule a call to CleanRecoveryJournalIgnoreReturn() via
  // an owned timer. If this object is destroyed, the timer
  // will automatically be cancelled.
  void StartJournalCleaningTimer();

  // Attempt to clean the recovery journal. This will remove
  // any referenced files and delete the journal entry. If any
  // transaction is currently committing this will be deferred
  // via StartJournalCleaningTimer().
  void CleanRecoveryJournalIgnoreReturn();

  Status MigrateToV4(LevelDBWriteBatch* write_batch);
  Status MigrateToV5(LevelDBWriteBatch* write_batch);

  // Used by ActiveBlobRegistry::MarkBlobInactive.
  void ReportBlobUnused(int64_t database_id, int64_t blob_number);

  // Remove the blob directory for the specified database and all contained
  // blob files.
  bool RemoveBlobDirectory(int64_t database_id) const;

  // Synchronously read the key-specified blob journal entry from the backing
  // store, delete all referenced blob files, and erase the journal entry.
  // This must not be used while temporary entries are present e.g. during
  // a two-stage transaction commit with blobs.
  Status CleanUpBlobJournal(const std::string& level_db_key) const;

  // Synchronously delete the files and/or directories on disk referenced by
  // the blob journal.
  Status CleanUpBlobJournalEntries(const BlobJournalType& journal) const;

  void WillCommitTransaction();
  // Can run a journal cleaning job if one is pending.
  void DidCommitTransaction();

  // Returns whether tombstone sweeping or compaction should occur now, checking
  // and updating timing information as needed for throttling.
  bool ShouldRunTombstoneSweeper();
  bool ShouldRunCompaction();

  // Returns true if a blob cleanup job is pending on journal_cleaning_timer_.
  bool IsBlobCleanupPending();

  // Stops the journal_cleaning_timer_ and runs its pending task.
  void ForceRunBlobCleanup();

  // Owns `this`. Should be initialized shortly after construction.
  raw_ptr<BucketContext> bucket_context_ = nullptr;

  const Mode backing_store_mode_;
  const storage::BucketLocator bucket_locator_;
  const base::FilePath blob_path_;

  // The origin identifier is a key prefix, unique to the storage key's origin,
  // used in the leveldb backing store to partition data by origin. It is a
  // normalized version of the origin URL with a versioning suffix appended,
  // e.g. "http_localhost_81@1." Since only one storage key is stored per
  // backing store this is redundant but necessary for backwards compatibility.
  const std::string origin_identifier_;

  std::map<std::string, std::unique_ptr<IndexedDBExternalObjectChangeRecord>>
      in_memory_external_object_map_;

  bool execute_journal_cleaning_on_no_txns_ = false;
  int num_aggregated_journal_cleaning_requests_ = 0;
  base::OneShotTimer journal_cleaning_timer_;
  base::TimeTicks journal_cleaning_timer_window_start_;

#if DCHECK_IS_ON()
  mutable int num_blob_files_deleted_ = 0;
#endif

  const std::unique_ptr<TransactionalLevelDBDatabase> db_;

  const BlobFilesCleanedCallback blob_files_cleaned_;

  // Whenever blobs are registered in active_blob_registry_,
  // indexed_db_factory_ will hold a reference to this backing store.
  std::unique_ptr<ActiveBlobRegistry> active_blob_registry_;

  // Ensures tombstones are removed periodically during an active session.
  LevelDBCleanupScheduler level_db_cleanup_scheduler_;

  // Incremented whenever a transaction starts committing, decremented when
  // complete. While > 0, temporary journal entries may exist so out-of-band
  // journal cleaning must be deferred.
  size_t committing_transaction_count_ = 0;

#if DCHECK_IS_ON()
  bool initialized_ = false;
#endif

  // Snapshot of all known DBs. Used for debugging/verification of potentially
  // destructive cleanup operations.
  std::optional<base::ListValue> dbs_snapshot_;

  std::unique_ptr<BackingStorePreCloseTaskQueue> pre_close_task_queue_;

  // Path to the leveldb database, or empty if in-memory.
  base::FilePath database_path_;

  base::WeakPtrFactory<BackingStore> weak_factory_{this};
};

void BindMockFailureSingletonForTesting(
    mojo::PendingReceiver<storage::mojom::MockFailureInjector> receiver);

}  // namespace level_db
}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_LEVELDB_BACKING_STORE_H_
