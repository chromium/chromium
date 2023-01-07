// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_TRANSACTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_TRANSACTION_H_

#include <stdint.h>

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MODULES_EXPORT WebIDBTransaction final {
 public:
  WebIDBTransaction(scoped_refptr<base::SequencedTaskRunner> task_runner,
                    int64_t transaction_id);
  ~WebIDBTransaction();

  void CreateObjectStore(int64_t objectstore_id,
                         const String& name,
                         const IDBKeyPath&,
                         bool auto_increment);
  void DeleteObjectStore(int64_t object_store_id);
  void Put(int64_t object_store_id,
           std::unique_ptr<IDBValue> value,
           std::unique_ptr<IDBKey> primary_key,
           mojom::blink::IDBPutMode,
           std::unique_ptr<WebIDBCallbacks> callbacks,
           Vector<IDBIndexKeys>);
  void PutCallback(std::unique_ptr<WebIDBCallbacks> callbacks,
                   mojom::blink::IDBTransactionPutResultPtr result);
  void Commit(int64_t num_errors_handled);

  mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
  CreateReceiver();

  void FlushForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(WebIDBTransactionTest, ValueSizeTest);
  FRIEND_TEST_ALL_PREFIXES(WebIDBTransactionTest, KeyAndValueSizeTest);

  // Maximum size (in bytes) of value/key pair allowed for put requests. Any
  // requests larger than this size will be rejected.
  // Used by unit tests to exercise behavior without allocating huge chunks
  // of memory.
  size_t max_put_value_size_ =
      mojom::blink::kIDBMaxMessageSize - mojom::blink::kIDBMaxMessageOverhead;

  mojo::AssociatedRemote<mojom::blink::IDBTransaction> transaction_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  int64_t transaction_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_TRANSACTION_H_
