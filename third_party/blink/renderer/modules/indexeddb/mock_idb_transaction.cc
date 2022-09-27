// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/mock_idb_transaction.h"

namespace blink {

void MockIDBTransaction::Bind(
    mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction> receiver) {
  receiver_.Bind(std::move(receiver));
}

}  // namespace blink
