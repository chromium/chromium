// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_H_

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/types/expected.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "content/browser/indexed_db/indexed_db_external_object_storage.h"
#include "content/browser/indexed_db/status.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace base {
class WaitableEvent;
}

namespace content::indexed_db {

struct IndexedDBDataLossInfo;
struct IndexedDBValue;

// NB: This interface is a WIP and is expected to experience heavy churn in the
// near future as additional interfaces are introduced to appropriately abstract
// the different database engines. See crbug.com/40273263.
//
// This interface abstracts the implementation of the data store for IDB data
// for a single bucket (which may contain many IDB databases). The current
// complete implementation uses LevelDB as its engine, along with a bespoke
// flat-file store for blobs, and is called level_db::BackingStore. In this
// system, each BackingStore corresponds to one LevelDB database.
//
// The SQLite version is a work in progress. There, each IDBDatabase correlates
// to a different database, so the BackingStore uses a collection of .sql
// files.
//
// Many of the methods here are likely to be moved to sibling interfaces that
// better encapsulate semantic objects. Eventually, all access to the
// BackingStore should be routed through this interface or its siblings rather
// than reaching directly into level_db::BackingStore.
class BackingStore {
 public:
  // Used to uniquely identify a record in the database. Can be treated as an
  // opaque token by consumers of the `BackingStore`.
  struct RecordIdentifier {
    // The meaning of these fields is backend-specific. Consumer code should
    // ignore them.
    // SQLite: a row id. LevelDB: a version.
    int64_t number;
    // SQLite and LevelDB: the *encoded* primary key bytes.
    std::string data;
  };

  class Cursor;
  class Transaction;

  // Represents a database in the backing store. A single Database may be
  // associated with many connections and transactions.
  class Database {
   public:
    virtual ~Database() = default;

    // Memory-cached metadata for this database.
    virtual const blink::IndexedDBDatabaseMetadata& GetMetadata() const = 0;

    // Returns info relating to any lost/corrupted data when this database was
    // opened.
    virtual const IndexedDBDataLossInfo& GetDataLossInfo() const = 0;

    // Generates the lock ID key for the given object store. Not called on
    // SQLite backing stores.
    virtual std::string GetObjectStoreLockIdKey(
        int64_t object_store_id) const = 0;

    // Creates a transaction on this database.
    virtual std::unique_ptr<Transaction> CreateTransaction(
        blink::mojom::IDBTransactionDurability durability,
        blink::mojom::IDBTransactionMode mode) = 0;

    // Deletes the database from the backing store and resets metadata to a
    // mostly uninitialized state. If the database does not exist, this should
    // return Status::OK() and `on_complete` need not be called. (The LevelDB
    // backing store does call it, which is harmless but unnecessary.)
    [[nodiscard]] virtual Status DeleteDatabase(
        std::vector<PartitionedLock> locks,
        base::OnceClosure on_complete) = 0;
  };

  // This interface wraps state and actions executed on the backing store by the
  // store-agnostic `Transaction`, and is to be implemented by backends such as
  // LevelDB or SQLite.
  // Each transaction is associated with a single `Database`.
  class Transaction {
   public:
    virtual ~Transaction() = default;

    virtual Status Begin(std::vector<PartitionedLock> locks) = 0;
    // CommitPhaseOne determines what blobs (if any) need to be written to disk
    // and updates the primary blob journal, and kicks off the async writing
    // of the blob files. In case of crash/rollback, the journal indicates what
    // files should be cleaned up.
    // The blob write callback will be called eventually on success or failure,
    // or immediately if phase one is complete due to lack of any blobs to
    // write.
    virtual Status CommitPhaseOne(
        BlobWriteCallback blob_write_callback,
        SerializeFsaCallback serialize_fsa_handle) = 0;
    // CommitPhaseTwo is called once the blob files (if any) have been written
    // to disk, and commits the actual transaction to the backing store,
    // including blob journal updates, then deletes any blob files deleted
    // by the transaction and not referenced by running scripts.
    virtual Status CommitPhaseTwo() = 0;
    virtual void Rollback() = 0;

