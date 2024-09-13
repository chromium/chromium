// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/pending_connection.h"

#include <utility>

#include "content/browser/indexed_db/instance/database_callbacks.h"
#include "content/browser/indexed_db/instance/factory_client.h"

namespace content::indexed_db {

PendingConnection::PendingConnection(
    std::unique_ptr<FactoryClient> factory_client,
    std::unique_ptr<DatabaseCallbacks> database_callbacks,
    int64_t transaction_id,
    int64_t version,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        pending_mojo_receiver)
    : factory_client(std::move(factory_client)),
      database_callbacks(std::move(database_callbacks)),
      transaction_id(transaction_id),
      version(version),
      pending_mojo_receiver(std::move(pending_mojo_receiver)) {}

PendingConnection::~PendingConnection() {}

}  // namespace content::indexed_db
