// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BACKING_STORE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_active_blob_registry.h"
#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
#include "content/browser/indexed_db/leveldb/leveldb_iterator.h"
#include "content/browser/indexed_db/leveldb/leveldb_transaction.h"
#include "content/common/content_export.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}

namespace blink {
class IndexedDBKeyRange;
struct IndexedDBDatabaseMetadata;
}  // namespace blink

namespace storage {
class FileWriterDelegate;
}

namespace content {

class IndexedDBFactory;
class LevelDBComparator;
class LevelDBDatabase;
class LevelDBFactory;
struct IndexedDBDataLossInfo;
struct IndexedDBValue;

namespace indexed_db_backing_store_unittest {
class IndexedDBBackingStoreTest;
FORWARD_DECLARE_TEST(IndexedDBBackingStoreTest, ReadCorruptionInfo);
}  // namespace indexed_db_backing_store_unittest

enum class V2SchemaCorruptionStatus {
  kUnknown = 0,  // Due to other unknown/critical errors.
  kNo = 1,
  kYes = 2,
};

class CONTENT_EXPORT IndexedDBBackingStore
    : public base::RefCounted<IndexedDBBackingStore> {
 public:
  class CONTENT_EXPORT Comparator : public LevelDBComparator {
   public:
    int Compare(const base::StringPiece& a,
                const base::StringPiece& b) const override;
    const char* Name() const override;
  };

  class CONTENT_EXPORT RecordIdentifier {
   public:
    RecordIdentifier(const std::string& primary_key, int64_t version);
    RecordIdentifier();
    ~RecordIdentifier();

    const std::string& primary_key() const { return primary_key_; }
    int64_t version() const { return version_; }
    void Reset(const std::string& primary_key, int64_t version) {
      primary_key_ = primary_key;
      version_ = version;
    }

   private:
    // TODO(jsbell): Make it more clear that this is the *encoded* version of
    // the key.
    std::string primary_key_;
    int64_t version_;
    DISALLOW_COPY_AND_ASSIGN(RecordIdentifier);
  };

  enum class BlobWriteResult { FAILURE_ASYNC, SUCCESS_ASYNC, SUCCESS_SYNC };

  class BlobWriteCallback : public base::RefCounted<BlobWriteCallback> {
   public:
    // TODO(dmurph): Make all calls to this method async after measuring
    // performance.
    virtual leveldb::Status Run(BlobWriteResult result) = 0;

   protected:
    friend class base::RefCounted<BlobWriteCallback>;
    virtual ~BlobWriteCallback() {}
  };

  class BlobChangeRecord {
   public:
    BlobChangeRecord(const std::string& key, int64_t object_store_id);
    ~BlobChangeRecord();

    const std::string& key() const { return key_; }
    int64_t object_store_id() const { return object_store_id_; }
    void SetBlobInfo(std::vector<IndexedDBBlobInfo>* blob_info);
    std::vector<IndexedDBBlobInfo>& mutable_blob_info() { return blob_info_; }
    const std::vector<IndexedDBBlobInfo>& blob_info() const {
      return blob_info_;
    }
    std::unique_ptr<BlobChangeRecord> Clone() const;

   private:
    std::string key_;
    int64_t object_store_id_;
    std::vector<IndexedDBBlobInfo> blob_info_;
    DISALLOW_COPY_AND_ASSIGN(BlobChangeRecord);
  };

  class CONTENT_EXPORT Transaction {
   public:
    explicit Transaction(IndexedDBBackingStore* backing_store);
    virtual ~Transaction();

    virtual void Begin();

    // CommitPhaseOne determines what blobs (if any) need to be written to disk
    // and updates the primary blob journal, and kicks off the async writing
    // of the blob files. In case of crash/rollback, the journal indicates what
    // files should be cleaned up.
    // The callback will be called eventually on success or failure, or
    // immediately if phase one is complete due to lack of any blobs to write.
    virtual leveldb::Status CommitPhaseOne(scoped_refptr<BlobWriteCallback>);

    // CommitPhaseTwo is called once the blob files (if any) have been written
    // to disk, and commits the actual transaction to the backing store,
    // including blob journal updates, then deletes any blob files deleted
    // by the transaction and not referenced by running scripts.
    virtual leveldb::Status CommitPhaseTwo();

    virtual void Rollback();
    void Reset() {
      backing_store_ = NULL;
      transaction_ = NULL;
    }
    leveldb::Status PutBlobInfoIfNeeded(
        int64_t database_id,
        int64_t object_store_id,
        const std::string& object_store_data_key,
        std::vector<IndexedDBBlobInfo>*);
    void PutBlobInfo(int64_t database_id,
                     int64_t object_store_id,
                     const std::string& object_store_data_key,
                     std::vector<IndexedDBBlobInfo>*);

    LevelDBTransaction* transaction() { return transaction_.get(); }

    virtual uint64_t GetTransactionSize();

    leveldb::Status GetBlobInfoForRecord(
        int64_t database_id,
        const std::string& object_store_data_key,
        IndexedDBValue* value);

    // This holds a BlobEntryKey and the encoded IndexedDBBlobInfo vector stored
    // under that key.
    typedef std::vector<std::pair<BlobEntryKey, std::string> >
        BlobEntryKeyValuePairVec;

    class CONTENT_EXPORT WriteDescriptor {
     public:
      WriteDescriptor(const storage::BlobDataHandle* blob,
                      int64_t key,
                      int64_t size,
                      base::Time last_modified);
      WriteDescriptor(const base::FilePath& path,
                      int64_t key,
                      int64_t size,
                      base::Time last_modified);
      WriteDescriptor(const WriteDescriptor& other);
      ~WriteDescriptor();
      WriteDescriptor& operator=(const WriteDescriptor& other);

      bool is_file() const { return is_file_; }
      const storage::BlobDataHandle* blob() const {
        DCHECK(!is_file_);
        return &blob_.value();
      }
      const base::FilePath& file_path() const {
        DCHECK(is_file_);
        return file_path_;
      }
      int64_t key() const { return key_; }
      int64_t size() const { return size_; }
      base::Time last_modified() const { return last_modified_; }

     private:
      bool is_file_;
      base::Optional<storage::BlobDataHandle> blob_;
      base::FilePath file_path_;
      int64_t key_;
      int64_t size_;
      base::Time last_modified_;
    };

    class ChainedBlobWriter
        : public base::RefCountedThreadSafe<ChainedBlobWriter> {
     public:
      virtual void set_delegate(
          std::unique_ptr<storage::FileWriterDelegate> delegate) = 0;

      // TODO(ericu): Add a reason in the event of failure.
      virtual void ReportWriteCompletion(bool succeeded,
                                         int64_t bytes_written) = 0;

      virtual void Abort() = 0;

     protected:
      friend class base::RefCountedThreadSafe<ChainedBlobWriter>;
      virtual ~ChainedBlobWriter() {}
    };

    class ChainedBlobWriterImpl;

    typedef std::vector<WriteDescriptor> WriteDescriptorVec;

   private:
    class BlobWriteCallbackWrapper;

    // Called by CommitPhaseOne: Identifies the blob entries to write and adds
    // them to the primary blob journal directly (i.e. not as part of the
    // transaction). Populates blobs_to_write_.
    leveldb::Status HandleBlobPreTransaction(
        BlobEntryKeyValuePairVec* new_blob_entries,
        WriteDescriptorVec* new_files_to_write);

    // Called by CommitPhaseOne: Populates blob_files_to_remove_ by
    // determining which blobs are deleted as part of the transaction, and
    // adds blob entry cleanup operations to the transaction. Returns true on
    // success, false on failure.
    bool CollectBlobFilesToRemove();

    // Called by CommitPhaseOne: Kicks off the asynchronous writes of blobs
    // identified in HandleBlobPreTransaction. The callback will be called
    // eventually on success or failure.
    void WriteNewBlobs(BlobEntryKeyValuePairVec* new_blob_entries,
                       WriteDescriptorVec* new_files_to_write,
                       scoped_refptr<BlobWriteCallback> callback);

    // Called by CommitPhaseTwo: Partition blob references in blobs_to_remove_
    // into live (active references) and dead (no references).
    void PartitionBlobsToRemove(BlobJournalType* dead_blobs,
                                BlobJournalType* live_blobs) const;

    IndexedDBBackingStore* backing_store_;
    scoped_refptr<LevelDBTransaction> transaction_;
    std::map<std::string, std::unique_ptr<BlobChangeRecord>> blob_change_map_;
    std::map<std::string, std::unique_ptr<BlobChangeRecord>>
        incognito_blob_map_;
    int64_t database_id_;

    // List of blob files being newly written as part of this transaction.
    // These will be added to the primary blob journal prior to commit, then
    // removed after a successful commit.
    BlobJournalType blobs_to_write_;

    // List of blob files being deleted as part of this transaction. These will
    // be added to either the primary or live blob journal as appropriate
    // following a successful commit.
    BlobJournalType blobs_to_remove_;
    scoped_refptr<ChainedBlobWriter> chained_blob_writer_;

    // Set to true between CommitPhaseOne and CommitPhaseTwo/Rollback, to
    // indicate that the committing_transaction_count_ on the backing store
    // has been bumped, and journal cleaning should be deferred.
    bool committing_;

    base::WeakPtrFactory<Transaction> ptr_factory_;

    DISALLOW_COPY_AND_ASSIGN(Transaction);
  };

  class Cursor {
   public:
    enum IteratorState { READY = 0, SEEK };

    virtual ~Cursor();

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
    };

    const blink::IndexedDBKey& key() const { return *current_key_; }
    bool Continue(leveldb::Status* s) { return Continue(NULL, NULL, SEEK, s); }
    bool Continue(const blink::IndexedDBKey* key,
                  IteratorState state,
                  leveldb::Status* s) {
      return Continue(key, NULL, state, s);
    }
    bool Continue(const blink::IndexedDBKey* key,
                  const blink::IndexedDBKey* primary_key,
                  IteratorState state,
                  leveldb::Status*);
    bool Advance(uint32_t count, leveldb::Status*);
    bool FirstSeek(leveldb::Status*);

    virtual std::unique_ptr<Cursor> Clone() const = 0;
    virtual const blink::IndexedDBKey& primary_key() const;
    virtual IndexedDBValue* value() = 0;
    virtual const RecordIdentifier& record_identifier() const;
    virtual bool LoadCurrentRow(leveldb::Status* s) = 0;

   protected:
    Cursor(scoped_refptr<IndexedDBBackingStore> backing_store,
           Transaction* transaction,
           int64_t database_id,
           const CursorOptions& cursor_options);
    explicit Cursor(const IndexedDBBackingStore::Cursor* other);

    virtual std::string EncodeKey(const blink::IndexedDBKey& key) = 0;
    virtual std::string EncodeKey(const blink::IndexedDBKey& key,
                                  const blink::IndexedDBKey& primary_key) = 0;

    bool IsPastBounds() const;
    bool HaveEnteredRange() const;

    IndexedDBBackingStore* backing_store_;
    Transaction* transaction_;
    int64_t database_id_;
    const CursorOptions cursor_options_;
    std::unique_ptr<LevelDBIterator> iterator_;
    std::unique_ptr<blink::IndexedDBKey> current_key_;
    IndexedDBBackingStore::RecordIdentifier record_identifier_;

   private:
    enum class ContinueResult { LEVELDB_ERROR, DONE, OUT_OF_BOUNDS };

    // For cursors with direction Next or NextNoDuplicate.
    ContinueResult ContinueNext(const blink::IndexedDBKey* key,
                                const blink::IndexedDBKey* primary_key,
                                IteratorState state,
                                leveldb::Status*);
    // For cursors with direction Prev or PrevNoDuplicate. The PrevNoDuplicate
    // case has additional complexity of not being symmetric with
    // NextNoDuplicate.
    ContinueResult ContinuePrevious(const blink::IndexedDBKey* key,
                                    const blink::IndexedDBKey* primary_key,
                                    IteratorState state,
                                    leveldb::Status*);

    DISALLOW_COPY_AND_ASSIGN(Cursor);
  };
  // Schedule an immediate blob journal cleanup if we reach this number of
  // requests.
  static constexpr const int kMaxJournalCleanRequests = 50;
  // Wait for a maximum of 5 seconds from the first call to the timer since the
  // last journal cleaning.
  static constexpr const base::TimeDelta kMaxJournalCleaningWindowTime =
      base::TimeDelta::FromSeconds(5);
  // Default to a 2 second timer delay before we clean up blobs.
  static constexpr const base::TimeDelta kInitialJournalCleaningWindowTime =
      base::TimeDelta::FromSeconds(2);

