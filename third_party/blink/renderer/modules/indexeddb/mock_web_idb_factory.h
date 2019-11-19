// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_FACTORY_H_

#include <memory>

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_factory.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace WTF {
class String;
}

namespace blink {

class MockWebIDBFactory : public testing::StrictMock<blink::WebIDBFactory> {
 public:
  MockWebIDBFactory();
  ~MockWebIDBFactory() override;

  void GetDatabaseInfo(std::unique_ptr<WebIDBCallbacks>);
  MOCK_METHOD1(GetDatabaseNames, void(std::unique_ptr<WebIDBCallbacks>));
  MOCK_METHOD6(
      Open,
      void(const WTF::String& name,
           int64_t version,
           mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
               transaction_receiver,
           int64_t transaction_id,
           std::unique_ptr<WebIDBCallbacks>,
           std::unique_ptr<WebIDBDatabaseCallbacks>));
  MOCK_METHOD3(DeleteDatabase,
               void(const WTF::String& name,
                    std::unique_ptr<WebIDBCallbacks>,
                    bool force_close));

  void SetCallbacksPointer(std::unique_ptr<WebIDBCallbacks>* callbacks);

 private:
  std::unique_ptr<WebIDBCallbacks>* callbacks_ptr_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_FACTORY_H_
