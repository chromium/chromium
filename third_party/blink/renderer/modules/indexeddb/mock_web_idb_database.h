// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_DATABASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_DATABASE_H_

#include <gmock/gmock.h>
#include <memory>
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database.h"

namespace blink {

class MockWebIDBDatabase : public testing::StrictMock<WebIDBDatabase> {
 public:
  ~MockWebIDBDatabase() override;

  static std::unique_ptr<MockWebIDBDatabase> Create();

  MOCK_METHOD5(CreateObjectStore,
               void(long long transaction_id,
                    long long object_store_id,
                    const String& name,
                    const WebIDBKeyPath&,
                    bool auto_increment));
  MOCK_METHOD2(DeleteObjectStore,
               void(long long transaction_id, long long object_store_id));
  MOCK_METHOD3(RenameObjectStore,
               void(long long transaction_id,
                    long long object_store_id,
                    const String& new_name));
  MOCK_METHOD3(CreateTransaction,
               void(long long id,
                    const Vector<int64_t>& scope,
                    WebIDBTransactionMode));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(VersionChangeIgnored, void());
  MOCK_METHOD1(Abort, void(long long transaction_id));
  MOCK_METHOD1(Commit, void(long long transaction_id));
  MOCK_METHOD7(CreateIndex,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id,
                    const String& name,
                    const WebIDBKeyPath&,
                    bool unique,
                    bool multi_entry));
  MOCK_METHOD3(DeleteIndex,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id));
  MOCK_METHOD4(RenameIndex,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id,
                    const String& new_name));
  MOCK_METHOD6(
      AddObserver,
      void(long long transaction_id,
           int32_t observer_id,
           bool include_transaction,
           bool no_records,
           bool values,
           const std::bitset<kWebIDBOperationTypeCount>& operation_types));
  MOCK_CONST_METHOD1(ContainsObserverId, bool(int32_t id));
  MOCK_METHOD1(RemoveObservers,
               void(const Vector<int32_t>& observer_ids_to_remove));
  MOCK_METHOD6(Get,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id,
                    const WebIDBKeyRange&,
                    bool key_only,
                    WebIDBCallbacks*));
  MOCK_METHOD7(GetAll,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id,
                    const WebIDBKeyRange&,
                    long long max_count,
                    bool key_only,
                    WebIDBCallbacks*));

  MOCK_METHOD8(Put,
               void(long long transaction_id,
                    long long object_store_id,
                    const WebData& value,
                    const Vector<WebBlobInfo>&,
                    WebIDBKeyView primary_key,
                    WebIDBPutMode,
                    WebIDBCallbacks*,
                    const Vector<WebIDBIndexKeys>&));

  MOCK_METHOD4(SetIndexKeys,
               void(long long transaction_id,
                    long long object_store_id,
                    WebIDBKeyView primary_key,
                    const Vector<WebIDBIndexKeys>&));
  MOCK_METHOD3(SetIndexesReady,
               void(long long transaction_id,
                    long long object_store_id,
                    const Vector<int64_t>& index_ids));
  MOCK_METHOD8(OpenCursor,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id,
                    const WebIDBKeyRange&,
                    WebIDBCursorDirection,
                    bool key_only,
                    WebIDBTaskType,
                    WebIDBCallbacks*));
  MOCK_METHOD5(Count,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id,
                    const WebIDBKeyRange&,
                    WebIDBCallbacks*));
  MOCK_METHOD4(Delete,
               void(long long transaction_id,
                    long long object_store_id,
                    WebIDBKeyView primary_key,
                    WebIDBCallbacks*));
  MOCK_METHOD4(DeleteRange,
               void(long long transaction_id,
                    long long object_store_id,
                    const WebIDBKeyRange&,
                    WebIDBCallbacks*));
  MOCK_METHOD3(Clear,
               void(long long transaction_id,
                    long long object_store_id,
                    WebIDBCallbacks*));

 private:
  MockWebIDBDatabase();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_DATABASE_H_
