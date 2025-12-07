// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/backing_store_transaction_impl.h"

#include "base/check.h"
#include "base/types/expected_macros.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "content/browser/indexed_db/status.h"

namespace content::indexed_db::sqlite {

BackingStoreTransactionImpl::BackingStoreTransactionImpl(
    base::WeakPtr<DatabaseConnection> db,
    blink::mojom::IDBTransactionDurability durability,
    blink::mojom::IDBTransactionMode mode)
    : db_(std::move(db)), durability_(durability), mode_(mode) {}

BackingStoreTransactionImpl::~BackingStoreTransactionImpl() {
  // If locks are non-empty then the transaction was begun.
  if (!locks_.empty()) {
    db_->EndTransaction(PassKey(), *this);
  }
}

Status BackingStoreTransactionImpl::Begin(std::vector<PartitionedLock> locks) {
  locks_ = std::move(locks);
  return db_->BeginTransaction(PassKey(), *this);
}

Status BackingStoreTransactionImpl::CommitPhaseOne(
    BlobWriteCallback callback,
    SerializeFsaCallback serialize_fsa) {
  return db_->CommitTransactionPhaseOne(PassKey(), *this, std::move(callback),
                                        std::move(serialize_fsa));
}

Status BackingStoreTransactionImpl::CommitPhaseTwo() {
  return db_->CommitTransactionPhaseTwo(PassKey(), *this);
}

void BackingStoreTransactionImpl::Rollback() {
  return db_->RollBackTransaction(PassKey(), *this);
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
  return db_->DeleteObjectStore(PassKey(), object_store_id);
}

Status BackingStoreTransactionImpl::RenameObjectStore(
    int64_t object_store_id,
    const std::u16string& new_name) {
  return db_->RenameObjectStore(PassKey(), object_store_id, new_name);
}

Status BackingStoreTransactionImpl::ClearObjectStore(int64_t object_store_id) {
  return db_->ClearObjectStore(PassKey(), object_store_id);
}

Status BackingStoreTransactionImpl::CreateIndex(
    int64_t object_store_id,
    blink::IndexedDBIndexMetadata index) {
  return db_->CreateIndex(PassKey(), object_store_id, std::move(index));
}

Status BackingStoreTransactionImpl::DeleteIndex(int64_t object_store_id,
                                                int64_t index_id) {
  return db_->DeleteIndex(PassKey(), object_store_id, index_id);
}

Status BackingStoreTransactionImpl::RenameIndex(
    int64_t object_store_id,
    int64_t index_id,
    const std::u16string& new_name) {
  return db_->RenameIndex(PassKey(), object_store_id, index_id, new_name);
}

StatusOr<IndexedDBValue> BackingStoreTransactionImpl::GetRecord(
    int64_t object_store_id,
    const blink::IndexedDBKey& key) {
  return db_->GetValue(PassKey(), object_store_id, key);
}

StatusOr<BackingStore::RecordIdentifier> BackingStoreTransactionImpl::PutRecord(
    int64_t object_store_id,
    const blink::IndexedDBKey& key,
    IndexedDBValue value) {
  return db_->PutRecord(PassKey(), object_store_id, key, std::move(value));
}

Status BackingStoreTransactionImpl::DeleteRange(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& range) {
  return db_->DeleteRange(PassKey(), object_store_id, range);
}

StatusOr<int64_t> BackingStoreTransactionImpl::GetKeyGeneratorCurrentNumber(
    int64_t object_store_id) {
  return db_->GetKeyGeneratorCurrentNumber(PassKey(), object_store_id);
}

Status BackingStoreTransactionImpl::MaybeUpdateKeyGeneratorCurrentNumber(
    int64_t object_store_id,
    int64_t new_number,
    bool was_generated) {
  // The `was_generated` hint is not useful for SQLite as the check and update
  // can be made in a single SQL statement.
  return db_->MaybeUpdateKeyGeneratorCurrentNumber(PassKey(), object_store_id,
                                                   new_number);
}

StatusOr<std::optional<BackingStore::RecordIdentifier>>
BackingStoreTransactionImpl::KeyExistsInObjectStore(
    int64_t object_store_id,
    const blink::IndexedDBKey& key) {
  return db_->GetRecordIdentifierIfExists(PassKey(), object_store_id, key);
}

Status BackingStoreTransactionImpl::PutIndexDataForRecord(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key,
    const BackingStore::RecordIdentifier& record) {
  return db_->PutIndexDataForRecord(PassKey(), object_store_id, index_id, key,
                                    record);
}

StatusOr<blink::IndexedDBKey>
BackingStoreTransactionImpl::GetFirstPrimaryKeyForIndexKey(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key) {
  return db_->GetFirstPrimaryKeyForIndexKey(PassKey(), object_store_id,
                                            index_id, key);
}

StatusOr<uint32_t> BackingStoreTransactionImpl::GetObjectStoreKeyCount(
    int64_t object_store_id,
    blink::IndexedDBKeyRange key_range) {
  return db_->GetObjectStoreKeyCount(PassKey(), object_store_id,
                                     std::move(key_range));
}

StatusOr<uint32_t> BackingStoreTransactionImpl::GetIndexKeyCount(
    int64_t object_store_id,
    int64_t index_id,
    blink::IndexedDBKeyRange key_range) {
  return db_->GetIndexKeyCount(PassKey(), object_store_id, index_id,
                               std::move(key_range));
}

StatusOr<std::unique_ptr<BackingStore::Cursor>>
BackingStoreTransactionImpl::OpenObjectStoreKeyCursor(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction) {
  return db_->OpenObjectStoreCursor(PassKey(), object_store_id, key_range,
                                    direction, /*key_only=*/true);
}

StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>>
BackingStoreTransactionImpl::OpenObjectStoreCursor(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction) {
  return db_->OpenObjectStoreCursor(PassKey(), object_store_id, key_range,
                                    direction, /*key_only=*/false);
}

StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>>
BackingStoreTransactionImpl::OpenIndexKeyCursor(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction) {
  return db_->OpenIndexCursor(PassKey(), object_store_id, index_id, key_range,
                              direction, /*key_only=*/true);
}

StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>>
BackingStoreTransactionImpl::OpenIndexCursor(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction) {
  return db_->OpenIndexCursor(PassKey(), object_store_id, index_id, key_range,
                              direction, /*key_only=*/false);
}

blink::mojom::IDBValuePtr BackingStoreTransactionImpl::BuildMojoValue(
    IndexedDBValue value,
    DeserializeFsaCallback deserialize_handle) {
  auto mojo_value = blink::mojom::IDBValue::New();
  if (!value.empty()) {
    mojo_value->bits = std::move(value.bits);
  }
  if (!value.external_objects.empty()) {
    mojo_value->external_objects = db_->CreateAllExternalObjects(
        PassKey(), value.external_objects, deserialize_handle);
  }
  return mojo_value;
}

}  // namespace content::indexed_db::sqlite