  const url::Origin& origin() const { return origin_; }
  IndexedDBFactory* factory() const { return indexed_db_factory_; }
  base::SequencedTaskRunner* task_runner() const { return task_runner_.get(); }
  base::OneShotTimer* close_timer() { return &close_timer_; }
  IndexedDBActiveBlobRegistry* active_blob_registry() {
    return &active_blob_registry_;
  }

  static scoped_refptr<IndexedDBBackingStore> Open(
      IndexedDBFactory* indexed_db_factory,
      const url::Origin& origin,
      const base::FilePath& path_base,
      IndexedDBDataLossInfo* data_loss_info,
      bool* disk_full,
      base::SequencedTaskRunner* task_runner,
      bool clean_journal,
      leveldb::Status* status);
  static scoped_refptr<IndexedDBBackingStore> Open(
      IndexedDBFactory* indexed_db_factory,
      const url::Origin& origin,
      const base::FilePath& path_base,
      IndexedDBDataLossInfo* data_loss_info,
      bool* disk_full,
      LevelDBFactory* leveldb_factory,
      base::SequencedTaskRunner* task_runner,
      bool clean_journal,
      leveldb::Status* status);
  static scoped_refptr<IndexedDBBackingStore> OpenInMemory(
      const url::Origin& origin,
      base::SequencedTaskRunner* task_runner,
      leveldb::Status* status);
  static scoped_refptr<IndexedDBBackingStore> OpenInMemory(
      const url::Origin& origin,
      LevelDBFactory* leveldb_factory,
      base::SequencedTaskRunner* task_runner,
      leveldb::Status* status);

