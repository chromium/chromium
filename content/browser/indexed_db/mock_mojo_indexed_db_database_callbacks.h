// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_DATABASE_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_DATABASE_CALLBACKS_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content::indexed_db {

class MockMojoDatabaseCallbacks : public blink::mojom::IDBDatabaseCallbacks {
 public:
  MockMojoDatabaseCallbacks();

  MockMojoDatabaseCallbacks(const MockMojoDatabaseCallbacks&) = delete;
  MockMojoDatabaseCallbacks& operator=(const MockMojoDatabaseCallbacks&) =
      delete;

  ~MockMojoDatabaseCallbacks() override;

  // Creates a remote that must be passed over another mojo pipe before it's
  // used.
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
  CreateInterfacePtrAndBind();

  // Creates a remote that has its own mojo pipe.
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
  BindNewEndpointAndPassDedicatedRemote();

  void FlushForTesting();

  MOCK_METHOD0(ForcedClose, void());
  MOCK_METHOD2(VersionChange, void(int64_t old_version, int64_t new_version));
  MOCK_METHOD3(Abort,
               void(int64_t transaction_id,
                    blink::mojom::IDBException code,
                    const std::u16string& message));
  MOCK_METHOD1(Complete, void(int64_t transaction_id));

 private:
  mojo::AssociatedReceiver<blink::mojom::IDBDatabaseCallbacks> receiver_{this};
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_DATABASE_CALLBACKS_H_
