// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_DATABASE_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_DATABASE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

namespace sql {
class Database;
}  // namespace sql

namespace content::indexed_db::sqlite {

class DatabaseConnection;

// Owns metadata maintenance. There is at most one instance per
// `DatabaseConnection`.
class BackingStoreDatabaseImpl : public BackingStore::Database {
 public:
  // Enables calling methods that upgrade the IndexedDB database.
  // This is modeled after `base::PassKey` which cannot be used directly since
  // we want to store some data inside the key too.
  class UpgradePassKey {
   public:
    UpgradePassKey(const UpgradePassKey&) = delete;
    UpgradePassKey& operator=(const UpgradePassKey&) = delete;
    UpgradePassKey(UpgradePassKey&&) = default;
    UpgradePassKey& operator=(UpgradePassKey&&) = default;

   private:
    friend class BackingStoreDatabaseImpl;
    explicit UpgradePassKey(blink::IndexedDBDatabaseMetadata metadata_snapshot)
        : metadata_snapshot_(std::move(metadata_snapshot)) {}

    // The metadata before the upgrade.
    blink::IndexedDBDatabaseMetadata metadata_snapshot_;
  };

  BackingStoreDatabaseImpl(const std::u16string& name,
                           base::WeakPtr<DatabaseConnection> open_db);
  BackingStoreDatabaseImpl(const BackingStoreDatabaseImpl&) = delete;
  BackingStoreDatabaseImpl& operator=(const BackingStoreDatabaseImpl&) = delete;
  ~BackingStoreDatabaseImpl() override;

  sql::Database* db() const { return connection_->db(); }

  // Rolls back all changes made through this pass key.
  void RollbackUpgrade(UpgradePassKey&);

  // Methods to upgrade the IndexedDB database. Require a valid pass key.
  Status SetDatabaseVersion(UpgradePassKey&, int64_t version);

  // BackingStore::Database:
  const blink::IndexedDBDatabaseMetadata& GetMetadata() override;
  PartitionedLockId GetLockId(int64_t object_store_id) const override;
  std::unique_ptr<BackingStore::Transaction> CreateTransaction(
      blink::mojom::IDBTransactionDurability durability,
      blink::mojom::IDBTransactionMode mode) override;
  Status DeleteDatabase(std::vector<PartitionedLock> locks,
                        base::OnceClosure on_complete) override;

 private:
  base::WeakPtr<BackingStoreDatabaseImpl> GetWeakPtr();

  base::WeakPtr<DatabaseConnection> connection_;
  blink::IndexedDBDatabaseMetadata metadata_;
  base::WeakPtrFactory<BackingStoreDatabaseImpl> weak_factory_{this};
};

}  // namespace content::indexed_db::sqlite

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_DATABASE_IMPL_H_
