// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_leveldb_env.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
namespace {

using blink::IndexedDBKey;
using blink::IndexedDBKeyRange;
using leveldb::Status;

TransactionalLevelDBFactory* GetTransactionalLevelDBFactory() {
  static base::NoDestructor<DefaultTransactionalLevelDBFactory> factory;
  return factory.get();
}

const storage::BucketLocator GetBucketLocator(blink::StorageKey storage_key) {
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  return bucket_locator;
}

}  // namespace

IndexedDBFakeBackingStore::IndexedDBFakeBackingStore()
    : IndexedDBBackingStore(IndexedDBBackingStore::Mode::kInMemory,
                            GetTransactionalLevelDBFactory(),
                            storage::BucketLocator(GetBucketLocator(
                                blink::StorageKey::CreateFromStringForTesting(
                                    "http://localhost:81"))),
                            base::FilePath(),
                            std::unique_ptr<TransactionalLevelDBDatabase>(),
                            /*blob_storage_context=*/nullptr,
                            /*file_system_access_context=*/nullptr,
                            std::make_unique<storage::FilesystemProxy>(
                                storage::FilesystemProxy::UNRESTRICTED,
                                base::FilePath()),
                            BlobFilesCleanedCallback(),
                            ReportOutstandingBlobsCallback(),
                            base::SequencedTaskRunner::GetCurrentDefault()) {}
IndexedDBFakeBackingStore::IndexedDBFakeBackingStore(
    BlobFilesCleanedCallback blob_files_cleaned,
    ReportOutstandingBlobsCallback report_outstanding_blobs,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : IndexedDBBackingStore(IndexedDBBackingStore::Mode::kOnDisk,
                            GetTransactionalLevelDBFactory(),
                            storage::BucketLocator(GetBucketLocator(
                                blink::StorageKey::CreateFromStringForTesting(
                                    "http://localhost:81"))),
                            base::FilePath(),
                            std::unique_ptr<TransactionalLevelDBDatabase>(),
                            /*blob_storage_context=*/nullptr,
                            /*file_system_access_context=*/nullptr,
                            std::make_unique<storage::FilesystemProxy>(
                                storage::FilesystemProxy::UNRESTRICTED,
                                base::FilePath()),
                            std::move(blob_files_cleaned),
                            std::move(report_outstanding_blobs),
                            task_runner) {}
IndexedDBFakeBackingStore::~IndexedDBFakeBackingStore() = default;

Status IndexedDBFakeBackingStore::CreateDatabase(
    blink::IndexedDBDatabaseMetadata& metadata) {
  return Status::OK();
}

Status IndexedDBFakeBackingStore::DeleteDatabase(
    const std::u16string& name,
    TransactionalLevelDBTransaction* transaction) {
  return Status::OK();
}

Status IndexedDBFakeBackingStore::SetDatabaseVersion(
    Transaction* transaction,
    int64_t row_id,
    int64_t version,
    blink::IndexedDBDatabaseMetadata* metadata) {
  metadata->version = version;
  return leveldb::Status::OK();
}

Status IndexedDBFakeBackingStore::CreateObjectStore(
    Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    std::u16string name,
    blink::IndexedDBKeyPath key_path,
    bool auto_increment,
    blink::IndexedDBObjectStoreMetadata* metadata) {
  metadata->name = std::move(name);
  metadata->id = object_store_id;
  metadata->key_path = std::move(key_path);
  metadata->auto_increment = auto_increment;
  metadata->max_index_id = blink::IndexedDBObjectStoreMetadata::kMinimumIndexId;
  return Status::OK();
}

Status IndexedDBFakeBackingStore::DeleteObjectStore(
    Transaction* transaction,
    int64_t database_id,
    const blink::IndexedDBObjectStoreMetadata& object_store) {
  return Status::OK();
}

Status IndexedDBFakeBackingStore::RenameObjectStore(
    Transaction* transaction,
    int64_t database_id,
    std::u16string new_name,
    std::u16string* old_name,
    blink::IndexedDBObjectStoreMetadata* metadata) {
  *old_name = std::move(metadata->name);
  metadata->name = std::move(new_name);
  return Status::OK();
}

Status IndexedDBFakeBackingStore::CreateIndex(
    Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    std::u16string name,
    blink::IndexedDBKeyPath key_path,
    bool is_unique,
    bool is_multi_entry,
    blink::IndexedDBIndexMetadata* metadata) {
  metadata->id = index_id;
  metadata->name = std::move(name);
  metadata->key_path = key_path;
  metadata->unique = is_unique;
  metadata->multi_entry = is_multi_entry;
  return Status::OK();
}

Status IndexedDBFakeBackingStore::DeleteIndex(
    Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const blink::IndexedDBIndexMetadata& metadata) {
  return Status::OK();
}

Status IndexedDBFakeBackingStore::RenameIndex(
    Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    std::u16string new_name,
    std::u16string* old_name,
    blink::IndexedDBIndexMetadata* metadata) {
  *old_name = std::move(metadata->name);
  metadata->name = std::move(new_name);
  return Status::OK();
}

Status IndexedDBFakeBackingStore::PutRecord(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKey& key,
    IndexedDBValue* value,
    RecordIdentifier* record) {
  return Status::OK();
}

Status IndexedDBFakeBackingStore::ClearObjectStore(Transaction*,
                                                   int64_t database_id,
                                                   int64_t object_store_id) {
  return Status::OK();
}
Status IndexedDBFakeBackingStore::DeleteRecord(Transaction*,
                                               int64_t database_id,
                                               int64_t object_store_id,
                                               const RecordIdentifier&) {
  return Status::OK();
}
Status IndexedDBFakeBackingStore::GetKeyGeneratorCurrentNumber(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    int64_t* current_number) {
  return Status::OK();
}
Status IndexedDBFakeBackingStore::MaybeUpdateKeyGeneratorCurrentNumber(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    int64_t new_number,
    bool check_current) {
  return Status::OK();
}
Status IndexedDBFakeBackingStore::KeyExistsInObjectStore(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKey&,
    RecordIdentifier* found_record_identifier,
    bool* found) {
  return Status::OK();
}

Status IndexedDBFakeBackingStore::ReadMetadataForDatabaseName(
    const std::u16string& name,
    blink::IndexedDBDatabaseMetadata* metadata,
    bool* found) {
  return Status::OK();
}

Status IndexedDBFakeBackingStore::ClearIndex(Transaction*,
                                             int64_t database_id,
                                             int64_t object_store_id,
                                             int64_t index_id) {
  return Status::OK();
}

Status IndexedDBFakeBackingStore::PutIndexDataForRecord(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKey&,
    const RecordIdentifier&) {
  return Status::OK();
}

void IndexedDBFakeBackingStore::ReportBlobUnused(int64_t database_id,
                                                 int64_t blob_number) {}

std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenObjectStoreKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection,
    Status* s) {
  return nullptr;
}
std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenObjectStoreCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection,
    Status* s) {
  return nullptr;
}
std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenIndexKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection,
    Status* s) {
  return nullptr;
}
std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenIndexCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection,
    Status* s) {
  return nullptr;
}

