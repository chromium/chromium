// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_DATABASE_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_DATABASE_CONNECTION_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/status.h"
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
      const base::FilePath& file_path);

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

  // Hooks called by `BackingStoreTransactionImpl`.
  void OnTransactionBegin(base::PassKey<BackingStoreTransactionImpl>,
                          const BackingStoreTransactionImpl& transaction);
  void OnBeforeTransactionCommit(
      base::PassKey<BackingStoreTransactionImpl>,
      const BackingStoreTransactionImpl& transaction);
  void OnTransactionCommit(base::PassKey<BackingStoreTransactionImpl>,
                           const BackingStoreTransactionImpl& transaction);
  void OnTransactionRollback(base::PassKey<BackingStoreTransactionImpl>,
                             const BackingStoreTransactionImpl& transaction);

  Status SetDatabaseVersion(base::PassKey<BackingStoreTransactionImpl>,
                            int64_t version);
  Status CreateObjectStore(base::PassKey<BackingStoreTransactionImpl>,
                           int64_t object_store_id,
                           std::u16string name,
                           blink::IndexedDBKeyPath key_path,
                           bool auto_increment);

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
  // Inserts a new record, removing the older one corresponding to
  // (`object_store_id`, `key`) if it existed.
  StatusOr<BackingStore::RecordIdentifier> PutRecord(
      base::PassKey<BackingStoreTransactionImpl>,
      int64_t object_store_id,
      const blink::IndexedDBKey& key,
      IndexedDBValue value);
  StatusOr<uint32_t> GetObjectStoreKeyCount(
      base::PassKey<BackingStoreTransactionImpl>,
      int64_t object_store_id,
      blink::IndexedDBKeyRange key_range);

 private:
  DatabaseConnection(std::unique_ptr<sql::Database> db,
                     std::unique_ptr<sql::MetaTable> meta_table,
                     blink::IndexedDBDatabaseMetadata metadata);

  bool HasActiveVersionChangeTransaction() const {
    return metadata_snapshot_.has_value();
  }

  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<sql::MetaTable> meta_table_;
  blink::IndexedDBDatabaseMetadata metadata_;

  // Only set while a version change transaction is active.
  std::optional<blink::IndexedDBDatabaseMetadata> metadata_snapshot_;

  base::WeakPtrFactory<DatabaseConnection> weak_factory_{this};
};

}  // namespace sqlite
}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_DATABASE_CONNECTION_H_
