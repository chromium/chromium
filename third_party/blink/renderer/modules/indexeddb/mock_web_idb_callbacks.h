// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_CALLBACKS_H_

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_error.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_metadata.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_name_and_version.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_value.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/web/web_heap.h"

namespace blink {

class MockWebIDBCallbacks : public blink::WebIDBCallbacks {
 public:
  MockWebIDBCallbacks();
  ~MockWebIDBCallbacks() override;
  MOCK_METHOD1(OnError, void(const blink::WebIDBDatabaseError&));

  void OnSuccess(blink::WebIDBKey,
                 blink::WebIDBKey primaryKey,
                 blink::WebIDBValue) override;
  MOCK_METHOD3(DoOnSuccess,
               void(const blink::WebIDBKey& key,
                    const blink::WebIDBKey& primaryKey,
                    const blink::WebIDBValue& value));

  MOCK_METHOD1(OnSuccess,
               void(const blink::WebVector<blink::WebIDBNameAndVersion>&));
  MOCK_METHOD1(OnSuccess, void(const blink::WebVector<blink::WebString>&));

  void OnSuccess(blink::WebIDBCursor* cursor,
                 blink::WebIDBKey key,
                 blink::WebIDBKey primaryKey,
                 blink::WebIDBValue value) override;
  MOCK_METHOD4(DoOnSuccess,
               void(blink::WebIDBCursor*,
                    const blink::WebIDBKey&,
                    const blink::WebIDBKey& primaryKey,
                    const blink::WebIDBValue&));

  MOCK_METHOD2(OnSuccess,
               void(blink::WebIDBDatabase*, const blink::WebIDBMetadata&));
  void OnSuccess(blink::WebIDBKey) override;
  MOCK_METHOD1(DoOnSuccess, void(const blink::WebIDBKey&));

  void OnSuccess(blink::WebIDBValue) override;
  MOCK_METHOD1(DoOnSuccess, void(const blink::WebIDBValue&));

  void OnSuccess(blink::WebVector<blink::WebIDBValue>) override;
  MOCK_METHOD1(DoOnSuccess, void(const blink::WebVector<blink::WebIDBValue>&));

  MOCK_METHOD1(OnSuccess, void(long long));
  MOCK_METHOD0(OnSuccess, void());
  MOCK_METHOD1(OnBlocked, void(long long oldVersion));
  MOCK_METHOD5(OnUpgradeNeeded,
               void(long long oldVersion,
                    blink::WebIDBDatabase*,
                    const blink::WebIDBMetadata&,
                    unsigned short dataLoss,
                    blink::WebString dataLossMessage));
  MOCK_METHOD0(Detach, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebIDBCallbacks);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_CALLBACKS_H_