  void GrantChildProcessPermissions(int child_process_id);

  // Compact is public for testing.
  virtual void Compact();
  virtual leveldb::Status DeleteDatabase(const base::string16& name);

  // Assumes caller has already closed the backing store.
  static leveldb::Status DestroyBackingStore(const base::FilePath& path_base,
                                             const url::Origin& origin);
  static bool RecordCorruptionInfo(const base::FilePath& path_base,
                                   const url::Origin& origin,
                                   const std::string& message);

  virtual leveldb::Status GetRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKey& key,
      IndexedDBValue* record) WARN_UNUSED_RESULT;
  virtual leveldb::Status PutRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKey& key,
      IndexedDBValue* value,
      RecordIdentifier* record) WARN_UNUSED_RESULT;
  virtual leveldb::Status ClearObjectStore(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id) WARN_UNUSED_RESULT;
  virtual leveldb::Status DeleteRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const RecordIdentifier& record) WARN_UNUSED_RESULT;
  virtual leveldb::Status DeleteRange(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKeyRange&) WARN_UNUSED_RESULT;
  virtual leveldb::Status GetKeyGeneratorCurrentNumber(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t* current_number) WARN_UNUSED_RESULT;
  virtual leveldb::Status MaybeUpdateKeyGeneratorCurrentNumber(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t new_state,
      bool check_current) WARN_UNUSED_RESULT;
  virtual leveldb::Status KeyExistsInObjectStore(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKey& key,
      RecordIdentifier* found_record_identifier,
      bool* found) WARN_UNUSED_RESULT;

  virtual leveldb::Status ClearIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id) WARN_UNUSED_RESULT;
  virtual leveldb::Status PutIndexDataForRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      const RecordIdentifier& record) WARN_UNUSED_RESULT;
  virtual leveldb::Status GetPrimaryKeyViaIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      std::unique_ptr<blink::IndexedDBKey>* primary_key) WARN_UNUSED_RESULT;
  virtual leveldb::Status KeyExistsInIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      std::unique_ptr<blink::IndexedDBKey>* found_primary_key,
      bool* exists) WARN_UNUSED_RESULT;

  // Public for IndexedDBActiveBlobRegistry::ReleaseBlobRef.
  virtual void ReportBlobUnused(int64_t database_id, int64_t blob_key);

  base::FilePath GetBlobFileName(int64_t database_id, int64_t key) const;

  virtual std::unique_ptr<Cursor> OpenObjectStoreKeyCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::WebIDBCursorDirection,
      leveldb::Status*);
  virtual std::unique_ptr<Cursor> OpenObjectStoreCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::WebIDBCursorDirection,
      leveldb::Status*);
  virtual std::unique_ptr<Cursor> OpenIndexKeyCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::WebIDBCursorDirection,
      leveldb::Status*);
  virtual std::unique_ptr<Cursor> OpenIndexCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::WebIDBCursorDirection,
      leveldb::Status*);

  IndexedDBPreCloseTaskQueue* pre_close_task_queue() {
    return pre_close_task_queue_.get();
  }

  void SetPreCloseTaskList(std::unique_ptr<IndexedDBPreCloseTaskQueue> list) {
    pre_close_task_queue_ = std::move(list);
  }

  // |pre_close_task_queue()| must not be null.
  void StartPreCloseTasks();

  LevelDBDatabase* db() { return db_.get(); }

  const std::string& origin_identifier() { return origin_identifier_; }

  // Returns true if a blob cleanup job is pending on journal_cleaning_timer_.
  bool IsBlobCleanupPending();

  int64_t GetInMemoryBlobSize() const;

