// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_TRANSACTION_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_TRANSACTION_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
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
      const storage::BucketLocator& bucket_locator,
      base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
      scoped_refptr<base::SequencedTaskRunner> idb_runner);

  TransactionImpl(const TransactionImpl&) = delete;
  TransactionImpl& operator=(const TransactionImpl&) = delete;

  ~TransactionImpl() override;

  // blink::mojom::IDBTransaction implementation
  void CreateObjectStore(int64_t object_store_id,
                         const std::u16string& name,
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
  // Turns an IDBValue into a set of IndexedDBExternalObjects in
  // |external_objects|.
  void CreateExternalObjects(
      blink::mojom::IDBValuePtr& value,
      std::vector<IndexedDBExternalObject>* external_objects);

  base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  base::WeakPtr<IndexedDBTransaction> transaction_;
  const storage::BucketLocator bucket_locator_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TransactionImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_TRANSACTION_IMPL_H_
