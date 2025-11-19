// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_DATABASE_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_DATABASE_CONNECTION_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/byte_count.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_external_object_storage.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/sqlite/active_blob_streamer.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"
#include "content/browser/indexed_db/instance/sqlite/blob_writer.h"
#include "content/browser/indexed_db/status.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "sql/streaming_blob_handle.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace blink {
class IndexedDBKey;
}  // namespace blink

namespace sql {
class Database;
class MetaTable;
class Statement;
class Transaction;
}  // namespace sql

namespace content::indexed_db {
struct IndexedDBValue;

namespace sqlite {
class BackingStoreCursorImpl;
class BackingStoreDatabaseImpl;
class BackingStoreTransactionImpl;

// Owns the sole connection to the SQLite database that is backing a given
// IndexedDB database. Also owns the schema, operations and in-memory metadata
// for this database. BackingStore interface methods call into this class to
// perform the actual database operations.
class CONTENT_EXPORT DatabaseConnection {
 public:
  // Opens a connection to the specified database. When `name` is present, it
  // will create a new DB if one does not exist. When `name` is null and a DB
  // does not exist or is not already initialized, returns an error. When `path`
  // is empty, the database will be opened in-memory.
  static StatusOr<std::unique_ptr<DatabaseConnection>> Open(
      std::optional<std::u16string_view> name,
      base::FilePath path,
      BackingStoreImpl& backing_store);

  // Destroys the DatabaseConnection pointed to by `db`, if appropriate, i.e. if
  // `db` is the last weak pointer.
  static void Release(base::WeakPtr<DatabaseConnection> db);

  DatabaseConnection(const DatabaseConnection&) = delete;
  DatabaseConnection& operator=(const DatabaseConnection&) = delete;
  ~DatabaseConnection();

  const blink::IndexedDBDatabaseMetadata& metadata() const { return metadata_; }
  const IndexedDBDataLossInfo& data_loss_info() const {
    return data_loss_info_;
  }

  // Gets the version of the database that is actually committed. This can be
  // different from the version in `metadata_` during a version change
  // transaction.
  int64_t GetCommittedVersion() const;

  // True when the database is in an early, partially initialized state,
  // containing schema but no data. This will be true when the database is first
  // created as well as when it's been deleted, but held open due to active blob
  // references. Note that in the latter case, the database will contain data
  // corresponding to active blobs, but no object stores, records, etc.
  bool IsZygotic() const;

  // Get the size of the database opened in-memory.
  uint64_t GetInMemorySize() const;

  std::unique_ptr<BackingStoreDatabaseImpl> CreateDatabaseWrapper();

  // Exposed to `BackingStoreDatabaseImpl`.
  std::unique_ptr<BackingStoreTransactionImpl> CreateTransactionWrapper(
      base::PassKey<BackingStoreDatabaseImpl>,
      blink::mojom::IDBTransactionDurability durability,
      blink::mojom::IDBTransactionMode mode);

  Status BeginTransaction(base::PassKey<BackingStoreTransactionImpl>,
                          const BackingStoreTransactionImpl& transaction);
  // In this phase, blobs, if any, are asynchronously written.
  Status CommitTransactionPhaseOne(
      base::PassKey<BackingStoreTransactionImpl>,
      const BackingStoreTransactionImpl& transaction,
      BlobWriteCallback callback,
      SerializeFsaCallback serialize_fsa_handle);
  Status CommitTransactionPhaseTwo(
      base::PassKey<BackingStoreTransactionImpl>,
      const BackingStoreTransactionImpl& transaction);
  void RollBackTransaction(base::PassKey<BackingStoreTransactionImpl>,
                           const BackingStoreTransactionImpl& transaction);
  // It's possible that a BackingStoreTransactionImpl is created, and Begin() is
  // called, but it's never used. In this case, neither Commit nor Rollback will
  // be called. This method will be called every time a transaction that was
  // begun is being destroyed.
  void EndTransaction(base::PassKey<BackingStoreTransactionImpl>,
                      const BackingStoreTransactionImpl& transaction);

