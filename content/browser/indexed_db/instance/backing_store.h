// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_H_

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
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/privileged/mojom/indexed_db_control_test.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_external_object_storage.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/status.h"
#include "content/common/content_export.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}  // namespace base

namespace blink {
class IndexedDBKeyRange;
struct IndexedDBDatabaseMetadata;
}  // namespace blink

namespace content::indexed_db {

class AutoDidCommitTransaction;
class BackingStoreTest;
class BucketContext;
class ActiveBlobRegistry;
class LevelDBWriteBatch;
class PartitionedLockManager;
class TransactionalLevelDBDatabase;
class TransactionalLevelDBFactory;
class TransactionalLevelDBIterator;
class TransactionalLevelDBTransaction;
struct IndexedDBDataLossInfo;
struct IndexedDBValue;

namespace indexed_db_backing_store_unittest {
FORWARD_DECLARE_TEST(BackingStoreTest, ReadCorruptionInfo);
}  // namespace indexed_db_backing_store_unittest

// This class is not thread-safe.
// All accessses to one instance must occur on the same sequence. Currently,
// this must be the IndexedDB task runner's sequence.
class CONTENT_EXPORT BackingStore {
 public:
  // This class is not thread-safe.
  // All accessses to one instance must occur on the same sequence.
  class CONTENT_EXPORT RecordIdentifier {
   public:
    RecordIdentifier();
    RecordIdentifier(std::string primary_key, int64_t version);

    RecordIdentifier(const RecordIdentifier&) = delete;
    RecordIdentifier& operator=(const RecordIdentifier&) = delete;

    ~RecordIdentifier();

    const std::string& primary_key() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return primary_key_;
    }
    int64_t version() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return version_;
    }

    void Reset(std::string primary_key, int64_t version);

   private:
    // TODO(jsbell): Make it more clear that this is the *encoded* version of
    // the key.
    std::string primary_key_ GUARDED_BY_CONTEXT(sequence_checker_);
    int64_t version_ GUARDED_BY_CONTEXT(sequence_checker_) = -1;

