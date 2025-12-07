// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_FACTORY_CLIENT_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_FACTORY_CLIENT_H_

#include <stddef.h>
#include <string>

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace blink {
struct IndexedDBDatabaseMetadata;
}

namespace content::indexed_db {

class MockMojoFactoryClient : public blink::mojom::IDBFactoryClient {
 public:
  explicit MockMojoFactoryClient();

  MockMojoFactoryClient(const MockMojoFactoryClient&) = delete;
  MockMojoFactoryClient& operator=(const MockMojoFactoryClient&) = delete;

  ~MockMojoFactoryClient() override;

  mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
  CreateInterfacePtrAndBind();

  MOCK_METHOD2(Error,
               void(blink::mojom::IDBException code,
                    const std::u16string& message));

  MOCK_METHOD1(SuccessStringList,
               void(const std::vector<std::u16string>& value));

  MOCK_METHOD1(Blocked, void(int64_t existing_version));

  MOCK_METHOD5(MockedUpgradeNeeded,
               void(mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase>*
                        pending_database,
                    int64_t old_version,
                    blink::mojom::IDBDataLoss data_loss,
                    const std::string& data_loss_message,
                    const blink::IndexedDBDatabaseMetadata& metadata));

  // Move-only types not supported by mock methods.
  void UpgradeNeeded(
      mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database,
      int64_t old_version,
      blink::mojom::IDBDataLoss data_loss,
      const std::string& data_loss_message,
      const blink::IndexedDBDatabaseMetadata& metadata) override {
    MockedUpgradeNeeded(&pending_database, old_version, data_loss,
                        data_loss_message, metadata);
  }

  MOCK_METHOD2(MockedOpenSuccess,
               void(mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase>*
                        pending_database,
                    const blink::IndexedDBDatabaseMetadata& metadata));
  void OpenSuccess(
      mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database,
      const blink::IndexedDBDatabaseMetadata& metadata) override {
    MockedOpenSuccess(&pending_database, metadata);
  }

  MOCK_METHOD1(DeleteSuccess, void(int64_t old_version));
  MOCK_METHOD0(Success, void());

 private:
  mojo::AssociatedReceiver<blink::mojom::IDBFactoryClient> receiver_{this};
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_FACTORY_CLIENT_H_