  Status SetDatabaseVersion(base::PassKey<BackingStoreTransactionImpl>,
                            int64_t version);
  Status CreateObjectStore(base::PassKey<BackingStoreTransactionImpl>,
                           int64_t object_store_id,
                           std::u16string name,
                           blink::IndexedDBKeyPath key_path,
                           bool auto_increment);
  Status DeleteObjectStore(base::PassKey<BackingStoreTransactionImpl>,
                           int64_t object_store_id);
  Status RenameObjectStore(base::PassKey<BackingStoreTransactionImpl>,
                           int64_t object_store_id,
                           const std::u16string& new_name);
  Status CreateIndex(base::PassKey<BackingStoreTransactionImpl>,
                     int64_t object_store_id,
                     blink::IndexedDBIndexMetadata index);
  Status DeleteIndex(base::PassKey<BackingStoreTransactionImpl>,
                     int64_t object_store_id,
                     int64_t index_id);
  Status RenameIndex(base::PassKey<BackingStoreTransactionImpl>,
                     int64_t object_store_id,
                     int64_t index_id,
                     const std::u16string& new_name);

  StatusOr<int64_t> GetKeyGeneratorCurrentNumber(
      base::PassKey<BackingStoreTransactionImpl>,
      int64_t object_store_id);
  // Updates the key generator current number of `object_store_id` to
  // `new_number` if greater than the current number.
  Status MaybeUpdateKeyGeneratorCurrentNumber(
      base::PassKey<BackingStoreTransactionImpl>,
      int64_t object_store_id,
      int64_t new_number);

  StatusOr<std::optional<BackingStore::RecordIdentifier>>
  GetRecordIdentifierIfExists(base::PassKey<BackingStoreTransactionImpl>,
                              int64_t object_store_id,
                              const blink::IndexedDBKey& key);
  // Returns an empty `IndexedDBValue` if the record is not found.
  StatusOr<IndexedDBValue> GetValue(base::PassKey<BackingStoreTransactionImpl>,
                                    int64_t object_store_id,
                                    const blink::IndexedDBKey& key);
  StatusOr<BackingStore::RecordIdentifier> PutRecord(
      base::PassKey<BackingStoreTransactionImpl>,
      int64_t object_store_id,
      const blink::IndexedDBKey& key,
      IndexedDBValue value);
  Status DeleteRange(base::PassKey<BackingStoreTransactionImpl>,
                     int64_t object_store_id,
                     const blink::IndexedDBKeyRange&);
  Status ClearObjectStore(base::PassKey<BackingStoreTransactionImpl>,
                          int64_t object_store_id);
  StatusOr<uint32_t> GetObjectStoreKeyCount(
      base::PassKey<BackingStoreTransactionImpl>,
      int64_t object_store_id,
      blink::IndexedDBKeyRange key_range);
  Status PutIndexDataForRecord(base::PassKey<BackingStoreTransactionImpl>,
                               int64_t object_store_id,
                               int64_t index_id,
                               const blink::IndexedDBKey& key,
                               const BackingStore::RecordIdentifier& record);
  StatusOr<blink::IndexedDBKey> GetFirstPrimaryKeyForIndexKey(
      base::PassKey<BackingStoreTransactionImpl>,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key);
  StatusOr<uint32_t> GetIndexKeyCount(
      base::PassKey<BackingStoreTransactionImpl>,
      int64_t object_store_id,
      int64_t index_id,
      blink::IndexedDBKeyRange key_range);

  StatusOr<std::unique_ptr<BackingStore::Cursor>> OpenObjectStoreCursor(
      base::PassKey<BackingStoreTransactionImpl>,
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection direction,
      bool key_only);
  StatusOr<std::unique_ptr<BackingStore::Cursor>> OpenIndexCursor(
      base::PassKey<BackingStoreTransactionImpl>,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection direction,
      bool key_only);

