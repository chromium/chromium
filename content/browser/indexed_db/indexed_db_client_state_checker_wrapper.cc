// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_client_state_checker_wrapper.h"

namespace content {

IndexedDBClientStateCheckerWrapper::IndexedDBClientStateCheckerWrapper(
    mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote) {
  if (client_state_checker_remote.is_valid()) {
    client_state_checker_remote_.Bind(std::move(client_state_checker_remote));
  }
}

IndexedDBClientStateCheckerWrapper::~IndexedDBClientStateCheckerWrapper() =
    default;

void IndexedDBClientStateCheckerWrapper::DisallowInactiveClient(
    storage::mojom::DisallowInactiveClientReason reason,
    mojo::PendingReceiver<storage::mojom::IndexedDBClientKeepActive>
        keep_active,
    storage::mojom::IndexedDBClientStateChecker::DisallowInactiveClientCallback
        callback) {
  if (client_state_checker_remote_.is_bound()) {
    client_state_checker_remote_->DisallowInactiveClient(
        reason, std::move(keep_active), std::move(callback));
  } else {
    // If the remote is no longer connected, we expect the client will terminate
    // the connection soon, so marking `was_active` true here.
    std::move(callback).Run(/*was_active=*/true);
  }
}

}  // namespace content
