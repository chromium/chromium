// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_FAKE_TRANSACTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_FAKE_TRANSACTION_H_

#include <stdint.h>

#include "content/browser/indexed_db/instance/backing_store.h"

namespace content::indexed_db {

class FakeTransaction : public BackingStore::Transaction {
 public:
  FakeTransaction(Status phase_two_result,
                  blink::mojom::IDBTransactionMode mode,
                  base::WeakPtr<BackingStore> backing_store);
  explicit FakeTransaction(Status phase_two_result);

  FakeTransaction(const FakeTransaction&) = delete;
  FakeTransaction& operator=(const FakeTransaction&) = delete;

  void Begin(std::vector<PartitionedLock> locks) override;
  Status CommitPhaseOne(BlobWriteCallback) override;
  Status CommitPhaseTwo() override;
  uint64_t GetTransactionSize() override;
  void Rollback() override;

 private:
  Status result_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_FAKE_TRANSACTION_H_
