// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_

#include <stdint.h>

#include "content/browser/indexed_db/indexed_db_backing_store.h"

namespace content {

class FakeTransaction : public IndexedDBBackingStore::Transaction {
 public:
  FakeTransaction(leveldb::Status phase_two_result,
                  blink::mojom::IDBTransactionMode mode,
                  base::WeakPtr<IndexedDBBackingStore> backing_store);
  explicit FakeTransaction(leveldb::Status phase_two_result);

  FakeTransaction(const FakeTransaction&) = delete;
  FakeTransaction& operator=(const FakeTransaction&) = delete;

  void Begin(std::vector<PartitionedLock> locks) override;
  leveldb::Status CommitPhaseOne(BlobWriteCallback) override;
  leveldb::Status CommitPhaseTwo() override;
  uint64_t GetTransactionSize() override;
  void Rollback() override;

 private:
  leveldb::Status result_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_
