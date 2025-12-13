// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/fake_transaction.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace content::indexed_db {

FakeTransaction::FakeTransaction(
    Status phase_two_result,
    std::unique_ptr<BackingStore::Transaction> wrapped)
    : result_(phase_two_result), wrapped_transaction_(std::move(wrapped)) {}

FakeTransaction::~FakeTransaction() = default;

Status FakeTransaction::CommitPhaseOne(BlobWriteCallback callback,
                                       SerializeFsaCallback /*unused*/) {
  return std::move(callback).Run(BlobWriteResult::kRunPhaseTwoAndReturnResult);
}

Status FakeTransaction::CommitPhaseTwo() {
  return result_;
}

void FakeTransaction::Rollback() {}

Status FakeTransaction::Begin(std::vector<PartitionedLock> locks) {
  return wrapped_transaction_->Begin(std::move(locks));
}

Status FakeTransaction::SetDatabaseVersion(int64_t version) {
  return wrapped_transaction_->SetDatabaseVersion(version);
}

Status FakeTransaction::CreateObjectStore(int64_t object_store_id,
                                          const std::u16string& name,
                                          blink::IndexedDBKeyPath key_path,
                                          bool auto_increment) {
  return wrapped_transaction_->CreateObjectStore(
      object_store_id, name, std::move(key_path), auto_increment);
}

Status FakeTransaction::DeleteObjectStore(int64_t object_store_id) {
  return wrapped_transaction_->DeleteObjectStore(object_store_id);
}

Status FakeTransaction::RenameObjectStore(int64_t object_store_id,
                                          const std::u16string& new_name) {
  return wrapped_transaction_->RenameObjectStore(object_store_id, new_name);
}

Status FakeTransaction::ClearObjectStore(int64_t object_store_id) {
  return wrapped_transaction_->ClearObjectStore(object_store_id);
}

Status FakeTransaction::CreateIndex(int64_t object_store_id,
                                    blink::IndexedDBIndexMetadata index) {
  return wrapped_transaction_->CreateIndex(object_store_id, std::move(index));
}

Status FakeTransaction::DeleteIndex(int64_t object_store_id, int64_t index_id) {
  return wrapped_transaction_->DeleteIndex(object_store_id, index_id);
}

Status FakeTransaction::RenameIndex(int64_t object_store_id,
                                    int64_t index_id,
                                    const std::u16string& new_name) {
  return wrapped_transaction_->RenameIndex(object_store_id, index_id, new_name);
}

StatusOr<IndexedDBValue> FakeTransaction::GetRecord(
    int64_t object_store_id,
    const blink::IndexedDBKey& key) {
  return wrapped_transaction_->GetRecord(object_store_id, key);
}

StatusOr<BackingStore::RecordIdentifier> FakeTransaction::PutRecord(
    int64_t object_store_id,
    const blink::IndexedDBKey& key,
    IndexedDBValue value) {
  return wrapped_transaction_->PutRecord(object_store_id, key,
                                         std::move(value));
}

Status FakeTransaction::DeleteRange(int64_t object_store_id,
                                    const blink::IndexedDBKeyRange& key_range) {
  return wrapped_transaction_->DeleteRange(object_store_id, key_range);
}

StatusOr<int64_t> FakeTransaction::GetKeyGeneratorCurrentNumber(
    int64_t object_store_id) {
  return wrapped_transaction_->GetKeyGeneratorCurrentNumber(object_store_id);
}

Status FakeTransaction::MaybeUpdateKeyGeneratorCurrentNumber(
    int64_t object_store_id,
    int64_t new_number,
    bool was_generated) {
  return wrapped_transaction_->MaybeUpdateKeyGeneratorCurrentNumber(
      object_store_id, new_number, was_generated);
}

StatusOr<std::optional<BackingStore::RecordIdentifier>>
FakeTransaction::KeyExistsInObjectStore(int64_t object_store_id,
                                        const blink::IndexedDBKey& key) {
  return wrapped_transaction_->KeyExistsInObjectStore(object_store_id, key);
}

Status FakeTransaction::PutIndexDataForRecord(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key,
    const BackingStore::RecordIdentifier& record) {
  return wrapped_transaction_->PutIndexDataForRecord(object_store_id, index_id,
                                                     key, record);
}

StatusOr<blink::IndexedDBKey> FakeTransaction::GetFirstPrimaryKeyForIndexKey(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key) {
  return wrapped_transaction_->GetFirstPrimaryKeyForIndexKey(object_store_id,
                                                             index_id, key);
}

StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>>
FakeTransaction::OpenObjectStoreKeyCursor(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction) {
  return wrapped_transaction_->OpenObjectStoreKeyCursor(object_store_id,
                                                        key_range, direction);
}

StatusOr<uint32_t> FakeTransaction::GetObjectStoreKeyCount(
    int64_t object_store_id,
    blink::IndexedDBKeyRange key_range) {
  return wrapped_transaction_->GetObjectStoreKeyCount(object_store_id,
                                                      std::move(key_range));
}

StatusOr<uint32_t> FakeTransaction::GetIndexKeyCount(
    int64_t object_store_id,
    int64_t index_id,
    blink::IndexedDBKeyRange key_range) {
  return wrapped_transaction_->GetIndexKeyCount(object_store_id, index_id,
                                                std::move(key_range));
}

StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>>
FakeTransaction::OpenObjectStoreCursor(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction) {
  return wrapped_transaction_->OpenObjectStoreCursor(object_store_id, key_range,
                                                     direction);
}

StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>>
FakeTransaction::OpenIndexKeyCursor(
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction) {
  return wrapped_transaction_->OpenIndexKeyCursor(object_store_id, index_id,
                                                  key_range, direction);
}

StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>>
FakeTransaction::OpenIndexCursor(int64_t object_store_id,
                                 int64_t index_id,
                                 const blink::IndexedDBKeyRange& key_range,
                                 blink::mojom::IDBCursorDirection direction) {
  return wrapped_transaction_->OpenIndexCursor(object_store_id, index_id,
                                               key_range, direction);
}

blink::mojom::IDBValuePtr FakeTransaction::BuildMojoValue(
    IndexedDBValue value,
    DeserializeFsaCallback deserialize_fsa_handle) {
  return wrapped_transaction_->BuildMojoValue(
      std::move(value), std::move(deserialize_fsa_handle));
}

}  // namespace content::indexed_db
