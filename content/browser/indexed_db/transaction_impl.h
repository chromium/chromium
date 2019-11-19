// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_TRANSACTION_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_TRANSACTION_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class IndexedDBContextImpl;
class IndexedDBDispatcherHost;
class IndexedDBTransaction;

class TransactionImpl : public blink::mojom::IDBTransaction {
 public:
  explicit TransactionImpl(
      base::WeakPtr<IndexedDBTransaction> transaction,
      const url::Origin& origin,
      base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
      scoped_refptr<base::SequencedTaskRunner> idb_runner);
  ~TransactionImpl() override;

  // blink::mojom::IDBTransaction implementation
  void CreateObjectStore(int64_t object_store_id,
                         const base::string16& name,
                         const blink::IndexedDBKeyPath& key_path,
                         bool auto_increment) override;
  void DeleteObjectStore(int64_t object_store_id) override;
  void Put(int64_t object_store_id,
           blink::mojom::IDBValuePtr value,
           const blink::IndexedDBKey& key,
           blink::mojom::IDBPutMode mode,
           const std::vector<blink::IndexedDBIndexKeys>& index_keys,
           blink::mojom::IDBTransaction::PutCallback callback) override;
  void Commit(int64_t num_errors_handled) override;

  void OnGotUsageAndQuotaForCommit(blink::mojom::QuotaStatusCode status,
                                   int64_t usage,
                                   int64_t quota);

 private:
  class IOHelper;

  std::unique_ptr<IOHelper, BrowserThread::DeleteOnIOThread> io_helper_;

  base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  base::WeakPtr<IndexedDBTransaction> transaction_;
  const url::Origin origin_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TransactionImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TransactionImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_TRANSACTION_IMPL_H_
