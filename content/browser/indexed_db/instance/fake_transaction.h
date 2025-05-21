// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_FAKE_TRANSACTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_FAKE_TRANSACTION_H_

#include <stdint.h>

#include <memory>

#include "content/browser/indexed_db/instance/backing_store.h"

namespace content::indexed_db {

class FakeTransaction : public BackingStore::Transaction {
 public:
  FakeTransaction(Status phase_two_result,
                  std::unique_ptr<BackingStore::Transaction> wrapped);
  ~FakeTransaction() override;

  FakeTransaction(const FakeTransaction&) = delete;
  FakeTransaction& operator=(const FakeTransaction&) = delete;

  Status CommitPhaseOne(BlobWriteCallback) override;
  Status CommitPhaseTwo() override;
  void Rollback() override;
  void Begin(std::vector<PartitionedLock> locks) override;
  void Reset() override;
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
  base::expected<std::unique_ptr<indexed_db::BackingStore::Cursor>, Status>
  OpenObjectStoreKeyCursor(int64_t object_store_id,
                           const blink::IndexedDBKeyRange& key_range,
                           blink::mojom::IDBCursorDirection) override;
  base::expected<std::unique_ptr<indexed_db::BackingStore::Cursor>, Status>
  OpenObjectStoreCursor(int64_t object_store_id,
                        const blink::IndexedDBKeyRange& key_range,
                        blink::mojom::IDBCursorDirection) override;
  base::expected<std::unique_ptr<indexed_db::BackingStore::Cursor>, Status>
  OpenIndexKeyCursor(int64_t object_store_id,
                     int64_t index_id,
                     const blink::IndexedDBKeyRange& key_range,
                     blink::mojom::IDBCursorDirection) override;
  base::expected<std::unique_ptr<indexed_db::BackingStore::Cursor>, Status>
  OpenIndexCursor(int64_t object_store_id,
                  int64_t index_id,
                  const blink::IndexedDBKeyRange& key_range,
                  blink::mojom::IDBCursorDirection) override;

 private:
  Status result_;
  std::unique_ptr<BackingStore::Transaction> wrapped_transaction_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_FAKE_TRANSACTION_H_