    // Data members must be immutable or GUARDED_BY_CONTEXT(sequence_checker_).
    SEQUENCE_CHECKER(sequence_checker_);
  };

  // This class is not thread-safe.
  // All accessses to one instance must occur on the same sequence.
  class CONTENT_EXPORT Transaction {
   public:
    struct BlobWriteState {
      BlobWriteState();
      explicit BlobWriteState(int calls_left, BlobWriteCallback on_complete);
      ~BlobWriteState();
      int calls_left = 0;
      BlobWriteCallback on_complete;
    };

    Transaction(base::WeakPtr<BackingStore> backing_store,
                blink::mojom::IDBTransactionDurability durability,
                blink::mojom::IDBTransactionMode mode);

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    virtual ~Transaction();

    virtual void Begin(std::vector<PartitionedLock> locks);

    // CommitPhaseOne determines what blobs (if any) need to be written to disk
    // and updates the primary blob journal, and kicks off the async writing
    // of the blob files. In case of crash/rollback, the journal indicates what
    // files should be cleaned up.
    // The callback will be called eventually on success or failure, or
    // immediately if phase one is complete due to lack of any blobs to write.
    virtual Status CommitPhaseOne(BlobWriteCallback callback);

    // CommitPhaseTwo is called once the blob files (if any) have been written
    // to disk, and commits the actual transaction to the backing store,
    // including blob journal updates, then deletes any blob files deleted
    // by the transaction and not referenced by running scripts.
    virtual Status CommitPhaseTwo();

    virtual void Rollback();

    void Reset();
    Status PutExternalObjectsIfNeeded(int64_t database_id,
                                      const std::string& object_store_data_key,
                                      std::vector<IndexedDBExternalObject>*);
    void PutExternalObjects(int64_t database_id,
                            const std::string& object_store_data_key,
                            std::vector<IndexedDBExternalObject>*);

    TransactionalLevelDBTransaction* transaction() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return transaction_.get();
    }

    virtual uint64_t GetTransactionSize();

    Status GetExternalObjectsForRecord(int64_t database_id,
                                       const std::string& object_store_data_key,
                                       IndexedDBValue* value);

    base::WeakPtr<Transaction> AsWeakPtr();

    blink::mojom::IDBTransactionDurability durability() const {
      return durability_;
    }
    blink::mojom::IDBTransactionMode mode() const { return mode_; }

    BackingStore* backing_store() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return backing_store_.get();
    }

   private:
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

    // This does NOT mean that this class can outlive the BackingStore.
    // This is only to protect against security issues before this class is
    // refactored away and this isn't necessary.
    // https://crbug.com/1012918
    base::WeakPtr<BackingStore> backing_store_
        GUARDED_BY_CONTEXT(sequence_checker_);

    scoped_refptr<TransactionalLevelDBTransaction> transaction_
        GUARDED_BY_CONTEXT(sequence_checker_);

    std::map<std::string, std::unique_ptr<IndexedDBExternalObjectChangeRecord>>
        external_object_change_map_ GUARDED_BY_CONTEXT(sequence_checker_);
    std::map<std::string, std::unique_ptr<IndexedDBExternalObjectChangeRecord>>
        in_memory_external_object_map_ GUARDED_BY_CONTEXT(sequence_checker_);
    int64_t database_id_ GUARDED_BY_CONTEXT(sequence_checker_) = -1;

    // List of blob files being newly written as part of this transaction.
    // These will be added to the recovery blob journal prior to commit, then
    // removed after a successful commit.
    BlobJournalType blobs_to_write_ GUARDED_BY_CONTEXT(sequence_checker_);

    // List of blob files being deleted as part of this transaction. These will
    // be added to either the recovery or live blob journal as appropriate
    // following a successful commit.
    BlobJournalType blobs_to_remove_ GUARDED_BY_CONTEXT(sequence_checker_);

    // Populated when blobs are being written to disk. This is saved here (as
    // opposed to being ephemeral and owned by the WriteBlobToFile callbacks)
    // because the transaction needs to be able to cancel this operation in
    // Rollback().
    std::optional<BlobWriteState> write_state_
        GUARDED_BY_CONTEXT(sequence_checker_);

    // Set to true between CommitPhaseOne and CommitPhaseTwo/Rollback, to
    // indicate that the committing_transaction_count_ on the backing store
    // has been bumped, and journal cleaning should be deferred.
    bool committing_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

    // This flag is passed to LevelDBScopes as `sync_on_commit`, converted
    // via ShouldSyncOnCommit.
    const blink::mojom::IDBTransactionDurability durability_;
    const blink::mojom::IDBTransactionMode mode_;

    // Data members must be immutable or GUARDED_BY_CONTEXT(sequence_checker_).
    SEQUENCE_CHECKER(sequence_checker_);

    base::WeakPtrFactory<Transaction> weak_ptr_factory_
        GUARDED_BY_CONTEXT(sequence_checker_){this};
  };

  // This class is not thread-safe.
  // All accessses to one instance must occur on the same sequence.
  class Cursor {
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

    virtual ~Cursor();

    const blink::IndexedDBKey& key() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return *current_key_;
    }

    bool Continue(Status* s) { return Continue(nullptr, nullptr, SEEK, s); }
    bool Continue(const blink::IndexedDBKey* key,
                  IteratorState state,
                  Status* s) {
      return Continue(key, nullptr, state, s);
    }
    bool Continue(const blink::IndexedDBKey* key,
                  const blink::IndexedDBKey* primary_key,
                  IteratorState state,
                  Status*);
    bool Advance(uint32_t count, Status*);
    bool FirstSeek(Status*);

    // Clone may return a nullptr if cloning fails for any reason.
    virtual std::unique_ptr<Cursor> Clone() const = 0;
    virtual const blink::IndexedDBKey& primary_key() const;
    virtual IndexedDBValue* value() = 0;
    virtual const RecordIdentifier& record_identifier() const;
    virtual bool LoadCurrentRow(Status* s) = 0;

   protected:
    Cursor(base::WeakPtr<Transaction> transaction,
           int64_t database_id,
           const CursorOptions& cursor_options);
    explicit Cursor(const Cursor* other,
                    std::unique_ptr<TransactionalLevelDBIterator> iterator);

    // May return nullptr.
    static std::unique_ptr<TransactionalLevelDBIterator> CloneIterator(
        const Cursor* other);

    virtual std::string EncodeKey(const blink::IndexedDBKey& key) = 0;
    virtual std::string EncodeKey(const blink::IndexedDBKey& key,
                                  const blink::IndexedDBKey& primary_key) = 0;

    bool IsPastBounds() const;
    bool HaveEnteredRange() const;

    // This does NOT mean that this class can outlive the Transaction.
    // This is only to protect against security issues before this class is
    // refactored away and this isn't necessary.
    // https://crbug.com/1012918
    const base::WeakPtr<Transaction> transaction_;
    const int64_t database_id_;
    const CursorOptions cursor_options_;
    std::unique_ptr<TransactionalLevelDBIterator> iterator_
        GUARDED_BY_CONTEXT(sequence_checker_);
    std::unique_ptr<blink::IndexedDBKey> current_key_
        GUARDED_BY_CONTEXT(sequence_checker_);
    RecordIdentifier record_identifier_ GUARDED_BY_CONTEXT(sequence_checker_);

    // Data members must be immutable or GUARDED_BY_CONTEXT(sequence_checker_).
    SEQUENCE_CHECKER(sequence_checker_);

   private:
    enum class ContinueResult { LEVELDB_ERROR, DONE, OUT_OF_BOUNDS };

    // For cursors with direction Next or NextNoDuplicate.
    ContinueResult ContinueNext(const blink::IndexedDBKey* key,
                                const blink::IndexedDBKey* primary_key,
                                IteratorState state,
                                Status*);
    // For cursors with direction Prev or PrevNoDuplicate. The PrevNoDuplicate
    // case has additional complexity of not being symmetric with
    // NextNoDuplicate.
    ContinueResult ContinuePrevious(const blink::IndexedDBKey* key,
                                    const blink::IndexedDBKey* primary_key,
                                    IteratorState state,
                                    Status*);

    base::WeakPtrFactory<Cursor> weak_factory_
        GUARDED_BY_CONTEXT(sequence_checker_){this};
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
               TransactionalLevelDBFactory& transactional_leveldb_factory,
               std::unique_ptr<TransactionalLevelDBDatabase> db,
               BlobFilesCleanedCallback blob_files_cleaned,
               ReportOutstandingBlobsCallback report_outstanding_blobs,
               scoped_refptr<base::SequencedTaskRunner> idb_task_runner);

  BackingStore(const BackingStore&) = delete;
  BackingStore& operator=(const BackingStore&) = delete;

  virtual ~BackingStore();

  // Initializes the backing store. This must be called before doing any
  // operations or method calls on this object.
  Status Initialize(bool clean_active_blob_journal);

  virtual void TearDown(base::WaitableEvent* signal_on_destruction);

  const storage::BucketLocator& bucket_locator() const {
    return bucket_locator_;
  }
  base::SequencedTaskRunner* idb_task_runner() const {
    return idb_task_runner_.get();
  }
  ActiveBlobRegistry* active_blob_registry() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return active_blob_registry_.get();
  }
  TransactionalLevelDBFactory& transactional_leveldb_factory() const {
    return *transactional_leveldb_factory_;
  }

  // Virtual for testing.
  virtual void Compact();
  // Creates a new database in the backing store. `metadata` is an in-out param.
  // The `name` and `version` fields are inputs, while the `id` and
  // `max_object_store_id` fields are outputs.
  virtual Status CreateDatabase(blink::IndexedDBDatabaseMetadata& metadata);
  virtual Status DeleteDatabase(const std::u16string& name,
                                std::vector<PartitionedLock> locks,
                                base::OnceClosure on_complete);
  // Changes the database version to |version|.
  [[nodiscard]] virtual Status SetDatabaseVersion(
      Transaction* transaction,
      int64_t row_id,
      int64_t version,
      blink::IndexedDBDatabaseMetadata* metadata);

  static bool RecordCorruptionInfo(const base::FilePath& path_base,
                                   const storage::BucketLocator& bucket_locator,
                                   const std::string& message);

  [[nodiscard]] virtual Status CreateObjectStore(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      std::u16string name,
      blink::IndexedDBKeyPath key_path,
      bool auto_increment,
      blink::IndexedDBObjectStoreMetadata* metadata);
  [[nodiscard]] virtual Status DeleteObjectStore(
      Transaction* transaction,
      int64_t database_id,
      const blink::IndexedDBObjectStoreMetadata& object_store);
  [[nodiscard]] virtual Status RenameObjectStore(
      Transaction* transaction,
      int64_t database_id,
      std::u16string new_name,
      std::u16string* old_name,
      blink::IndexedDBObjectStoreMetadata* metadata);

  // Creates a new index metadata and writes it to the transaction.
  [[nodiscard]] virtual Status CreateIndex(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      std::u16string name,
      blink::IndexedDBKeyPath key_path,
      bool is_unique,
      bool is_multi_entry,
      blink::IndexedDBIndexMetadata* metadata);
  // Deletes the index metadata on the transaction (but not any index entries).
  [[nodiscard]] virtual Status DeleteIndex(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBIndexMetadata& metadata);
  // Renames the given index and writes it to the transaction.
  [[nodiscard]] virtual Status RenameIndex(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      std::u16string new_name,
      std::u16string* old_name,
      blink::IndexedDBIndexMetadata* metadata);

  [[nodiscard]] virtual Status GetRecord(Transaction* transaction,
                                         int64_t database_id,
                                         int64_t object_store_id,
                                         const blink::IndexedDBKey& key,
                                         IndexedDBValue* record);
  [[nodiscard]] virtual Status PutRecord(Transaction* transaction,
                                         int64_t database_id,
                                         int64_t object_store_id,
                                         const blink::IndexedDBKey& key,
                                         IndexedDBValue* value,
                                         RecordIdentifier* record);
  [[nodiscard]] virtual Status ClearObjectStore(Transaction* transaction,
                                                int64_t database_id,
                                                int64_t object_store_id);
  [[nodiscard]] virtual Status DeleteRecord(Transaction* transaction,
                                            int64_t database_id,
                                            int64_t object_store_id,
                                            const RecordIdentifier& record);
  [[nodiscard]] virtual Status DeleteRange(Transaction* transaction,
                                           int64_t database_id,
                                           int64_t object_store_id,
                                           const blink::IndexedDBKeyRange&);
  [[nodiscard]] virtual Status GetKeyGeneratorCurrentNumber(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t* current_number);
  [[nodiscard]] virtual Status MaybeUpdateKeyGeneratorCurrentNumber(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t new_state,
      bool check_current);
  [[nodiscard]] virtual Status KeyExistsInObjectStore(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKey& key,
      RecordIdentifier* found_record_identifier,
      bool* found);

  [[nodiscard]] virtual Status ClearIndex(Transaction* transaction,
                                          int64_t database_id,
                                          int64_t object_store_id,
                                          int64_t index_id);
  [[nodiscard]] virtual Status PutIndexDataForRecord(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      const RecordIdentifier& record);
  [[nodiscard]] virtual Status GetPrimaryKeyViaIndex(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      std::unique_ptr<blink::IndexedDBKey>* primary_key);
  [[nodiscard]] virtual Status KeyExistsInIndex(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      std::unique_ptr<blink::IndexedDBKey>* found_primary_key,
      bool* exists);

  // Fill in the provided list with existing database names.
  [[nodiscard]] Status GetDatabaseNames(std::vector<std::u16string>* names);
  // Fill in the provided list with existing database names and versions.
  [[nodiscard]] Status GetDatabaseNamesAndVersions(
      std::vector<blink::mojom::IDBNameAndVersionPtr>* names_and_versions);
  // Reads in metadata for the database and all object stores & indices.
  // Note: the database name is not populated in |metadata|. Virtual for
  // testing.
  [[nodiscard]] virtual Status ReadMetadataForDatabaseName(
      const std::u16string& name,
      blink::IndexedDBDatabaseMetadata* metadata,
      bool* found);

  base::FilePath GetBlobFileName(int64_t database_id, int64_t key) const;

  virtual std::unique_ptr<Cursor> OpenObjectStoreKeyCursor(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      Status*);
  virtual std::unique_ptr<Cursor> OpenObjectStoreCursor(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      Status*);
  virtual std::unique_ptr<Cursor> OpenIndexKeyCursor(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      Status*);
  virtual std::unique_ptr<Cursor> OpenIndexCursor(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      Status*);

  TransactionalLevelDBDatabase* db() { return db_.get(); }

  const std::string& origin_identifier() { return origin_identifier_; }

  // Gets the total size of blobs and the database for in-memory backing stores.
  int64_t GetInMemorySize() const;

