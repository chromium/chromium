// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"

#include "base/notimplemented.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_transaction_impl.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "content/browser/indexed_db/status.h"

namespace content::indexed_db::sqlite {

BackingStoreDatabaseImpl::BackingStoreDatabaseImpl(
    base::WeakPtr<DatabaseConnection> db)
    : db_(std::move(db)) {}

BackingStoreDatabaseImpl::~BackingStoreDatabaseImpl() = default;

const blink::IndexedDBDatabaseMetadata&
BackingStoreDatabaseImpl::GetMetadata() {
  return db_->metadata();
}

PartitionedLockId BackingStoreDatabaseImpl::GetLockId(
    int64_t object_store_id) const {
  NOTIMPLEMENTED();
  return PartitionedLockId();
}

std::unique_ptr<BackingStore::Transaction>
BackingStoreDatabaseImpl::CreateTransaction(
    blink::mojom::IDBTransactionDurability durability,
    blink::mojom::IDBTransactionMode mode) {
  return db_->CreateTransaction(PassKey(), durability, mode);
}

Status BackingStoreDatabaseImpl::DeleteDatabase(
    std::vector<PartitionedLock> locks,
    base::OnceClosure on_complete) {
  NOTIMPLEMENTED();
  return Status::InvalidArgument("Not implemented");
}

}  // namespace content::indexed_db::sqlite
