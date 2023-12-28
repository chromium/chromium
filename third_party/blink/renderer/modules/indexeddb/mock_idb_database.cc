// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/mock_idb_database.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

void MockIDBDatabase::Bind(
    mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabase> receiver) {
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      WTF::BindOnce(&MockIDBDatabase::OnDisconnect, base::Unretained(this)));
}

mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase>
MockIDBDatabase::BindNewEndpointAndPassDedicatedRemote() {
  auto remote = receiver_.BindNewEndpointAndPassDedicatedRemote();
  receiver_.set_disconnect_handler(
      WTF::BindOnce(&MockIDBDatabase::OnDisconnect, base::Unretained(this)));
  return remote;
}

}  // namespace blink
