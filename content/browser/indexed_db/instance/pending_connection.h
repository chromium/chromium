// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_PENDING_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_PENDING_CONNECTION_H_

#include <stdint.h>

#include <optional>

#include "base/unguessable_token.h"
#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace content::indexed_db {

class DatabaseCallbacks;

// This struct holds data relevant to opening a new connection/database while
// ConnectionCoordinator manages queued operations.
struct CONTENT_EXPORT PendingConnection {
  PendingConnection(
      mojo::AssociatedRemote<blink::mojom::IDBFactoryClient> factory_client,
      std::unique_ptr<DatabaseCallbacks> database_callbacks,
      int64_t transaction_id,
      int64_t version,
      mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
          pending_mojo_receiver);
  ~PendingConnection();

  mojo::AssociatedRemote<blink::mojom::IDBFactoryClient> factory_client;
  std::unique_ptr<DatabaseCallbacks> database_callbacks;
  int64_t transaction_id;
  int64_t version;
  int scheduling_priority = 0;
  IndexedDBDataLossInfo data_loss_info;
  mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
      pending_mojo_receiver;
  storage::BucketClientInfo client_info;
  // If set, the client would like to share the connection that was created by
  // its open request with this `transaction_id`. See
  // `blink::mojom::IDBFactory::Open()`.
  std::optional<int64_t> shared_connection_id;

  base::UnguessableToken client_token() const {
    return client_info.document_token ? client_info.document_token->value()
                                      : client_info.context_token.value();
  }
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_PENDING_CONNECTION_H_
