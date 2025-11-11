// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/pending_connection.h"

#include <atomic>
#include <cstdint>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "content/browser/indexed_db/instance/database_callbacks.h"

namespace content::indexed_db {

namespace {

// Number of pending connections in the process, across all BucketContexts. All
// operations use std::memory_order_relaxed since there is no dependency with
// other data.
//
// TODO(crbug.com/381086791): Remove after the bug is understood.
std::atomic_int64_t g_num_pending_connections = 0;

void IncrementNumPendingConnections() {
  int64_t new_pending_connection_count =
      g_num_pending_connections.fetch_add(1, std::memory_order_relaxed) + 1;

  // Report the number of pending connections when it's high. This will be used
  // to determine the proportion of clients with elevated number of pending
  // connections and as a trace trigger to understand how clients get into that
  // state.
  constexpr int64_t kHighPendingConnectionCount = 10000;
  if (new_pending_connection_count > kHighPendingConnectionCount) {
    base::UmaHistogramCounts100000(
        "IndexedDB.NumPendingConnections.OnCreateAbove10k",
        new_pending_connection_count);
  }
}

void DecrementNumPendingConnections() {
  g_num_pending_connections.fetch_sub(1, std::memory_order_relaxed);
}

}  // namespace

PendingConnection::PendingConnection(
    mojo::AssociatedRemote<blink::mojom::IDBFactoryClient> factory_client,
    std::unique_ptr<DatabaseCallbacks> database_callbacks,
    int64_t transaction_id,
    int64_t version,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        pending_mojo_receiver)
    : factory_client(std::move(factory_client)),
      database_callbacks(std::move(database_callbacks)),
      transaction_id(transaction_id),
      version(version),
      pending_mojo_receiver(std::move(pending_mojo_receiver)) {
  IncrementNumPendingConnections();
}

PendingConnection::~PendingConnection() {
  DecrementNumPendingConnections();
}

}  // namespace content::indexed_db