  // Connects mojo pipes for `objects`. These pipes are backed by
  // `ActiveBlobStreamer`.
  std::vector<blink::mojom::IDBExternalObjectPtr> CreateAllExternalObjects(
      base::PassKey<BackingStoreTransactionImpl>,
      const std::vector<IndexedDBExternalObject>& objects,
      DeserializeFsaCallback deserialize_fsa_handle);

  // Called when the IDB database associated with this connection is deleted.
  // This should drop all data with the exception of active blobs, which may
  // keep `this` alive.
  void DeleteIdbDatabase(base::PassKey<BackingStoreDatabaseImpl>);

  // These are exposed for cursors to access `Statement` resources associated
  // with `db_`.
  //
  // Returns a unique ID and a pointer to a `Statement` whose lifetime is
  // managed by `this`.
  std::tuple<uint64_t, sql::Statement*> CreateCursorStatement(
      base::PassKey<BackingStoreCursorImpl>,
      std::string query,
      int64_t object_store_id);
  // Called when a statement is no longer needed by the cursor that created it.
  void ReleaseCursorStatement(base::PassKey<BackingStoreCursorImpl>,
                              uint64_t id);
  // May return `nullptr` if the statement has been destroyed.
  sql::Statement* GetCursorStatement(base::PassKey<BackingStoreCursorImpl>,
                                     uint64_t id);

  // Returns a `Status` for the last operation on `db_`.
  // This is exposed for cursor implementations which `Step()` statements
  // outside of this class.
  Status GetStatusOfLastOperation(base::PassKey<BackingStoreCursorImpl>);

  // Also for internal use only; exposed for cursor implementations.
  // This adds external objects to `value` which should later be further hooked
  // up via `CreateAllExternalObjects()`.
  StatusOr<IndexedDBValue> AddExternalObjectMetadataToValue(
      IndexedDBValue value,
      int64_t record_row_id);

  // Decompresses bytes found in the database. Will return an error and mark the
  // database as corrupt on failure.
  StatusOr<mojo_base::BigBuffer> Decompress(
      base::span<const uint8_t> compressed,
      int compression_type);

  // Changes the size at which blobs are chunked.
  static void OverrideMaxBlobSizeForTesting(base::ByteCount size);

 private:
  friend class BackingStoreSqliteTest;
  FRIEND_TEST_ALL_PREFIXES(DatabaseConnectionTest, TooNew);

  DatabaseConnection(base::FilePath path, BackingStoreImpl& backing_store);

  bool in_memory() const { return path_.empty(); }

  // All startup/initialization tasks that can error are performed here. Will
  // return Status::OK() on success. `name` must be provided if the database is
  // new. If the database is pre-existing, `name` may not be provided, but if it
  // is, it must match the database's stored name.
  Status Init(std::optional<std::u16string_view> name);

  bool HasActiveVersionChangeTransaction() const {
    return metadata_snapshot_.has_value();
  }

  // Gets a handle to a blob in either the `blobs` table (when `chunk_index` is
  // 0) or the `overflow_blob_chunks` table, used for writing bytes that
  // overflow a single SQLite BLOB.
  std::optional<sql::StreamingBlobHandle> OpenBlobChunkForStreaming(
      int64_t blob_row_id,
      bool readonly,
      size_t chunk_index);

  // Invoked by an owned `BlobWriter` when it's done writing, or has encountered
  // an error.
  void OnBlobWriteComplete(int64_t blob_row_id, bool success);

  // Invoked when an FSA handle has been serialized. `token` will be empty if
  // the serialization was not successful.
  void OnFsaHandleSerialized(int64_t blob_row_id,
                             const std::vector<uint8_t>& token);

  // Cancels all outstanding external object processing/writing, including blob
  // writes and FSA handle serialization/writing. This is to be called on error.
  void CancelBlobWriting();

  // Called when a blob that was opened for reading stops being "active", i.e.
  // when `ActiveBlobStreamer` in `active_blobs_` no longer has connections.
  void OnBlobBecameInactive(int64_t blob_number);