IndexedDBFakeBackingStore::FakeTransaction::FakeTransaction(Status result)
    : FakeTransaction(result, blink::mojom::IDBTransactionMode::ReadWrite) {}
IndexedDBFakeBackingStore::FakeTransaction::FakeTransaction(
    Status result,
    blink::mojom::IDBTransactionMode mode)
    : IndexedDBBackingStore::Transaction(
          nullptr,
          blink::mojom::IDBTransactionDurability::Relaxed,
          mode),
      result_(result) {}
void IndexedDBFakeBackingStore::FakeTransaction::Begin(
    std::vector<PartitionedLock> locks) {}
Status IndexedDBFakeBackingStore::FakeTransaction::CommitPhaseOne(
    BlobWriteCallback callback) {
  return std::move(callback).Run(
      BlobWriteResult::kRunPhaseTwoAndReturnResult,
      storage::mojom::WriteBlobToFileResult::kSuccess);
}
Status IndexedDBFakeBackingStore::FakeTransaction::CommitPhaseTwo() {
  return result_;
}
uint64_t IndexedDBFakeBackingStore::FakeTransaction::GetTransactionSize() {
  return 0;
}
void IndexedDBFakeBackingStore::FakeTransaction::Rollback() {}

std::unique_ptr<IndexedDBBackingStore::Transaction>
IndexedDBFakeBackingStore::CreateTransaction(
    blink::mojom::IDBTransactionDurability durability,
    blink::mojom::IDBTransactionMode mode) {
  return std::make_unique<FakeTransaction>(Status::OK(), mode);
}

}  // namespace content
