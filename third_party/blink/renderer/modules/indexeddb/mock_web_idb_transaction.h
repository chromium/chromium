// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_TRANSACTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_TRANSACTION_H_

#include <gmock/gmock.h>
#include <memory>

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_transaction.h"

namespace blink {

class MockWebIDBTransaction : public testing::StrictMock<WebIDBTransaction> {
 public:
  MockWebIDBTransaction(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        int64_t transaction_id);
  MockWebIDBTransaction();
  ~MockWebIDBTransaction() override;

  MOCK_METHOD4(CreateObjectStore,
               void(int64_t object_store_id,
                    const String& name,
                    const IDBKeyPath&,
                    bool auto_increment));
  MOCK_METHOD1(DeleteObjectStore, void(int64_t object_store_id));
  MOCK_METHOD6(Put,
               void(int64_t object_store_id,
                    std::unique_ptr<IDBValue> value,
                    std::unique_ptr<IDBKey> primary_key,
                    mojom::IDBPutMode,
                    std::unique_ptr<WebIDBCallbacks>,
                    Vector<IDBIndexKeys>));
  MOCK_METHOD1(Commit, void(int64_t num_errors_handled));

  mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction> CreateReceiver()
      override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_TRANSACTION_H_
