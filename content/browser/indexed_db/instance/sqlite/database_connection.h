// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_DATABASE_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_DATABASE_CONNECTION_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/sqlite/active_blob_streamer.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"
#include "content/browser/indexed_db/instance/sqlite/blob_writer.h"
#include "content/browser/indexed_db/status.h"
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
class BackingStoreDatabaseImpl;
class BackingStoreTransactionImpl;

// Owns the sole connection to the SQLite database that is backing a given
// IndexedDB database. Also owns the schema, operations and in-memory metadata
// for this database. BackingStore interface methods call into this class to
// perform the actual database operations.
class DatabaseConnection {
 public:
  // Opens the SQL database for the IndexedDB database with `name` at
  // `file_path`, creating it if it doesn't exist.
  static StatusOr<std::unique_ptr<DatabaseConnection>> Open(
      const std::u16string& name,
      const base::FilePath& file_path,
      BackingStoreImpl& backing_store);

  DatabaseConnection(const DatabaseConnection&) = delete;
  DatabaseConnection& operator=(const DatabaseConnection&) = delete;
  ~DatabaseConnection();

  const blink::IndexedDBDatabaseMetadata& metadata() const { return metadata_; }

  base::WeakPtr<DatabaseConnection> GetWeakPtr();

  // Exposed to `BackingStoreDatabaseImpl`.
  std::unique_ptr<BackingStoreTransactionImpl> CreateTransaction(
      base::PassKey<BackingStoreDatabaseImpl>,
      blink::mojom::IDBTransactionDurability durability,
      blink::mojom::IDBTransactionMode mode);

  void BeginTransaction(base::PassKey<BackingStoreTransactionImpl>,
                        const BackingStoreTransactionImpl& transaction);
  // In this phase, blobs, if any, are asynchronously written.
  Status CommitTransactionPhaseOne(
      base::PassKey<BackingStoreTransactionImpl>,
      const BackingStoreTransactionImpl& transaction,
      BlobWriteCallback callback);
  Status CommitTransactionPhaseTwo(
      base::PassKey<BackingStoreTransactionImpl>,
      const BackingStoreTransactionImpl& transaction);
  void RollBackTransaction(base::PassKey<BackingStoreTransactionImpl>,
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
  Status CreateIndex(base::PassKey<BackingStoreTransactionImpl>,
                     int64_t object_store_id,
                     blink::IndexedDBIndexMetadata index);

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
  Status DeleteRange(int64_t object_store_id, const blink::IndexedDBKeyRange&);
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
      const std::vector<IndexedDBExternalObject>& objects);

  // Called when the IDB database associated with this connection is deleted.
  // This should drop all data with the exception of active blobs, which may
  // keep `this` alive.
  void DeleteIdbDatabase(base::PassKey<BackingStoreDatabaseImpl>);

  // These are exposed for `RecordIterator`s to access `Statement` resources
  // associated with `db_`.
  // Returns a unique ID and a pointer to a `Statement` whose lifetime is
  // managed by `this`.
  std::tuple<uint64_t, sql::Statement*> CreateLongLivedStatement(
      std::string query);
  // Called when a statement is no longer needed by a `RecordIterator`.
  void ReleaseLongLivedStatement(uint64_t id);
  // May return `nullptr` if the statement has been destroyed.
  sql::Statement* GetLongLivedStatement(uint64_t id);

  // Also for internal use only; exposed for RecordIterator implementations.
  // This adds external objects to `value` which should later be further hooked
  // up via `CreateAllExternalObjects()`.
  IndexedDBValue AddExternalObjectMetadataToValue(IndexedDBValue value,
                                                  int64_t record_row_id);

 private:
  DatabaseConnection(std::unique_ptr<sql::Database> db,
                     std::unique_ptr<sql::MetaTable> meta_table,
                     blink::IndexedDBDatabaseMetadata metadata,
                     BackingStoreImpl& backing_store);

  // True when the database is in an early, partially initialized state,
  // containing schema but no data. This will be true when the database is first
  // created as well as when it's been deleted, but held open due to active blob
  // references. Note that in the latter case, the database will contain data
  // corresponding to active blobs, but no object stores, records, etc.
  bool IsZygotic() const;

  bool HasActiveVersionChangeTransaction() const {
    return metadata_snapshot_.has_value();
  }

  // Invoked by an owned `BlobWriter` when it's done writing, or has encountered
  // an error.
  void OnBlobWriteComplete(int64_t blob_row_id, bool success);

  // Called when a blob that was opened for reading stops being "active", i.e.
  // when `ActiveBlobStreamer` in `active_blobs_` no longer has connections.
  void OnBlobBecameInactive(int64_t blob_number);

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

  // Long-lived statements (those used for cursor iteration) are owned by `this`
  // to ensure that database resources are freed before closing `db_`.
  uint64_t next_statement_id_ = 0;
  std::map<uint64_t, std::unique_ptr<sql::Statement>> statements_;

  // Only set while a version change transaction is active.
  std::optional<blink::IndexedDBDatabaseMetadata> metadata_snapshot_;

  // blob_row_id to blob metadata. These are collected over the lifetime of a
  // single transaction as records with associated blobs are inserted into the
  // database. The contents of the blobs are not written until commit time. The
  // objects in this map are also used to vend bytes (via their connected mojo
  // remote) if the client reads a value after writing but before committing.
  // ("Pending" blobs.)
  std::map<int64_t, IndexedDBExternalObject> blobs_to_write_;

  // This map will be empty until `CommitTransactionPhaseOne()` is called, at
  // which point it will be populated with helper objects that feed the blob
  // bytes into the SQLite database. The map will be empty again after all blobs
  // are done writing successfully, or at least one has failed.
  std::map<int64_t, std::unique_ptr<BlobWriter>> blob_writers_;

  // This is non-null whenever `blob_writers_` is non-empty.
  BlobWriteCallback blob_write_callback_;

  // A blob is active when there's a live reference in some client. Every active
  // blob has a corresponding entry in this map. These blobs must keep `this`
  // alive since they're backed by the SQLite database.
  std::map<int64_t, std::unique_ptr<ActiveBlobStreamer>> active_blobs_;

  // TODO(crbug.com/419203257): this should invalidate its weak pointers when
  // `db_` is closed.
  base::WeakPtrFactory<DatabaseConnection> record_iterator_weak_factory_{this};

  // Only used for the callbacks passed to `blob_writers_`.
  base::WeakPtrFactory<DatabaseConnection> blob_writers_weak_factory_{this};

  base::WeakPtrFactory<DatabaseConnection> weak_factory_{this};
};

}  // namespace sqlite
}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_DATABASE_CONNECTION_H_
