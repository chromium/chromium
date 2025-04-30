// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/fake_transaction.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom-shared.h"
#include "content/browser/indexed_db/indexed_db_external_object_storage.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/status.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content::indexed_db {

FakeTransaction::FakeTransaction(
    Status result,
    std::unique_ptr<BackingStore::Transaction> wrapped)
    : result_(result), wrapped_transaction_(std::move(wrapped)) {}

FakeTransaction::~FakeTransaction() = default;

Status FakeTransaction::CommitPhaseOne(BlobWriteCallback callback) {
  return std::move(callback).Run(
      BlobWriteResult::kRunPhaseTwoAndReturnResult,
      storage::mojom::WriteBlobToFileResult::kSuccess);
}

Status FakeTransaction::CommitPhaseTwo() {
  return result_;
}

void FakeTransaction::Rollback() {}

void FakeTransaction::Begin(std::vector<PartitionedLock> locks) {
  wrapped_transaction_->Begin(std::move(locks));
}

void FakeTransaction::Reset() {
  wrapped_transaction_->Reset();
}

Status FakeTransaction::SetDatabaseVersion(int64_t version) {
  return wrapped_transaction_->SetDatabaseVersion(version);
}

Status FakeTransaction::CreateObjectStore(
    int64_t object_store_id,
    std::u16string name,
    blink::IndexedDBKeyPath key_path,
    bool auto_increment,
    blink::IndexedDBObjectStoreMetadata* metadata) {
  return wrapped_transaction_->CreateObjectStore(
      object_store_id, std::move(name), std::move(key_path), auto_increment,
      metadata);
}

Status FakeTransaction::DeleteObjectStore(
    const blink::IndexedDBObjectStoreMetadata& object_store) {
  return wrapped_transaction_->DeleteObjectStore(object_store);
}

Status FakeTransaction::RenameObjectStore(
    std::u16string new_name,
    std::u16string* old_name,
    blink::IndexedDBObjectStoreMetadata* metadata) {
  return wrapped_transaction_->RenameObjectStore(std::move(new_name), old_name,
                                                 metadata);
}

Status FakeTransaction::CreateIndex(int64_t object_store_id,
                                    int64_t index_id,
                                    std::u16string name,
                                    blink::IndexedDBKeyPath key_path,
                                    bool is_unique,
                                    bool is_multi_entry,
                                    blink::IndexedDBIndexMetadata* metadata) {
  return wrapped_transaction_->CreateIndex(object_store_id, index_id,
                                           std::move(name), std::move(key_path),
                                           is_unique, is_multi_entry, metadata);
}

Status FakeTransaction::DeleteIndex(
    int64_t object_store_id,
    const blink::IndexedDBIndexMetadata& metadata) {
  return wrapped_transaction_->DeleteIndex(object_store_id, metadata);
}

Status FakeTransaction::RenameIndex(int64_t object_store_id,
                                    std::u16string new_name,
                                    std::u16string* old_name,
                                    blink::IndexedDBIndexMetadata* metadata) {
  return wrapped_transaction_->RenameIndex(object_store_id, std::move(new_name),
                                           old_name, metadata);
}

Status FakeTransaction::GetRecord(int64_t object_store_id,
                                  const blink::IndexedDBKey& key,
                                  IndexedDBValue* record) {
  return wrapped_transaction_->GetRecord(object_store_id, key, record);
}

Status FakeTransaction::PutRecord(int64_t object_store_id,
                                  const blink::IndexedDBKey& key,
                                  IndexedDBValue* value,
                                  BackingStore::RecordIdentifier* record) {
  return wrapped_transaction_->PutRecord(object_store_id, key, value, record);
}

Status FakeTransaction::ClearObjectStore(int64_t object_store_id) {
  return wrapped_transaction_->ClearObjectStore(object_store_id);
}

Status FakeTransaction::DeleteRecord(
    int64_t object_store_id,
    const BackingStore::RecordIdentifier& record) {
  return wrapped_transaction_->DeleteRecord(object_store_id, record);
}

Status FakeTransaction::DeleteRange(int64_t object_store_id,
                                    const blink::IndexedDBKeyRange& key_range) {
  return wrapped_transaction_->DeleteRange(object_store_id, key_range);
}

Status FakeTransaction::GetKeyGeneratorCurrentNumber(int64_t object_store_id,
                                                     int64_t* current_number) {
  return wrapped_transaction_->GetKeyGeneratorCurrentNumber(object_store_id,
                                                            current_number);
}

Status FakeTransaction::MaybeUpdateKeyGeneratorCurrentNumber(
    int64_t object_store_id,
    int64_t new_state,
    bool check_current) {
  return wrapped_transaction_->MaybeUpdateKeyGeneratorCurrentNumber(
      object_store_id, new_state, check_current);
}

Status FakeTransaction::KeyExistsInObjectStore(
    int64_t object_store_id,
    const blink::IndexedDBKey& key,
    BackingStore::RecordIdentifier* found_record_identifier,
    bool* found) {
  return wrapped_transaction_->KeyExistsInObjectStore(
      object_store_id, key, found_record_identifier, found);
}

Status FakeTransaction::ClearIndex(int64_t object_store_id, int64_t index_id) {
  return wrapped_transaction_->ClearIndex(object_store_id, index_id);
}

Status FakeTransaction::PutIndexDataForRecord(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key,
    const BackingStore::RecordIdentifier& record) {
  return wrapped_transaction_->PutIndexDataForRecord(object_store_id, index_id,
                                                     key, record);
}

Status FakeTransaction::GetPrimaryKeyViaIndex(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key,
    std::unique_ptr<blink::IndexedDBKey>* primary_key) {
  return wrapped_transaction_->GetPrimaryKeyViaIndex(object_store_id, index_id,
                                                     key, primary_key);
}

Status FakeTransaction::KeyExistsInIndex(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key,
    std::unique_ptr<blink::IndexedDBKey>* found_primary_key,
    bool* exists) {
  return wrapped_transaction_->KeyExistsInIndex(object_store_id, index_id, key,
                                                found_primary_key, exists);
}

std::unique_ptr<indexed_db::BackingStore::Cursor>
FakeTransaction::OpenObjectStoreKeyCursor(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction,
    Status* status) {
  return wrapped_transaction_->OpenObjectStoreKeyCursor(
      object_store_id, key_range, direction, status);
}

std::unique_ptr<indexed_db::BackingStore::Cursor>
FakeTransaction::OpenObjectStoreCursor(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction,
    Status* status) {
  return wrapped_transaction_->OpenObjectStoreCursor(object_store_id, key_range,
                                                     direction, status);
}

std::unique_ptr<indexed_db::BackingStore::Cursor>
FakeTransaction::OpenIndexKeyCursor(int64_t object_store_id,
                                    int64_t index_id,
                                    const blink::IndexedDBKeyRange& key_range,
                                    blink::mojom::IDBCursorDirection direction,
                                    Status* status) {
  return wrapped_transaction_->OpenIndexKeyCursor(object_store_id, index_id,
                                                  key_range, direction, status);
}

std::unique_ptr<indexed_db::BackingStore::Cursor>
FakeTransaction::OpenIndexCursor(int64_t object_store_id,
                                 int64_t index_id,
                                 const blink::IndexedDBKeyRange& key_range,
                                 blink::mojom::IDBCursorDirection direction,
                                 Status* status) {
  return wrapped_transaction_->OpenIndexCursor(object_store_id, index_id,
                                               key_range, direction, status);
}

}  // namespace content::indexed_db
