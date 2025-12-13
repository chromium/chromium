// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_TRANSACTION_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_TRANSACTION_IMPL_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace content::indexed_db::sqlite {

class DatabaseConnection;

class BackingStoreTransactionImpl : public BackingStore::Transaction {
 public:
  using PassKey = base::PassKey<BackingStoreTransactionImpl>;

  BackingStoreTransactionImpl(base::WeakPtr<DatabaseConnection> db,
                              blink::mojom::IDBTransactionDurability durability,
                              blink::mojom::IDBTransactionMode mode);
  BackingStoreTransactionImpl(const BackingStoreTransactionImpl&) = delete;
  BackingStoreTransactionImpl& operator=(const BackingStoreTransactionImpl&) =
      delete;
  ~BackingStoreTransactionImpl() override;

  blink::mojom::IDBTransactionDurability durability() const {
    return durability_;
  }
  blink::mojom::IDBTransactionMode mode() const { return mode_; }

  // BackingStore::Transaction:
  Status Begin(std::vector<PartitionedLock> locks) override;
  Status CommitPhaseOne(BlobWriteCallback callback,
                        SerializeFsaCallback serialize_fsa) override;
  Status CommitPhaseTwo() override;
  void Rollback() override;
  Status SetDatabaseVersion(int64_t version) override;
  Status CreateObjectStore(int64_t object_store_id,
                           const std::u16string& name,
                           blink::IndexedDBKeyPath key_path,
                           bool auto_increment) override;
  Status DeleteObjectStore(int64_t object_store_id) override;
  Status RenameObjectStore(int64_t object_store_id,
                           const std::u16string& new_name) override;
  Status ClearObjectStore(int64_t object_store_id) override;
  Status CreateIndex(int64_t object_store_id,
                     blink::IndexedDBIndexMetadata index) override;
  Status DeleteIndex(int64_t object_store_id, int64_t index_id) override;
  Status RenameIndex(int64_t object_store_id,
                     int64_t index_id,
                     const std::u16string& new_name) override;
  StatusOr<IndexedDBValue> GetRecord(int64_t object_store_id,
                                     const blink::IndexedDBKey& key) override;
  StatusOr<BackingStore::RecordIdentifier> PutRecord(
      int64_t object_store_id,
      const blink::IndexedDBKey& key,
      IndexedDBValue value) override;
  Status DeleteRange(int64_t object_store_id,
                     const blink::IndexedDBKeyRange&) override;
  StatusOr<int64_t> GetKeyGeneratorCurrentNumber(
      int64_t object_store_id) override;
  Status MaybeUpdateKeyGeneratorCurrentNumber(int64_t object_store_id,
                                              int64_t new_number,
                                              bool was_generated) override;
  StatusOr<std::optional<BackingStore::RecordIdentifier>>
  KeyExistsInObjectStore(int64_t object_store_id,
                         const blink::IndexedDBKey& key) override;
  Status PutIndexDataForRecord(
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      const BackingStore::RecordIdentifier& record) override;
  StatusOr<blink::IndexedDBKey> GetFirstPrimaryKeyForIndexKey(
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key) override;
  StatusOr<uint32_t> GetObjectStoreKeyCount(
      int64_t object_store_id,
      blink::IndexedDBKeyRange key_range) override;
  StatusOr<uint32_t> GetIndexKeyCount(
      int64_t object_store_id,
      int64_t index_id,
      blink::IndexedDBKeyRange key_range) override;
  StatusOr<std::unique_ptr<BackingStore::Cursor>> OpenObjectStoreKeyCursor(
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection) override;
  StatusOr<std::unique_ptr<BackingStore::Cursor>> OpenObjectStoreCursor(
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection) override;
  StatusOr<std::unique_ptr<BackingStore::Cursor>> OpenIndexKeyCursor(
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection) override;
  StatusOr<std::unique_ptr<BackingStore::Cursor>> OpenIndexCursor(
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection) override;
  blink::mojom::IDBValuePtr BuildMojoValue(
      IndexedDBValue value,
      DeserializeFsaCallback deserialize_handle) override;

 protected:
  base::WeakPtr<DatabaseConnection> db_;

 private:
  blink::mojom::IDBTransactionDurability durability_;
  blink::mojom::IDBTransactionMode mode_;
  std::vector<PartitionedLock> locks_;
};

}  // namespace content::indexed_db::sqlite

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_TRANSACTION_IMPL_H_
