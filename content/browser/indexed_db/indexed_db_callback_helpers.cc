// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_callback_helpers.h"

namespace content {
namespace indexed_db_callback_helpers_internal {

template <>
mojo::PendingReceiver<blink::mojom::IDBDatabaseGetAllResultSink> AbortCallback(
    base::WeakPtr<IndexedDBTransaction> transaction) {
  if (transaction)
    transaction->IncrementNumErrorsSent();

  mojo::Remote<blink::mojom::IDBDatabaseGetAllResultSink> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();

  IndexedDBDatabaseError error(blink::mojom::IDBException::kIgnorableAbortError,
                               "Backend aborted error");
  remote->OnError(blink::mojom::IDBError::New(error.code(), error.message()));
  return receiver;
}

}  // namespace indexed_db_callback_helpers_internal
}  // namespace content
