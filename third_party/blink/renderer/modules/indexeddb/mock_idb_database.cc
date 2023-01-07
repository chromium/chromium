// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/mock_idb_database.h"

namespace blink {

void MockIDBDatabase::Bind(
    mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabase> receiver) {
  receiver_.Bind(std::move(receiver));
}

mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase>
MockIDBDatabase::BindNewEndpointAndPassDedicatedRemote() {
  return receiver_.BindNewEndpointAndPassDedicatedRemote();
}

}  // namespace blink
