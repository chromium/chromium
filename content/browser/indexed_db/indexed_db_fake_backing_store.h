// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

namespace base {
class SequencedTaskRunner;
}

namespace blink {
class IndexedDBKeyRange;
}

namespace content {

class IndexedDBFakeBackingStore : public IndexedDBBackingStore {
 public:
  IndexedDBFakeBackingStore();
  IndexedDBFakeBackingStore(
      BlobFilesCleanedCallback blob_files_cleaned,
      ReportOutstandingBlobsCallback report_outstanding_blobs,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  IndexedDBFakeBackingStore(const IndexedDBFakeBackingStore&) = delete;
  IndexedDBFakeBackingStore& operator=(const IndexedDBFakeBackingStore&) =
      delete;

  ~IndexedDBFakeBackingStore() override;

  leveldb::Status DeleteDatabase(
      const std::u16string& name,
      TransactionalLevelDBTransaction* transaction) override;

  leveldb::Status PutRecord(IndexedDBBackingStore::Transaction* transaction,
                            int64_t database_id,
                            int64_t object_store_id,
                            const blink::IndexedDBKey& key,
                            IndexedDBValue* value,
                            RecordIdentifier* record) override;

  leveldb::Status ClearObjectStore(Transaction*,
                                   int64_t database_id,
                                   int64_t object_store_id) override;
  leveldb::Status DeleteRecord(Transaction*,
                               int64_t database_id,
                               int64_t object_store_id,
                               const RecordIdentifier&) override;
  leveldb::Status GetKeyGeneratorCurrentNumber(
      Transaction*,
      int64_t database_id,
      int64_t object_store_id,
      int64_t* current_number) override;
  leveldb::Status MaybeUpdateKeyGeneratorCurrentNumber(
      Transaction*,
      int64_t database_id,
      int64_t object_store_id,
      int64_t new_number,
      bool check_current) override;
  leveldb::Status KeyExistsInObjectStore(
      Transaction*,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKey&,
      RecordIdentifier* found_record_identifier,
      bool* found) override;

  leveldb::Status ClearIndex(Transaction*,
                             int64_t database_id,
                             int64_t object_store_id,
                             int64_t index_id) override;
  leveldb::Status PutIndexDataForRecord(Transaction*,
                                        int64_t database_id,
                                        int64_t object_store_id,
                                        int64_t index_id,
                                        const blink::IndexedDBKey&,
                                        const RecordIdentifier&) override;
  void ReportBlobUnused(int64_t database_id, int64_t blob_number) override;
  std::unique_ptr<Cursor> OpenObjectStoreKeyCursor(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      leveldb::Status*) override;
  std::unique_ptr<Cursor> OpenObjectStoreCursor(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      leveldb::Status*) override;
  std::unique_ptr<Cursor> OpenIndexKeyCursor(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      leveldb::Status*) override;
  std::unique_ptr<Cursor> OpenIndexCursor(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      leveldb::Status*) override;

  class FakeTransaction : public IndexedDBBackingStore::Transaction {
   public:
    FakeTransaction(leveldb::Status phase_two_result,
                    blink::mojom::IDBTransactionMode mode);
    explicit FakeTransaction(leveldb::Status phase_two_result);

    FakeTransaction(const FakeTransaction&) = delete;
    FakeTransaction& operator=(const FakeTransaction&) = delete;

    void Begin(std::vector<PartitionedLock> locks) override;
    leveldb::Status CommitPhaseOne(BlobWriteCallback) override;
    leveldb::Status CommitPhaseTwo() override;
    uint64_t GetTransactionSize() override;
    leveldb::Status Rollback() override;

   private:
    leveldb::Status result_;
  };

  std::unique_ptr<IndexedDBBackingStore::Transaction> CreateTransaction(
      blink::mojom::IDBTransactionDurability durability,
      blink::mojom::IDBTransactionMode mode) override;

 protected:
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_
