// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace blink {
struct IndexedDBDatabaseMetadata;
}

namespace content {
class IndexedDBConnection;
class IndexedDBCursor;
class IndexedDBDatabase;
struct IndexedDBDataLossInfo;

class CONTENT_EXPORT IndexedDBCallbacks
    : public base::RefCounted<IndexedDBCallbacks> {
 public:
  IndexedDBCallbacks(base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
                     const absl::optional<storage::BucketInfo>& bucket,
                     mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
                         pending_callbacks,
                     scoped_refptr<base::SequencedTaskRunner> idb_runner);

  IndexedDBCallbacks(const IndexedDBCallbacks&) = delete;
  IndexedDBCallbacks& operator=(const IndexedDBCallbacks&) = delete;

  virtual void OnError(const IndexedDBDatabaseError& error);

  // IndexedDBFactory::Open / DeleteDatabase
  virtual void OnBlocked(int64_t existing_version);

  // IndexedDBFactory::Open
  virtual void OnUpgradeNeeded(int64_t old_version,
                               std::unique_ptr<IndexedDBConnection> connection,
                               const blink::IndexedDBDatabaseMetadata& metadata,
                               const IndexedDBDataLossInfo& data_loss_info);
  virtual void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                         const blink::IndexedDBDatabaseMetadata& metadata);

  // IndexedDBDatabase::Count
  // IndexedDBFactory::DeleteDatabase
  // IndexedDBDatabase::DeleteRange
  // IndexedDBDatabase::GetKeyGeneratorCurrentNumber
  virtual void OnSuccess(int64_t value);

  // IndexedDBCursor::Continue / Advance (when complete)
  virtual void OnSuccess();

  void OnConnectionError();

  bool is_complete() const { return complete_; }

 protected:
  virtual ~IndexedDBCallbacks();

 private:
  friend class base::RefCounted<IndexedDBCallbacks>;

  // Stores if this callbacks object is complete and should not be called again.
  bool complete_ = false;

  // Depending on whether the database needs upgrading, we create connections in
  // different spots. This stores if we've already created the connection so
  // OnSuccess(Connection) doesn't create an extra one.
  bool connection_created_ = false;

  // Used to assert that OnSuccess is only called if there was no data loss.
  blink::mojom::IDBDataLoss data_loss_;

  // The "blocked" event should be sent at most once per request.
  bool sent_blocked_ = false;

  base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host_;
  absl::optional<storage::BucketInfo> bucket_info_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;
  mojo::AssociatedRemote<blink::mojom::IDBCallbacks> callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_H_