  // This method adds a row to the `blob_references` table. The row corresponds
  // to an active blob, i.e. the `record_row_id` will be null. These updates are
  // made right away when `active_blobs_` is updated (an element is added or
  // removed), and also after a transaction is rolled back which may have caused
  // the loss of a `blob_references` update.
  bool AddActiveBlobReference(int64_t blob_number);

  // The connection needs to be held open when there are active blobs or an
  // active BackingStore::Database referencing it. This will return false if
  // that's the case. Even when this is false, `this` may be destroyed if the
  // `BucketContext` is force-closed.
  bool CanSelfDestruct() const;

  // Attempts to read metadata from the SQLite DB for storing in memory (in
  // `metadata_`).
  StatusOr<blink::IndexedDBDatabaseMetadata> GenerateIndexedDbMetadata();

  // This enum is used to track various events of interest, mostly errors.
  //
  // LINT.IfChange(SpecificEvent)
  enum class SpecificEvent : uint8_t {
    // Logged once per database connection, when initializing.
    kDatabaseOpenAttempt = 0,
    // Logged at most once per database connection, at shutdown time.
    kDatabaseHadSqlError = 1,

    // These errors correlate to points in the code where a SQLite error may
    // occur, but cannot easily be reported to the frontend because they are not
    // directly associated with an ongoing request. Most of them correlate with
    // blob bookkeeping, and the worst thing that can happen is that reading
    // from a blob may throw errors or that blob data may persist on disk until
    // the next time the DB is opened.
    kSyncActiveBlobsFailed = 2,
    kOpenBlobForStreamingFailed = 3,
    kAddActiveBlobReferenceFailed = 4,
    kRemoveActiveBlobReferenceFailed = 5,
    kPragmaPageCountFailed = 6,
    kPragmaPageSizeFailed = 7,

    // Events associated with various callers of `Fatal()`.
    kMissingMetadataTable = 8,
    kDatabaseTooNew = 9,
    kDatabaseSchemaUnknown = 10,
    kDatabaseNameMismatch = 11,
    kBlobChunkMissing = 12,
    kObjectStoreNotFound = 13,
    kBlobTypeUnknown = 14,
    kV8FormatTooNewOrMissing = 15,
    kUtf16StringUnreadable = 16,
    kDecompressionFailure = 17,

    kMaxValue = kDecompressionFailure,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:IndexedDbSqliteSpecificEvent)

  void LogEvent(SpecificEvent event) const;

  // Called when a logical inconsistency or other irrecoverable state is
  // detected. This could be due to a bug or due to disk corruption. This will
  // not/should not be called when SQLite reports an error. If SQLite does not
  // report an error, but a logical inconsistency is found in the database, we
  // assume that recovering will fail. Therefore this function marks the
  // database for deletion.
  Status Fatal(Status s, SpecificEvent event);

  // Called when the records of an object store have been modified (inserted or
  // deleted). This invalidates all cursor statements operating on that store.
  void OnRecordsModified(int64_t object_store_id);

  // Makes sure the given IDs exist in `metadata_`.
  void ValidateInputs(int64_t object_store_id, int64_t index_id);

  // The expected path for `db_`, or empty for in-memory DBs.
  const base::FilePath path_;

  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<sql::MetaTable> meta_table_;
  blink::IndexedDBDatabaseMetadata metadata_;
  raw_ref<BackingStoreImpl> backing_store_;

  // A `sql::Transaction` is created only for version change and readwrite
  // IndexedDB transactions, only one of which is allowed to run concurrently,
  // irrespective of the scope* (this is enforced by `PartitionedLockManager`).
  // Readonly IndexedDB transactions that don't overlap with the current
  // readwrite transaction run concurrently, executing their statements in the
  // context of the active `sql::Transaction` if it exists, else as standalone
  // statements with no explicit `sql::Transaction`.
  //
  // *This is because SQLite allows only one active (readwrite) transaction on a
  // database at a time.
  std::unique_ptr<sql::Transaction> active_rw_transaction_;

