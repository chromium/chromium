// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_H_

#include <list>
#include <memory>
#include <optional>
#include <string>
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
    // SQLite: unused. LevelDB: the *encoded* primary key bytes.
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
    virtual const blink::IndexedDBDatabaseMetadata& GetMetadata() = 0;

    // Generates a lock ID for the given object store.
    virtual PartitionedLockId GetLockId(int64_t object_store_id) const = 0;

    // Creates a transaction on this database.
    virtual std::unique_ptr<Transaction> CreateTransaction(
        blink::mojom::IDBTransactionDurability durability,
        blink::mojom::IDBTransactionMode mode) = 0;

    // Deletes the database from the backing store and resets metadata to a
    // mostly uninitialized state.
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

    // For now, refer to comments in level_db::BackingStore::Transaction for
    // documentation.
    virtual void Begin(std::vector<PartitionedLock> locks) = 0;
    virtual Status CommitPhaseOne(BlobWriteCallback callback) = 0;
    virtual Status CommitPhaseTwo() = 0;
    virtual void Rollback() = 0;

    // Called after the transaction is aborted or completed.
    // TODO(crbug.com/40253999): can this be removed in favor of deleting the
    // object?
    virtual void Reset() = 0;

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
    [[nodiscard]] virtual Status GetRecord(int64_t object_store_id,
                                           const blink::IndexedDBKey& key,
                                           IndexedDBValue* record) = 0;
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
    [[nodiscard]] virtual Status MaybeUpdateKeyGeneratorCurrentNumber(
        int64_t object_store_id,
        int64_t new_state,
        bool check_current) = 0;
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
    [[nodiscard]] virtual Status GetPrimaryKeyViaIndex(
        int64_t object_store_id,
        int64_t index_id,
        const blink::IndexedDBKey& key,
        std::unique_ptr<blink::IndexedDBKey>* primary_key) = 0;
    [[nodiscard]] virtual Status KeyExistsInIndex(
        int64_t object_store_id,
        int64_t index_id,
        const blink::IndexedDBKey& key,
        std::unique_ptr<blink::IndexedDBKey>* found_primary_key,
        bool* exists) = 0;
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
  };

  // Another interface to be implemented by a backend implementation.
  class Cursor {
   public:
    enum IteratorState { READY = 0, SEEK };

    virtual ~Cursor() = default;

    virtual const blink::IndexedDBKey& GetKey() const = 0;
    virtual const blink::IndexedDBKey& GetPrimaryKey() const = 0;
    virtual blink::IndexedDBKey TakeKey() && = 0;
    virtual IndexedDBValue& GetValue() = 0;

    virtual bool Continue(const blink::IndexedDBKey& key,
                          const blink::IndexedDBKey& primary_key,
                          IteratorState state,
                          Status*) = 0;
    virtual bool Advance(uint32_t count, Status*) = 0;
    // Clone may return a nullptr if cloning fails for any reason.
    virtual std::unique_ptr<Cursor> Clone() const = 0;

    bool Continue(Status* s) { return Continue({}, {}, SEEK, s); }
  };

  virtual ~BackingStore() = default;

  // Get tasks to be run after a BackingStore no longer has any connections.
  virtual void TearDown(base::WaitableEvent* signal_on_destruction) = 0;
  virtual void InvalidateBlobReferences() = 0;
  virtual void StartPreCloseTasks(base::OnceClosure on_done) = 0;
  virtual void StopPreCloseTasks() = 0;
  // Gets the total size of blobs and the database for in-memory backing
  // stores.
  virtual int64_t GetInMemorySize() const = 0;
  // Returns a list of names of existing databases, regardless of whether
  // they're currently open.
  [[nodiscard]] virtual StatusOr<std::vector<std::u16string>>
  GetDatabaseNames() = 0;
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
