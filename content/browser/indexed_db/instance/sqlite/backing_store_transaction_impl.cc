// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/backing_store_transaction_impl.h"

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"
#include "content/browser/indexed_db/status.h"
#include "sql/transaction.h"

namespace content::indexed_db::sqlite {

BackingStoreTransactionImpl::BackingStoreTransactionImpl(
    base::WeakPtr<BackingStoreDatabaseImpl> db)
    : db_(db), transaction_(db_->db()) {}

BackingStoreTransactionImpl::~BackingStoreTransactionImpl() = default;

void BackingStoreTransactionImpl::Begin(std::vector<PartitionedLock> locks) {
  CHECK(transaction_.Begin());
}

Status BackingStoreTransactionImpl::CommitPhaseOne(BlobWriteCallback callback) {
  return std::move(callback).Run(
      BlobWriteResult::kRunPhaseTwoAndReturnResult,
      storage::mojom::WriteBlobToFileResult::kSuccess);
}

Status BackingStoreTransactionImpl::CommitPhaseTwo() {
  transaction_.Commit();
  return Status::OK();
}

void BackingStoreTransactionImpl::Rollback() {
  transaction_.Rollback();
}

Status BackingStoreTransactionImpl::SetDatabaseVersion(int64_t version) {
  NOTREACHED() << "Implemented by BackingStoreVersionChangeTransaction";
}

Status BackingStoreTransactionImpl::CreateObjectStore(
    int64_t object_store_id,
    const std::u16string& name,
    blink::IndexedDBKeyPath key_path,
    bool auto_increment) {
  NOTREACHED() << "Implemented by BackingStoreVersionChangeTransaction";
}

Status BackingStoreTransactionImpl::DeleteObjectStore(int64_t object_store_id) {
  NOTREACHED() << "Implemented by BackingStoreVersionChangeTransaction";
}

Status BackingStoreTransactionImpl::RenameObjectStore(
    int64_t object_store_id,
    const std::u16string& new_name) {
  NOTREACHED() << "Implemented by BackingStoreVersionChangeTransaction";
}

Status BackingStoreTransactionImpl::ClearObjectStore(int64_t object_store_id) {
  NOTREACHED() << "Implemented by BackingStoreVersionChangeTransaction";
}

Status BackingStoreTransactionImpl::CreateIndex(
    int64_t object_store_id,
    int64_t index_id,
    const std::u16string& name,
    blink::IndexedDBKeyPath key_path,
    bool is_unique,
    bool is_multi_entry) {
  NOTREACHED() << "Implemented by BackingStoreVersionChangeTransaction";
}

Status BackingStoreTransactionImpl::DeleteIndex(int64_t object_store_id,
                                                int64_t index_id) {
  NOTREACHED() << "Implemented by BackingStoreVersionChangeTransaction";
}

Status BackingStoreTransactionImpl::RenameIndex(
    int64_t object_store_id,
    int64_t index_id,
    const std::u16string& new_name) {
  NOTREACHED() << "Implemented by BackingStoreVersionChangeTransaction";
}

Status BackingStoreTransactionImpl::GetRecord(int64_t object_store_id,
                                              const blink::IndexedDBKey& key,
                                              IndexedDBValue* record) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::PutRecord(
    int64_t object_store_id,
    const blink::IndexedDBKey& key,
    IndexedDBValue* value,
    BackingStore::RecordIdentifier* record) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::DeleteRange(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange&) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::GetKeyGeneratorCurrentNumber(
    int64_t object_store_id,
    int64_t* current_number) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::MaybeUpdateKeyGeneratorCurrentNumber(
    int64_t object_store_id,
    int64_t new_state,
    bool check_current) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::KeyExistsInObjectStore(
    int64_t object_store_id,
    const blink::IndexedDBKey& key,
    BackingStore::RecordIdentifier* found_record_identifier,
    bool* found) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::PutIndexDataForRecord(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key,
    const BackingStore::RecordIdentifier& record) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::GetPrimaryKeyViaIndex(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key,
    std::unique_ptr<blink::IndexedDBKey>* primary_key) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::KeyExistsInIndex(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key,
    std::unique_ptr<blink::IndexedDBKey>* found_primary_key,
    bool* exists) {
  NOTIMPLEMENTED();
  return Status::OK();
}

std::unique_ptr<BackingStore::Cursor>
BackingStoreTransactionImpl::OpenObjectStoreKeyCursor(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection,
    Status*) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<BackingStore::Cursor>
BackingStoreTransactionImpl::OpenObjectStoreCursor(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection,
    Status*) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<BackingStore::Cursor>
BackingStoreTransactionImpl::OpenIndexKeyCursor(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection,
    Status*) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<BackingStore::Cursor>
BackingStoreTransactionImpl::OpenIndexCursor(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection,
    Status*) {
  NOTIMPLEMENTED();
  return nullptr;
}

BackingStoreVersionChangeTransaction::BackingStoreVersionChangeTransaction(
    base::WeakPtr<BackingStoreDatabaseImpl> db,
    BackingStoreDatabaseImpl::UpgradePassKey pass_key)
    : BackingStoreTransactionImpl(db), pass_key_(std::move(pass_key)) {}

BackingStoreVersionChangeTransaction::~BackingStoreVersionChangeTransaction() =
    default;

void BackingStoreVersionChangeTransaction::Rollback() {
  BackingStoreTransactionImpl::Rollback();
  db_->RollbackUpgrade(pass_key_);
}

Status BackingStoreVersionChangeTransaction::SetDatabaseVersion(
    int64_t version) {
  return db_->SetDatabaseVersion(pass_key_, version);
}

}  // namespace content::indexed_db::sqlite
