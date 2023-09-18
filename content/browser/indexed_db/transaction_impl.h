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
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace content {
class IndexedDBTransaction;

class TransactionImpl : public blink::mojom::IDBTransaction {
 public:
  // Creates a self-owned `TransactionImpl` that deletes itself when its
  // mojo connection is closed.
  static void CreateAndBind(
      mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction> pending,
      base::WeakPtr<IndexedDBTransaction> transaction);

  ~TransactionImpl() override;

 private:
  explicit TransactionImpl(base::WeakPtr<IndexedDBTransaction> transaction);

  TransactionImpl(const TransactionImpl&) = delete;
  TransactionImpl& operator=(const TransactionImpl&) = delete;

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

  void OnQuotaCheckDone(bool allowed);

  // Turns an IDBValue into a set of IndexedDBExternalObjects in
  // |external_objects|.
  uint64_t CreateExternalObjects(
      blink::mojom::IDBValuePtr& value,
      std::vector<IndexedDBExternalObject>* external_objects);

  base::WeakPtr<IndexedDBTransaction> transaction_;

  // In bytes, the estimated additional space used on disk after this
  // transaction is committed.
  int64_t size_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TransactionImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_TRANSACTION_IMPL_H_