#if DCHECK_IS_ON()
  int NumBlobFilesDeletedForTesting() { return num_blob_files_deleted_; }
  int NumAggregatedJournalCleaningRequestsForTesting() const {
    return num_aggregated_journal_cleaning_requests_;
  }
#endif

  // Stops the journal_cleaning_timer_ and runs its pending task.
  void ForceRunBlobCleanup();

  // HasV2SchemaCorruption() returns whether the backing store is v2 and
  // has blob references.
  V2SchemaCorruptionStatus HasV2SchemaCorruption();

  // RevertSchemaToV2() updates a backing store state on disk to override its
  // metadata version to 2.  This allows triggering https://crbug.com/829141 on
  // an otherwise healthy backing store.
  leveldb::Status RevertSchemaToV2();

 protected:
  friend class base::RefCounted<IndexedDBBackingStore>;

  IndexedDBBackingStore(
      IndexedDBFactory* indexed_db_factory,
      const url::Origin& origin,
      const base::FilePath& blob_path,
      std::unique_ptr<LevelDBDatabase> db,
      std::unique_ptr<LevelDBComparator> comparator,
      base::SequencedTaskRunner* task_runner);
  virtual ~IndexedDBBackingStore();

  bool is_incognito() const { return !indexed_db_factory_; }

  leveldb::Status AnyDatabaseContainsBlobs(LevelDBTransaction* transaction,
                                           bool* blobs_exist);

  leveldb::Status SetUpMetadata();

  // TODO(dmurph): Move this completely to IndexedDBMetadataFactory.
  leveldb::Status GetCompleteMetadata(
      std::vector<blink::IndexedDBDatabaseMetadata>* output);

  virtual bool WriteBlobFile(
      int64_t database_id,
      const Transaction::WriteDescriptor& descriptor,
      Transaction::ChainedBlobWriter* chained_blob_writer);

  // Remove the referenced file on disk.
  virtual bool RemoveBlobFile(int64_t database_id, int64_t key) const;

  // Schedule a call to CleanPrimaryJournalIgnoreReturn() via
  // an owned timer. If this object is destroyed, the timer
  // will automatically be cancelled.
  virtual void StartJournalCleaningTimer();

  // Attempt to clean the primary journal. This will remove
  // any referenced files and delete the journal entry. If any
  // transaction is currently committing this will be deferred
  // via StartJournalCleaningTimer().
  void CleanPrimaryJournalIgnoreReturn();

 private:
  FRIEND_TEST_ALL_PREFIXES(
      indexed_db_backing_store_unittest::IndexedDBBackingStoreTest,
      ReadCorruptionInfo);

  static scoped_refptr<IndexedDBBackingStore> Create(
      IndexedDBFactory* indexed_db_factory,
      const url::Origin& origin,
      const base::FilePath& blob_path,
      std::unique_ptr<LevelDBDatabase> db,
      std::unique_ptr<LevelDBComparator> comparator,
      base::SequencedTaskRunner* task_runner,
      leveldb::Status* status);

  static bool ReadCorruptionInfo(const base::FilePath& path_base,
                                 const url::Origin& origin,
                                 std::string* message);

  leveldb::Status FindKeyInIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      std::string* found_encoded_primary_key,
      bool* found);

  // Remove the blob directory for the specified database and all contained
  // blob files.
  bool RemoveBlobDirectory(int64_t database_id) const;

  // Synchronously read the key-specified blob journal entry from the backing
  // store, delete all referenced blob files, and erase the journal entry.
  // This must not be used while temporary entries are present e.g. during
  // a two-stage transaction commit with blobs.
  leveldb::Status CleanUpBlobJournal(const std::string& level_db_key) const;

  // Synchronously delete the files and/or directories on disk referenced by
  // the blob journal.
  leveldb::Status CleanUpBlobJournalEntries(
      const BlobJournalType& journal) const;

  void WillCommitTransaction();
  // Can run a journal cleaning job if one is pending.
  void DidCommitTransaction();

  IndexedDBFactory* indexed_db_factory_;
  const url::Origin origin_;
  base::FilePath blob_path_;

  // The origin identifier is a key prefix unique to the origin used in the
  // leveldb backing store to partition data by origin. It is a normalized
  // version of the origin URL with a versioning suffix appended, e.g.
  // "http_localhost_81@1" Since only one origin is stored per backing store
  // this is redundant but necessary for backwards compatibility; the suffix
  // provides for future flexibility.
  const std::string origin_identifier_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::set<int> child_process_ids_granted_;
  std::map<std::string, std::unique_ptr<BlobChangeRecord>> incognito_blob_map_;

  bool execute_journal_cleaning_on_no_txns_ = false;
  int num_aggregated_journal_cleaning_requests_ = 0;
  base::OneShotTimer journal_cleaning_timer_;
  base::TimeTicks journal_cleaning_timer_window_start_;

#if DCHECK_IS_ON()
  mutable int num_blob_files_deleted_ = 0;
#endif

  std::unique_ptr<LevelDBDatabase> db_;
  std::unique_ptr<LevelDBComparator> comparator_;
  // Whenever blobs are registered in active_blob_registry_, indexed_db_factory_
  // will hold a reference to this backing store.
  IndexedDBActiveBlobRegistry active_blob_registry_;
  base::OneShotTimer close_timer_;
  std::unique_ptr<IndexedDBPreCloseTaskQueue> pre_close_task_queue_;

  // Incremented whenever a transaction starts committing, decremented when
  // complete. While > 0, temporary journal entries may exist so out-of-band
  // journal cleaning must be deferred.
  size_t committing_transaction_count_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBBackingStore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BACKING_STORE_H_
