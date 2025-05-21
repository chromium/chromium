// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/backing_store_transaction_impl.h"

#include "base/check.h"
#include "base/notimplemented.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "content/browser/indexed_db/status.h"
#include "sql/transaction.h"

namespace content::indexed_db::sqlite {

BackingStoreTransactionImpl::BackingStoreTransactionImpl(
    base::WeakPtr<DatabaseConnection> db,
    std::unique_ptr<sql::Transaction> transaction,
    blink::mojom::IDBTransactionDurability durability,
    blink::mojom::IDBTransactionMode mode)
    : db_(std::move(db)),
      transaction_(std::move(transaction)),
      durability_(durability),
      mode_(mode) {}

BackingStoreTransactionImpl::~BackingStoreTransactionImpl() = default;

void BackingStoreTransactionImpl::Begin(std::vector<PartitionedLock> locks) {
  // TODO(crbug.com/40253999): How do we surface the error if this call fails?
  CHECK(transaction_->Begin());
  db_->OnTransactionBegin(PassKey(), *this);
}

Status BackingStoreTransactionImpl::CommitPhaseOne(BlobWriteCallback callback) {
  return std::move(callback).Run(
      BlobWriteResult::kRunPhaseTwoAndReturnResult,
      storage::mojom::WriteBlobToFileResult::kSuccess);
}

Status BackingStoreTransactionImpl::CommitPhaseTwo() {
  db_->OnBeforeTransactionCommit(PassKey(), *this);
  transaction_->Commit();
  db_->OnTransactionCommit(PassKey(), *this);
  return Status::OK();
}

void BackingStoreTransactionImpl::Rollback() {
  transaction_->Rollback();
  db_->OnTransactionRollback(PassKey(), *this);
}

Status BackingStoreTransactionImpl::SetDatabaseVersion(int64_t version) {
  return db_->SetDatabaseVersion(PassKey(), version);
}

Status BackingStoreTransactionImpl::CreateObjectStore(
    int64_t object_store_id,
    const std::u16string& name,
    blink::IndexedDBKeyPath key_path,
    bool auto_increment) {
  return db_->CreateObjectStore(PassKey(), object_store_id, name,
                                std::move(key_path), auto_increment);
}

Status BackingStoreTransactionImpl::DeleteObjectStore(int64_t object_store_id) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::RenameObjectStore(
    int64_t object_store_id,
    const std::u16string& new_name) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::ClearObjectStore(int64_t object_store_id) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::CreateIndex(
    int64_t object_store_id,
    blink::IndexedDBIndexMetadata index) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::DeleteIndex(int64_t object_store_id,
                                                int64_t index_id) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreTransactionImpl::RenameIndex(
    int64_t object_store_id,
    int64_t index_id,
    const std::u16string& new_name) {
  NOTIMPLEMENTED();
  return Status::OK();
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

base::expected<std::unique_ptr<BackingStore::Cursor>, Status>
BackingStoreTransactionImpl::OpenObjectStoreKeyCursor(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection) {
  NOTIMPLEMENTED();
  return nullptr;
}

base::expected<std::unique_ptr<indexed_db::BackingStore::Cursor>, Status>
BackingStoreTransactionImpl::OpenObjectStoreCursor(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection) {
  NOTIMPLEMENTED();
  return nullptr;
}

base::expected<std::unique_ptr<indexed_db::BackingStore::Cursor>, Status>
BackingStoreTransactionImpl::OpenIndexKeyCursor(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection) {
  NOTIMPLEMENTED();
  return nullptr;
}

base::expected<std::unique_ptr<indexed_db::BackingStore::Cursor>, Status>
BackingStoreTransactionImpl::OpenIndexCursor(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace content::indexed_db::sqlite
