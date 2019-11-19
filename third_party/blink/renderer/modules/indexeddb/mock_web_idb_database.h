// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_DATABASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_DATABASE_H_

#include <gmock/gmock.h>
#include <memory>
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database.h"

namespace blink {

class MockWebIDBDatabase : public testing::StrictMock<WebIDBDatabase> {
 public:
  MockWebIDBDatabase();
  ~MockWebIDBDatabase() override;

  MOCK_METHOD3(RenameObjectStore,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    const String& new_name));
  MOCK_METHOD5(CreateTransaction,
               void(mojo::PendingAssociatedReceiver<
                        mojom::blink::IDBTransaction> receiver,
                    int64_t id,
                    const Vector<int64_t>& scope,
                    mojom::IDBTransactionMode,
                    mojom::IDBTransactionDurability));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(VersionChangeIgnored, void());
  MOCK_METHOD1(Abort, void(int64_t transaction_id));
  MOCK_METHOD7(CreateIndex,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    int64_t index_id,
                    const String& name,
                    const IDBKeyPath&,
                    bool unique,
                    bool multi_entry));
  MOCK_METHOD3(DeleteIndex,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    int64_t index_id));
  MOCK_METHOD4(RenameIndex,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    int64_t index_id,
                    const String& new_name));
  MOCK_METHOD6(
      AddObserver,
      void(int64_t transaction_id,
           int32_t observer_id,
           bool include_transaction,
           bool no_records,
           bool values,
           std::bitset<blink::kIDBOperationTypeCount> operation_types));
  MOCK_CONST_METHOD1(ContainsObserverId, bool(int32_t id));
  MOCK_METHOD1(RemoveObservers,
               void(const Vector<int32_t>& observer_ids_to_remove));
  MOCK_METHOD6(Get,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    int64_t index_id,
                    const IDBKeyRange*,
                    bool key_only,
                    WebIDBCallbacks*));
  MOCK_METHOD7(GetAll,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    int64_t index_id,
                    const IDBKeyRange*,
                    int64_t max_count,
                    bool key_only,
                    WebIDBCallbacks*));

  MOCK_METHOD4(SetIndexKeys,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    std::unique_ptr<IDBKey> primary_key,
                    Vector<IDBIndexKeys>));
  MOCK_METHOD3(SetIndexesReady,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    const Vector<int64_t>& index_ids));
  MOCK_METHOD8(OpenCursor,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    int64_t index_id,
                    const IDBKeyRange*,
                    mojom::IDBCursorDirection,
                    bool key_only,
                    mojom::IDBTaskType,
                    WebIDBCallbacks*));
  MOCK_METHOD5(Count,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    int64_t index_id,
                    const IDBKeyRange*,
                    WebIDBCallbacks*));
  MOCK_METHOD4(Delete,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    const IDBKey* primary_key,
                    WebIDBCallbacks*));
  MOCK_METHOD4(DeleteRange,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    const IDBKeyRange*,
                    WebIDBCallbacks*));
  MOCK_METHOD3(GetKeyGeneratorCurrentNumber,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    WebIDBCallbacks*));
  MOCK_METHOD3(Clear,
               void(int64_t transaction_id,
                    int64_t object_store_id,
                    WebIDBCallbacks*));
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_DATABASE_H_
