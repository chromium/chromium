// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_pending_connection.h"

#include <utility>


namespace content {

IndexedDBPendingConnection::IndexedDBPendingConnection(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    int64_t transaction_id,
    int64_t version,
    base::OnceCallback<void(base::WeakPtr<IndexedDBTransaction>)>
        create_transaction_callback)
    : callbacks(callbacks),
      database_callbacks(database_callbacks),
      transaction_id(transaction_id),
      version(version),
      create_transaction_callback(std::move(create_transaction_callback)) {
}

IndexedDBPendingConnection::~IndexedDBPendingConnection() {}

}  // namespace content
