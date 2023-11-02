// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_CALLBACKS_H_

#include <memory>

#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink-forward.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_error.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_name_and_version.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks.h"

namespace blink {

class MockWebIDBCallbacks : public WebIDBCallbacks {
 public:
  MockWebIDBCallbacks();
  ~MockWebIDBCallbacks() override;

  // Disallow copy and assign.
  MockWebIDBCallbacks(const MockWebIDBCallbacks&) = delete;
  MockWebIDBCallbacks& operator=(const MockWebIDBCallbacks&) = delete;

  void SetState(base::WeakPtr<WebIDBCursor>, int64_t) override;

  MOCK_METHOD2(Error, void(mojom::blink::IDBException, const String&));

  void SuccessCursorContinue(
      std::unique_ptr<IDBKey>,
      std::unique_ptr<IDBKey> primaryKey,
      absl::optional<std::unique_ptr<IDBValue>>) override;
  MOCK_METHOD3(DoSuccessCursorContinue,
               void(const std::unique_ptr<IDBKey>& key,
                    const std::unique_ptr<IDBKey>& primaryKey,
                    const absl::optional<std::unique_ptr<IDBValue>>& value));

  MOCK_METHOD1(SuccessNamesAndVersionsList,
               void(Vector<mojom::blink::IDBNameAndVersionPtr>));

  void SuccessCursor(
      mojo::PendingAssociatedRemote<mojom::blink::IDBCursor> cursor_info,
      std::unique_ptr<IDBKey> key,
      std::unique_ptr<IDBKey> primary_key,
      absl::optional<std::unique_ptr<IDBValue>> optional_value) override;
  MOCK_METHOD4(
      DoSuccessCursor,
      void(const mojo::PendingAssociatedRemote<mojom::blink::IDBCursor>&
               cursor_info,
           const std::unique_ptr<IDBKey>& key,
           const std::unique_ptr<IDBKey>& primary_key,
           const absl::optional<std::unique_ptr<IDBValue>>& optional_value));

  MOCK_METHOD3(SuccessCursorPrefetch,
               void(Vector<std::unique_ptr<IDBKey>> keys,
                    Vector<std::unique_ptr<IDBKey>> primary_keys,
                    Vector<std::unique_ptr<IDBValue>> values));

  MOCK_METHOD2(SuccessDatabase,
               void(mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase>,
                    const IDBDatabaseMetadata&));

  void SuccessKey(std::unique_ptr<IDBKey>) override;
  MOCK_METHOD1(DoSuccessKey, void(const std::unique_ptr<IDBKey>&));

  void SuccessValue(mojom::blink::IDBReturnValuePtr) override;
  MOCK_METHOD1(DoSuccessValue, void(const mojom::blink::IDBReturnValuePtr&));

  void SuccessArray(Vector<mojom::blink::IDBReturnValuePtr>) override;
  MOCK_METHOD1(DoSuccessArray,
               void(const Vector<mojom::blink::IDBReturnValuePtr>&));

  void SuccessArrayArray(
      Vector<Vector<mojom::blink::IDBReturnValuePtr>>) override;

  MOCK_METHOD1(DoSuccessArrayArray,
               void(const Vector<Vector<mojom::blink::IDBReturnValuePtr>>&));

  MOCK_METHOD1(SuccessInteger, void(int64_t));

  MOCK_METHOD0(Success, void());

  MOCK_METHOD2(
      ReceiveGetAllResults,
      void(bool,
           mojo::PendingReceiver<mojom::blink::IDBDatabaseGetAllResultSink>));

  MOCK_METHOD1(Blocked, void(int64_t oldVersion));

  MOCK_METHOD5(UpgradeNeeded,
               void(mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase>,
                    int64_t oldVersion,
                    mojom::IDBDataLoss dataLoss,
                    const String& dataLossMessage,
                    const IDBDatabaseMetadata&));

  MOCK_METHOD0(DetachRequestFromCallback, void());
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_CALLBACKS_H_
