// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"

#include "base/notreached.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_transaction_impl.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "content/browser/indexed_db/status.h"

namespace content::indexed_db::sqlite {

BackingStoreDatabaseImpl::BackingStoreDatabaseImpl(
    base::WeakPtr<DatabaseConnection> db)
    : db_(std::move(db)) {}

BackingStoreDatabaseImpl::~BackingStoreDatabaseImpl() {
  DatabaseConnection::Release(std::move(db_));
}

const blink::IndexedDBDatabaseMetadata& BackingStoreDatabaseImpl::GetMetadata()
    const {
  return db_->metadata();
}

const IndexedDBDataLossInfo& BackingStoreDatabaseImpl::GetDataLossInfo() const {
  return db_->data_loss_info();
}

std::string BackingStoreDatabaseImpl::GetObjectStoreLockIdKey(
    int64_t object_store_id) const {
  NOTREACHED();
}

std::unique_ptr<BackingStore::Transaction>
BackingStoreDatabaseImpl::CreateTransaction(
    blink::mojom::IDBTransactionDurability durability,
    blink::mojom::IDBTransactionMode mode) {
  return db_->CreateTransactionWrapper(PassKey(), durability, mode);
}

Status BackingStoreDatabaseImpl::DeleteDatabase(
    std::vector<PartitionedLock> locks,
    base::OnceClosure on_complete) {
  db_->DeleteIdbDatabase(PassKey());
  CHECK(!db_);
  std::move(on_complete).Run();
  return Status::OK();
}

}  // namespace content::indexed_db::sqlite