  // Cursor statements are owned by `this` to ensure that database resources are
  // freed before closing `db_`. See `BackingStoreImpl::GetStatement()` for why
  // cursor statements are not ephemeral (unlike other statements).
  //
  // The object store ID is also stored alongside the `sql::Statement` so that
  // the statement can be invalidated when records change.
  // TODO(crbug.com/436880910): Consider also storing the `IndexedDBKeyRange` of
  // the statement for more precise invalidation.
  using CursorStatementHolder =
      std::tuple<std::unique_ptr<sql::Statement>, int64_t>;
  uint64_t next_statement_id_ = 0;
  std::map<uint64_t, CursorStatementHolder> cursor_statements_;

  // Only set while a version change transaction is active.
  std::optional<blink::IndexedDBDatabaseMetadata> metadata_snapshot_;

  // blob_row_id to blob metadata. These are collected over the lifetime of a
  // single transaction as records with associated blobs are inserted into the
  // database. The contents of the blobs are not written until commit time. The
  // objects in this map are also used to vend bytes (via their connected mojo
  // remote) if the client reads a value after writing but before committing.
  // ("Pending" blobs.) Note that some of these blobs may be associated with
  // records that were added and later deleted (or replaced) in the same commit.
  // A check to verify the blobs are still needed is performed at commit time.
  std::map<int64_t, IndexedDBExternalObject> blobs_staged_for_commit_;

  // This map will be empty until `CommitTransactionPhaseOne()` is called, at
  // which point it will be populated with helper objects that feed the blob
  // bytes into the SQLite database. The map will be empty again after all blobs
  // are done writing successfully, or at least one has failed.
  std::map<int64_t, std::unique_ptr<BlobWriter>> blob_writers_;

  // Tracks the number of currently existing operations that will write blobs
  // into the database, resulting from a call to WriteNewBlobs(). This will be
  // the sum of `blob_writers_.size()` and the number of FSA handle
  // serialization operations that have not yet finished. This is eventually the
  // same as the number of weak pointers currently vended from
  // `blob_writers_weak_factory_`, but will be updated *while* an operation
  // bound to such a weak pointer is executed (whereas the weak pointer itself
  // will be destroyed only *after* the operation completes).
  size_t outstanding_external_object_writes_ = 0U;

  // This is non-null whenever `blob_writers_` is non-empty.
  BlobWriteCallback blob_write_callback_;

  // A blob is active when there's a live reference in some client. Every active
  // blob has a corresponding entry in this map. These blobs must keep `this`
  // alive since they're backed by the SQLite database.
  std::map<int64_t, std::unique_ptr<ActiveBlobStreamer>> active_blobs_;

  // Used to track when rolling back a transaction necessitates updating
  // `blob_references`. Transaction rollback will affect `blob_references`
  // updates that have been made since the transaction started, but we need that
  // table to stay in sync with `active_blobs_` regardless of whether the
  // transaction is ultimately committed or rolled back.
  bool sync_active_blobs_after_transaction_ = false;

  // True once `DeleteIdbDatabase` has been called, or if a fatal error occurred
  // that we can't recover from.
  bool marked_for_permanent_deletion_ = false;

  // Information relating to any previous data that may have been lost while
  // attempting to open this database.
  IndexedDBDataLossInfo data_loss_info_;

  // TODO(crbug.com/419203257): this should invalidate its weak pointers when
  // `db_` is closed.
  base::WeakPtrFactory<DatabaseConnection> cursor_weak_factory_{this};

  // Only used for the callbacks passed to `blob_writers_`.
  base::WeakPtrFactory<DatabaseConnection> blob_writers_weak_factory_{this};

  // Used to vend pointers to the interfaces within `BackingStore`.
  base::WeakPtrFactory<DatabaseConnection> interface_wrapper_weak_factory_{
      this};
};

}  // namespace sqlite
}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_DATABASE_CONNECTION_H_
