// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_DATABASE_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_DATABASE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "content/browser/indexed_db/instance/backing_store.h"

namespace content::indexed_db::sqlite {

class DatabaseConnection;

// Thunks all operations to `DatabaseConnection`.
class BackingStoreDatabaseImpl : public BackingStore::Database {
 public:
  using PassKey = base::PassKey<BackingStoreDatabaseImpl>;

  explicit BackingStoreDatabaseImpl(base::WeakPtr<DatabaseConnection> db);
  BackingStoreDatabaseImpl(const BackingStoreDatabaseImpl&) = delete;
  BackingStoreDatabaseImpl& operator=(const BackingStoreDatabaseImpl&) = delete;
  ~BackingStoreDatabaseImpl() override;

  // BackingStore::Database:
  const blink::IndexedDBDatabaseMetadata& GetMetadata() const override;
  const IndexedDBDataLossInfo& GetDataLossInfo() const override;
  std::string GetObjectStoreLockIdKey(int64_t object_store_id) const override;
  std::unique_ptr<BackingStore::Transaction> CreateTransaction(
      blink::mojom::IDBTransactionDurability durability,
      blink::mojom::IDBTransactionMode mode) override;
  Status DeleteDatabase(std::vector<PartitionedLock> locks,
                        base::OnceClosure on_complete) override;

 private:
  friend class BackingStoreSqliteTest;

  // Note that this will be null after calling `DeleteDatabase()`, so `this`
  // should generally not be used after calling `DeleteDatabase()`.
  base::WeakPtr<DatabaseConnection> db_;
};

}  // namespace content::indexed_db::sqlite

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_DATABASE_IMPL_H_
