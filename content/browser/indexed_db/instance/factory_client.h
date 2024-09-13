// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_FACTORY_CLIENT_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_FACTORY_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace blink {
struct IndexedDBDatabaseMetadata;
}

namespace content::indexed_db {
class Connection;
struct IndexedDBDataLossInfo;

// This class wraps the remote (renderer-side) object for handling database open
// or delete operations.
class CONTENT_EXPORT FactoryClient {
 public:
  explicit FactoryClient(
      mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
          pending_client);
  virtual ~FactoryClient();

  FactoryClient(const FactoryClient&) = delete;
  FactoryClient& operator=(const FactoryClient&) = delete;

  // Factory::Open / DeleteDatabase
  virtual void OnError(const DatabaseError& error);
  virtual void OnBlocked(int64_t existing_version);

  // Factory::Open
  virtual void OnUpgradeNeeded(int64_t old_version,
                               std::unique_ptr<Connection> connection,
                               const blink::IndexedDBDatabaseMetadata& metadata,
                               const IndexedDBDataLossInfo& data_loss_info);
  virtual void OnOpenSuccess(std::unique_ptr<Connection> connection,
                             const blink::IndexedDBDatabaseMetadata& metadata);

  // Factory::DeleteDatabase
  virtual void OnDeleteSuccess(int64_t old_version);

  void OnConnectionError();

  bool is_complete() const { return complete_; }

 private:
  // Stores if this callbacks object is complete and should not be called again.
  bool complete_ = false;

  // Depending on whether the database needs upgrading, we create connections in
  // different spots. This stores if we've already created the connection so
  // OnOpenSuccess() doesn't create an extra one.
  bool connection_created_ = false;

  // Used to assert that OnOpenSuccess() is only called if there was no data
  // loss.
  blink::mojom::IDBDataLoss data_loss_;

  // The "blocked" event should be sent at most once per request.
  bool sent_blocked_ = false;

  scoped_refptr<base::SequencedTaskRunner> idb_runner_;
  mojo::AssociatedRemote<blink::mojom::IDBFactoryClient> remote_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_FACTORY_CLIENT_H_