#if DCHECK_IS_ON()
  int NumBlobFilesDeletedForTesting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return num_blob_files_deleted_;
  }
  int NumAggregatedJournalCleaningRequestsForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return num_aggregated_journal_cleaning_requests_;
  }
#endif
  void SetExecuteJournalCleaningOnNoTransactionsForTesting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    execute_journal_cleaning_on_no_txns_ = true;
  }
  void WriteToIndexedDBForTesting(const std::string& key,
                                  const std::string& value);

  // Returns true if a blob cleanup job is pending on journal_cleaning_timer_.
  bool IsBlobCleanupPending();

  // Stops the journal_cleaning_timer_ and runs its pending task.
  void ForceRunBlobCleanup();

  bool in_memory() const { return backing_store_mode_ == Mode::kInMemory; }

  virtual std::unique_ptr<Transaction> CreateTransaction(
      blink::mojom::IDBTransactionDurability durability,
      blink::mojom::IDBTransactionMode mode);

  base::WeakPtr<BackingStore> AsWeakPtr() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return weak_factory_.GetWeakPtr();
  }

  static bool ShouldSyncOnCommit(
      blink::mojom::IDBTransactionDurability durability);

  // Create and initialize a BackingStore; verify and report its status.
  static std::tuple<std::unique_ptr<BackingStore>,
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

  // Delete LevelDB files; used to handle corruptions.
  static Status DestroyDatabase(const base::FilePath file_path);

 protected:
  friend class BucketContext;

  void set_bucket_context(BucketContext* bucket_context) {
    bucket_context_ = bucket_context;
  }

  Status AnyDatabaseContainsBlobs(bool* blobs_exist);

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

  // TODO(dmurph): Move this completely to IndexedDBMetadataFactory.
  Status GetCompleteMetadata(
      std::vector<blink::IndexedDBDatabaseMetadata>* output);

  // Remove the referenced file on disk.
  virtual bool RemoveBlobFile(int64_t database_id, int64_t key) const;

  // Schedule a call to CleanRecoveryJournalIgnoreReturn() via
  // an owned timer. If this object is destroyed, the timer
  // will automatically be cancelled.
  virtual void StartJournalCleaningTimer();

  // Attempt to clean the recovery journal. This will remove
  // any referenced files and delete the journal entry. If any
  // transaction is currently committing this will be deferred
  // via StartJournalCleaningTimer().
  void CleanRecoveryJournalIgnoreReturn();

 private:
  FRIEND_TEST_ALL_PREFIXES(BackingStoreTestWithExternalObjects,
                           ActiveBlobJournal);

  friend class AutoDidCommitTransaction;

  Status MigrateToV4(LevelDBWriteBatch* write_batch);
  Status MigrateToV5(LevelDBWriteBatch* write_batch);

  Status FindKeyInIndex(Transaction* transaction,
                        int64_t database_id,
                        int64_t object_store_id,
                        int64_t index_id,
                        const blink::IndexedDBKey& key,
                        std::string* found_encoded_primary_key,
                        bool* found);

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

  const scoped_refptr<base::SequencedTaskRunner> idb_task_runner_;
  std::map<std::string, std::unique_ptr<IndexedDBExternalObjectChangeRecord>>
      in_memory_external_object_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  bool execute_journal_cleaning_on_no_txns_
      GUARDED_BY_CONTEXT(sequence_checker_) = false;
  int num_aggregated_journal_cleaning_requests_
      GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  base::OneShotTimer journal_cleaning_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::TimeTicks journal_cleaning_timer_window_start_
      GUARDED_BY_CONTEXT(sequence_checker_);

#if DCHECK_IS_ON()
  mutable int num_blob_files_deleted_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
#endif

  // This factory is used to modify LevelDB behavior for tests. It's owned by
  // the bucket context even though ideally it would be owned by `this`, which
  // is due to poor encapsulation of LevelDB operations within `this`.
  raw_ref<TransactionalLevelDBFactory> transactional_leveldb_factory_;
  const std::unique_ptr<TransactionalLevelDBDatabase> db_;

  const BlobFilesCleanedCallback blob_files_cleaned_;

  // Whenever blobs are registered in active_blob_registry_,
  // indexed_db_factory_ will hold a reference to this backing store.
  std::unique_ptr<ActiveBlobRegistry> active_blob_registry_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Incremented whenever a transaction starts committing, decremented when
  // complete. While > 0, temporary journal entries may exist so out-of-band
  // journal cleaning must be deferred.
  size_t committing_transaction_count_ GUARDED_BY_CONTEXT(sequence_checker_) =
      0;

#if DCHECK_IS_ON()
  bool initialized_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
#endif

  // Data members must be immutable or GUARDED_BY_CONTEXT(sequence_checker_).
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BackingStore> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_H_
