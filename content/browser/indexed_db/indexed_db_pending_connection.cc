// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_pending_connection.h"

#include <utility>


namespace content {

IndexedDBPendingConnection::IndexedDBPendingConnection(
    std::unique_ptr<IndexedDBFactoryClient> factory_client,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    int64_t transaction_id,
    int64_t version,
    base::OnceCallback<void(base::WeakPtr<IndexedDBTransaction>)>
        create_transaction_callback)
    : factory_client(std::move(factory_client)),
      database_callbacks(database_callbacks),
      transaction_id(transaction_id),
      version(version),
      create_transaction_callback(std::move(create_transaction_callback)) {}

IndexedDBPendingConnection::~IndexedDBPendingConnection() {}

}  // namespace content
