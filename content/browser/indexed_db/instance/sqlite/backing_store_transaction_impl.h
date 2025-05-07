// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_TRANSACTION_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_TRANSACTION_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"
#include "sql/transaction.h"

namespace content::indexed_db::sqlite {

class BackingStoreTransactionImpl : public BackingStore::Transaction {
 public:
  explicit BackingStoreTransactionImpl(
      base::WeakPtr<BackingStoreDatabaseImpl> db);
  BackingStoreTransactionImpl(const BackingStoreTransactionImpl&) = delete;
  BackingStoreTransactionImpl& operator=(const BackingStoreTransactionImpl&) =
      delete;
  ~BackingStoreTransactionImpl() override;

  // BackingStore::Transaction:
  void Begin(std::vector<PartitionedLock> locks) override;
  Status CommitPhaseOne(BlobWriteCallback callback) override;
  Status CommitPhaseTwo() override;
  void Rollback() override;
  void Reset() override {}
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
                     int64_t index_id,
                     const std::u16string& name,
                     blink::IndexedDBKeyPath key_path,
                     bool is_unique,
                     bool is_multi_entry) override;
  Status DeleteIndex(int64_t object_store_id, int64_t index_id) override;
  Status RenameIndex(int64_t object_store_id,
                     int64_t index_id,
                     const std::u16string& new_name) override;
  Status GetRecord(int64_t object_store_id,
                   const blink::IndexedDBKey& key,
                   IndexedDBValue* record) override;
  Status PutRecord(int64_t object_store_id,
                   const blink::IndexedDBKey& key,
                   IndexedDBValue* value,
                   BackingStore::RecordIdentifier* record) override;
  Status DeleteRange(int64_t object_store_id,
                     const blink::IndexedDBKeyRange&) override;
  Status GetKeyGeneratorCurrentNumber(int64_t object_store_id,
                                      int64_t* current_number) override;
  Status MaybeUpdateKeyGeneratorCurrentNumber(int64_t object_store_id,
                                              int64_t new_state,
                                              bool check_current) override;
  Status KeyExistsInObjectStore(
      int64_t object_store_id,
      const blink::IndexedDBKey& key,
      BackingStore::RecordIdentifier* found_record_identifier,
      bool* found) override;
  Status PutIndexDataForRecord(
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      const BackingStore::RecordIdentifier& record) override;
  Status GetPrimaryKeyViaIndex(
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      std::unique_ptr<blink::IndexedDBKey>* primary_key) override;
  Status KeyExistsInIndex(
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      std::unique_ptr<blink::IndexedDBKey>* found_primary_key,
      bool* exists) override;
  std::unique_ptr<BackingStore::Cursor> OpenObjectStoreKeyCursor(
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      Status*) override;
  std::unique_ptr<BackingStore::Cursor> OpenObjectStoreCursor(
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      Status*) override;
  std::unique_ptr<BackingStore::Cursor> OpenIndexKeyCursor(
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      Status*) override;
  std::unique_ptr<BackingStore::Cursor> OpenIndexCursor(
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      Status*) override;

 protected:
  base::WeakPtr<BackingStoreDatabaseImpl> db_;
  sql::Transaction transaction_;
};

class BackingStoreVersionChangeTransaction
    : public BackingStoreTransactionImpl {
 public:
  BackingStoreVersionChangeTransaction(
      base::WeakPtr<BackingStoreDatabaseImpl> db,
      BackingStoreDatabaseImpl::UpgradePassKey pass_key);
  BackingStoreVersionChangeTransaction(
      const BackingStoreVersionChangeTransaction&) = delete;
  BackingStoreVersionChangeTransaction& operator=(
      const BackingStoreVersionChangeTransaction&) = delete;
  ~BackingStoreVersionChangeTransaction() override;

  // BackingStore::Transaction:
  void Rollback() override;
  Status SetDatabaseVersion(int64_t version) override;
  // TODO(crbug.com/40253999): Implement the other relevant methods.

 private:
  BackingStoreDatabaseImpl::UpgradePassKey pass_key_;
};

}  // namespace content::indexed_db::sqlite

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_TRANSACTION_IMPL_H_
