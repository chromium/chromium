// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
namespace {

using leveldb::Status;

}  // namespace

FakeTransaction::FakeTransaction(Status result)
    : FakeTransaction(result,
                      blink::mojom::IDBTransactionMode::ReadWrite,
                      nullptr) {}
FakeTransaction::FakeTransaction(
    Status result,
    blink::mojom::IDBTransactionMode mode,
    base::WeakPtr<IndexedDBBackingStore> backing_store)
    : IndexedDBBackingStore::Transaction(
          backing_store,
          blink::mojom::IDBTransactionDurability::Relaxed,
          mode),
      result_(result) {}
void FakeTransaction::Begin(std::vector<PartitionedLock> locks) {
  if (backing_store()) {
    Transaction::Begin(std::move(locks));
  }
}
Status FakeTransaction::CommitPhaseOne(BlobWriteCallback callback) {
  return std::move(callback).Run(
      BlobWriteResult::kRunPhaseTwoAndReturnResult,
      storage::mojom::WriteBlobToFileResult::kSuccess);
}
Status FakeTransaction::CommitPhaseTwo() {
  return result_;
}
uint64_t FakeTransaction::GetTransactionSize() {
  return 0;
}
void FakeTransaction::Rollback() {}

}  // namespace content
