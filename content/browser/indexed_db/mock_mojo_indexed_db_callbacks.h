// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_CALLBACKS_H_

#include <stddef.h>
#include <string>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace blink {
struct IndexedDBDatabaseMetadata;
}

namespace content {

class MockMojoIndexedDBCallbacks : public blink::mojom::IDBCallbacks {
 public:
  explicit MockMojoIndexedDBCallbacks();
  ~MockMojoIndexedDBCallbacks() override;

  blink::mojom::IDBCallbacksAssociatedPtrInfo CreateInterfacePtrAndBind();

  MOCK_METHOD2(Error, void(int32_t code, const base::string16& message));

  MOCK_METHOD1(SuccessNamesAndVersionsList,
               void(std::vector<blink::mojom::IDBNameAndVersionPtr> list));

  MOCK_METHOD1(SuccessStringList,
               void(const std::vector<base::string16>& value));

  MOCK_METHOD1(Blocked, void(int64_t existing_version));

  MOCK_METHOD5(MockedUpgradeNeeded,
               void(blink::mojom::IDBDatabaseAssociatedPtrInfo* database_info,
                    int64_t old_version,
                    blink::WebIDBDataLoss data_loss,
                    const std::string& data_loss_message,
                    const blink::IndexedDBDatabaseMetadata& metadata));

  // Move-only types not supported by mock methods.
  void UpgradeNeeded(
      blink::mojom::IDBDatabaseAssociatedPtrInfo database_info,
      int64_t old_version,
      blink::WebIDBDataLoss data_loss,
      const std::string& data_loss_message,
      const blink::IndexedDBDatabaseMetadata& metadata) override {
    MockedUpgradeNeeded(&database_info, old_version, data_loss,
                        data_loss_message, metadata);
  }

  MOCK_METHOD2(MockedSuccessDatabase,
               void(blink::mojom::IDBDatabaseAssociatedPtrInfo* database_info,
                    const blink::IndexedDBDatabaseMetadata& metadata));
  void SuccessDatabase(
      blink::mojom::IDBDatabaseAssociatedPtrInfo database_info,
      const blink::IndexedDBDatabaseMetadata& metadata) override {
    MockedSuccessDatabase(&database_info, metadata);
  }

  MOCK_METHOD4(MockedSuccessCursor,
               void(blink::mojom::IDBCursorAssociatedPtrInfo* cursor,
                    const blink::IndexedDBKey& key,
                    const blink::IndexedDBKey& primary_key,
                    blink::mojom::IDBValuePtr* value));
  void SuccessCursor(blink::mojom::IDBCursorAssociatedPtrInfo cursor,
                     const blink::IndexedDBKey& key,
                     const blink::IndexedDBKey& primary_key,
                     blink::mojom::IDBValuePtr value) override {
    MockedSuccessCursor(&cursor, key, primary_key, &value);
  }

  MOCK_METHOD1(MockedSuccessValue,
               void(blink::mojom::IDBReturnValuePtr* value));
  void SuccessValue(blink::mojom::IDBReturnValuePtr value) override {
    MockedSuccessValue(&value);
  }

  MOCK_METHOD3(MockedSuccessCursorContinue,
               void(const blink::IndexedDBKey& key,
                    const blink::IndexedDBKey& primary_key,
                    blink::mojom::IDBValuePtr* value));

  void SuccessCursorContinue(const blink::IndexedDBKey& key,
                             const blink::IndexedDBKey& primary_key,
                             blink::mojom::IDBValuePtr value) override {
    MockedSuccessCursorContinue(key, primary_key, &value);
  }

  MOCK_METHOD3(MockedSuccessCursorPrefetch,
               void(const std::vector<blink::IndexedDBKey>& keys,
                    const std::vector<blink::IndexedDBKey>& primary_keys,
                    std::vector<blink::mojom::IDBValuePtr>* values));

  void SuccessCursorPrefetch(
      const std::vector<blink::IndexedDBKey>& keys,
      const std::vector<blink::IndexedDBKey>& primary_keys,
      std::vector<blink::mojom::IDBValuePtr> values) override {
    MockedSuccessCursorPrefetch(keys, primary_keys, &values);
  }

  MOCK_METHOD1(MockedSuccessArray,
               void(std::vector<blink::mojom::IDBReturnValuePtr>* values));
  void SuccessArray(
      std::vector<blink::mojom::IDBReturnValuePtr> values) override {
    MockedSuccessArray(&values);
  }

  MOCK_METHOD1(SuccessKey, void(const blink::IndexedDBKey& key));
  MOCK_METHOD1(SuccessInteger, void(int64_t value));
  MOCK_METHOD0(Success, void());

 private:
  mojo::AssociatedBinding<blink::mojom::IDBCallbacks> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockMojoIndexedDBCallbacks);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_CALLBACKS_H_
