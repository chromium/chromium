// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_DATABASE_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_DATABASE_CALLBACKS_H_

#include <stdint.h>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {

class MockMojoIndexedDBDatabaseCallbacks
    : public blink::mojom::IDBDatabaseCallbacks {
 public:
  MockMojoIndexedDBDatabaseCallbacks();
  ~MockMojoIndexedDBDatabaseCallbacks() override;

  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
  CreateInterfacePtrAndBind();

  MOCK_METHOD0(ForcedClose, void());
  MOCK_METHOD2(VersionChange, void(int64_t old_version, int64_t new_version));
  MOCK_METHOD3(Abort,
               void(int64_t transaction_id,
                    blink::mojom::IDBException code,
                    const base::string16& message));
  MOCK_METHOD1(Complete, void(int64_t transaction_id));

  MOCK_METHOD1(MockedChanges,
               void(blink::mojom::IDBObserverChangesPtr* changes));
  void Changes(blink::mojom::IDBObserverChangesPtr changes) override {
    MockedChanges(&changes);
  }

 private:
  mojo::AssociatedReceiver<blink::mojom::IDBDatabaseCallbacks> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(MockMojoIndexedDBDatabaseCallbacks);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_DATABASE_CALLBACKS_H_
