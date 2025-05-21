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

Status FakeTransaction::GetRecord(int64_t object_store_id,
                                  const blink::IndexedDBKey& key,
                                  IndexedDBValue* record) {
  return wrapped_transaction_->GetRecord(object_store_id, key, record);
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
    int64_t new_state,
    bool check_current) {
  return wrapped_transaction_->MaybeUpdateKeyGeneratorCurrentNumber(
      object_store_id, new_state, check_current);
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

StatusOr<std::unique_ptr<indexed_db::BackingStore::Cursor>>
FakeTransaction::OpenObjectStoreKeyCursor(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction) {
  return wrapped_transaction_->OpenObjectStoreKeyCursor(object_store_id,
                                                        key_range, direction);
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

}  // namespace content::indexed_db