    // Changes the database version to |version|.
    [[nodiscard]] virtual Status SetDatabaseVersion(int64_t version) = 0;
    [[nodiscard]] virtual Status CreateObjectStore(
        int64_t object_store_id,
        const std::u16string& name,
        blink::IndexedDBKeyPath key_path,
        bool auto_increment) = 0;
    [[nodiscard]] virtual Status DeleteObjectStore(int64_t object_store_id) = 0;
    [[nodiscard]] virtual Status RenameObjectStore(
        int64_t object_store_id,
        const std::u16string& new_name) = 0;
    // Removes all data contained in the given object store but keeps the object
    // store.
    [[nodiscard]] virtual Status ClearObjectStore(int64_t object_store_id) = 0;

    // Creates a new index metadata and writes it to the transaction.
    [[nodiscard]] virtual Status CreateIndex(
        int64_t object_store_id,
        blink::IndexedDBIndexMetadata index) = 0;

    // Deletes the index metadata on the transaction (but not any index
    // entries).
    [[nodiscard]] virtual Status DeleteIndex(int64_t object_store_id,
                                             int64_t index_id) = 0;
    // Renames the given index and writes it to the transaction.
    [[nodiscard]] virtual Status RenameIndex(
        int64_t object_store_id,
        int64_t index_id,
        const std::u16string& new_name) = 0;
    // When not found, the returned value is empty.
    [[nodiscard]] virtual StatusOr<IndexedDBValue> GetRecord(
        int64_t object_store_id,
        const blink::IndexedDBKey& key) = 0;
    // When successful, returns the identifier for the newly stored record.
    [[nodiscard]] virtual StatusOr<RecordIdentifier> PutRecord(
        int64_t object_store_id,
        const blink::IndexedDBKey& key,
        IndexedDBValue value) = 0;
    [[nodiscard]] virtual Status DeleteRange(
        int64_t object_store_id,
        const blink::IndexedDBKeyRange&) = 0;
    [[nodiscard]] virtual StatusOr<int64_t> GetKeyGeneratorCurrentNumber(
        int64_t object_store_id) = 0;
    // Sets the key generator current number for `object_store_id` to
    // max(`new_number`, current number). `was_generated` is a hint that can be
    // used by implementations to skip reading the current number.
    [[nodiscard]] virtual Status MaybeUpdateKeyGeneratorCurrentNumber(
        int64_t object_store_id,
        int64_t new_number,
        bool was_generated) = 0;
    // Returns the `RecordIdentifier` for the record if the primary key exists
    // in the given object store. Returns `Status` on error. Returns nullopt if
    // no record exists with the given key.
    [[nodiscard]] virtual StatusOr<std::optional<RecordIdentifier>>
    KeyExistsInObjectStore(int64_t object_store_id,
                           const blink::IndexedDBKey& key) = 0;
    [[nodiscard]] virtual Status PutIndexDataForRecord(
        int64_t object_store_id,
        int64_t index_id,
        const blink::IndexedDBKey& key,
        const RecordIdentifier& record) = 0;
    // Returns the primary key of the first record (sorted by primary key) in
    // the index with key value `key`, if found. Returns a "none" key
    // (!IsValid()) if not found. Returns a `Status` on database error.
    [[nodiscard]] virtual StatusOr<blink::IndexedDBKey>
    GetFirstPrimaryKeyForIndexKey(int64_t object_store_id,
                                  int64_t index_id,
                                  const blink::IndexedDBKey& key) = 0;
    [[nodiscard]] virtual StatusOr<uint32_t> GetObjectStoreKeyCount(
        int64_t object_store_id,
        blink::IndexedDBKeyRange key_range) = 0;
    [[nodiscard]] virtual StatusOr<uint32_t> GetIndexKeyCount(
        int64_t object_store_id,
        int64_t index_id,
        blink::IndexedDBKeyRange key_range) = 0;
    virtual StatusOr<std::unique_ptr<Cursor>> OpenObjectStoreKeyCursor(
        int64_t object_store_id,
        const blink::IndexedDBKeyRange& key_range,
        blink::mojom::IDBCursorDirection) = 0;
    virtual StatusOr<std::unique_ptr<Cursor>> OpenObjectStoreCursor(
        int64_t object_store_id,
        const blink::IndexedDBKeyRange& key_range,
        blink::mojom::IDBCursorDirection) = 0;
    virtual StatusOr<std::unique_ptr<Cursor>> OpenIndexKeyCursor(
        int64_t object_store_id,
        int64_t index_id,
        const blink::IndexedDBKeyRange& key_range,
        blink::mojom::IDBCursorDirection) = 0;
    virtual StatusOr<std::unique_ptr<Cursor>> OpenIndexCursor(
        int64_t object_store_id,
        int64_t index_id,
        const blink::IndexedDBKeyRange& key_range,
        blink::mojom::IDBCursorDirection) = 0;
    // Builds a complete value to be passed to the renderer by creating external
    // objects for `value`. `deserialize_handle` can be used to help create FSA
    // handle external objects out of their serialized representations.
    virtual blink::mojom::IDBValuePtr BuildMojoValue(
        IndexedDBValue value,
        DeserializeFsaCallback deserialize_handle) = 0;
  };

