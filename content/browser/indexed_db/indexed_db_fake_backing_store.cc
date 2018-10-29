// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"

#include "base/files/file_path.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace content {
namespace {

using blink::IndexedDBKey;
using blink::IndexedDBKeyRange;

}  // namespace

IndexedDBFakeBackingStore::IndexedDBFakeBackingStore()
    : IndexedDBBackingStore(nullptr /* indexed_db_factory */,
                            url::Origin::Create(GURL("http://localhost:81")),
                            base::FilePath(),
                            std::unique_ptr<LevelDBDatabase>(),
                            std::unique_ptr<LevelDBComparator>(),
                            base::SequencedTaskRunnerHandle::Get().get()) {}
IndexedDBFakeBackingStore::IndexedDBFakeBackingStore(
    IndexedDBFactory* factory,
    base::SequencedTaskRunner* task_runner)
    : IndexedDBBackingStore(factory,
                            url::Origin::Create(GURL("http://localhost:81")),
                            base::FilePath(),
                            std::unique_ptr<LevelDBDatabase>(),
                            std::unique_ptr<LevelDBComparator>(),
                            task_runner) {}
IndexedDBFakeBackingStore::~IndexedDBFakeBackingStore() {}

leveldb::Status IndexedDBFakeBackingStore::DeleteDatabase(
    const base::string16& name) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::PutRecord(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKey& key,
    IndexedDBValue* value,
    RecordIdentifier* record) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::ClearObjectStore(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::DeleteRecord(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    const RecordIdentifier&) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::GetKeyGeneratorCurrentNumber(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    int64_t* current_number) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::MaybeUpdateKeyGeneratorCurrentNumber(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    int64_t new_number,
    bool check_current) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::KeyExistsInObjectStore(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKey&,
    RecordIdentifier* found_record_identifier,
    bool* found) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::ClearIndex(Transaction*,
                                                      int64_t database_id,
                                                      int64_t object_store_id,
                                                      int64_t index_id) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::PutIndexDataForRecord(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKey&,
    const RecordIdentifier&) {
  return leveldb::Status::OK();
}

void IndexedDBFakeBackingStore::ReportBlobUnused(int64_t database_id,
                                                 int64_t blob_key) {}

std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenObjectStoreKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range,
    blink::WebIDBCursorDirection,
    leveldb::Status* s) {
  return std::unique_ptr<IndexedDBBackingStore::Cursor>();
}
std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenObjectStoreCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range,
    blink::WebIDBCursorDirection,
    leveldb::Status* s) {
  return std::unique_ptr<IndexedDBBackingStore::Cursor>();
}
std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenIndexKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::WebIDBCursorDirection,
    leveldb::Status* s) {
  return std::unique_ptr<IndexedDBBackingStore::Cursor>();
}
std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenIndexCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::WebIDBCursorDirection,
    leveldb::Status* s) {
  return std::unique_ptr<IndexedDBBackingStore::Cursor>();
}

IndexedDBFakeBackingStore::FakeTransaction::FakeTransaction(
    leveldb::Status result)
    : IndexedDBBackingStore::Transaction(nullptr), result_(result) {}
void IndexedDBFakeBackingStore::FakeTransaction::Begin() {}
leveldb::Status IndexedDBFakeBackingStore::FakeTransaction::CommitPhaseOne(
    scoped_refptr<BlobWriteCallback> callback) {
  callback->Run(IndexedDBBackingStore::BlobWriteResult::SUCCESS_SYNC);
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::FakeTransaction::CommitPhaseTwo() {
  return result_;
}
uint64_t IndexedDBFakeBackingStore::FakeTransaction::GetTransactionSize() {
  return 0;
}
void IndexedDBFakeBackingStore::FakeTransaction::Rollback() {}

}  // namespace content