  // Another interface to be implemented by a backend implementation.
  class Cursor {
   public:
    virtual ~Cursor() = default;

    virtual const blink::IndexedDBKey& GetKey() const = 0;
    virtual const blink::IndexedDBKey& GetPrimaryKey() const = 0;
    virtual blink::IndexedDBKey TakeKey() && = 0;
    virtual IndexedDBValue& GetValue() = 0;

    // Advances the cursor to a new row and loads the row data. If the input
    // keys are valid, advances the cursor to the row for `key` or `key` and
    // `primary_key`. Returns true on success, or false if no eligible row was
    // found. Returns an error if there was a DB error.
    virtual StatusOr<bool> Continue() = 0;
    virtual StatusOr<bool> Continue(const blink::IndexedDBKey& key,
                                    const blink::IndexedDBKey& primary_key) = 0;
    virtual StatusOr<bool> Advance(uint32_t count) = 0;

    // Saves the current position of the cursor.
    virtual void SavePosition() = 0;
    // Attempts to reset the cursor to the last saved position. The cursor
    // instance is no longer usable if the returned `Status` is not `ok()`. A
    // status of type `kInvalidArgument` indicates that the position was not
    // saved prior to this call.
    virtual Status TryResetToLastSavedPosition() = 0;
  };

  virtual ~BackingStore() = default;

  // The BucketContext deletes itself and the BackingStore when it has no
  // database or blob connections active (after a short timeout). This method
  // should return true if there are no connections and no blobs. Note that the
  // LevelDB store just returns true because the BucketContext implements the
  // logic for it. SQLite blobs are managed by the store itself, so this method
  // is necessary.
  // TODO(crbug.com/419203257): consider revisiting this logic since there's
  // very little memory to be reclaimed by deleting the SQLite BackingStore.
  virtual bool CanOpportunisticallyClose() const = 0;

  virtual void TearDown(base::WaitableEvent* signal_on_destruction) = 0;
  virtual void InvalidateBlobReferences() = 0;
  // Get tasks to be run after a BackingStore no longer has any connections.
  virtual void StartPreCloseTasks(base::OnceClosure on_done) = 0;
  virtual void StopPreCloseTasks() = 0;
  // Gets the total size of blobs and the database for in-memory backing
  // stores.
  virtual int64_t GetInMemorySize() const = 0;
  // Returns true iff a database with the given name exists, whether or not it's
  // currently open.
  [[nodiscard]] virtual StatusOr<bool> DatabaseExists(
      std::u16string_view name) = 0;
  // Returns a list of names of existing databases and their version numbers
  // (i.e. `IndexedDBDatabaseMetadata::version`), regardless of whether they're
  // currently open.
  [[nodiscard]]
  virtual StatusOr<std::vector<blink::mojom::IDBNameAndVersionPtr>>
  GetDatabaseNamesAndVersions() = 0;
  // Creates a new database in the backing store, or opens an existing one. If
  // pre-existing, the database's metadata will be populated from disk.
  // Otherwise the version will be initialized to NO_VERSION.
  [[nodiscard]] virtual StatusOr<std::unique_ptr<BackingStore::Database>>
  CreateOrOpenDatabase(const std::u16string& name) = 0;

  virtual uintptr_t GetIdentifierForMemoryDump() = 0;

  // Writes backing store files to disk in their long-term format, e.g. converts
  // a log to actual DB files.
  virtual void FlushForTesting() = 0;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_H_
