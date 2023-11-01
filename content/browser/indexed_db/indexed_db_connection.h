// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace content {
class IndexedDBDatabaseCallbacks;
class IndexedDBDatabaseError;
class IndexedDBTransaction;
class IndexedDBBucketContextHandle;

class CONTENT_EXPORT IndexedDBConnection {
 public:
  IndexedDBConnection(
      IndexedDBBucketContext& bucket_context,
      base::WeakPtr<IndexedDBDatabase> database,
      base::RepeatingClosure on_version_change_ignored,
      base::OnceCallback<void(IndexedDBConnection*)> on_close,
      std::unique_ptr<IndexedDBDatabaseCallbacks> callbacks,
      scoped_refptr<IndexedDBClientStateCheckerWrapper> client_state_checker);

  IndexedDBConnection(const IndexedDBConnection&) = delete;
  IndexedDBConnection& operator=(const IndexedDBConnection&) = delete;

  virtual ~IndexedDBConnection();

  enum class CloseErrorHandling {
    // Returns from the function on the first encounter with an error.
    kReturnOnFirstError,
    // Continues to call Abort() on all transactions despite any errors.
    // The last error encountered is returned.
    kAbortAllReturnLastError,
  };

  // The return value is `callbacks_`, passing ownership.
  std::unique_ptr<IndexedDBDatabaseCallbacks> AbortTransactionsAndClose(
      CloseErrorHandling error_handling);

  void CloseAndReportForceClose();
  bool IsConnected();

  void VersionChangeIgnored();

  int32_t id() const { return id_; }

  base::WeakPtr<IndexedDBDatabase> database() const { return database_; }
  IndexedDBDatabaseCallbacks* callbacks() const { return callbacks_.get(); }
  base::WeakPtr<IndexedDBConnection> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  IndexedDBTransaction* CreateVersionChangeTransaction(
      int64_t id,
      const std::set<int64_t>& scope,
      IndexedDBBackingStore::Transaction* backing_store_transaction);

  IndexedDBTransaction* CreateTransaction(
      mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
          transaction_receiver,
      int64_t id,
      const std::set<int64_t>& scope,
      blink::mojom::IDBTransactionMode mode,
      IndexedDBBackingStore::Transaction* backing_store_transaction);

  void AbortTransactionAndTearDownOnError(IndexedDBTransaction* transaction,
                                          const IndexedDBDatabaseError& error);

  leveldb::Status AbortAllTransactions(const IndexedDBDatabaseError& error);

  // Returns the last error that occurred, if there is any.
  leveldb::Status AbortAllTransactionsAndIgnoreErrors(
      const IndexedDBDatabaseError& error);

  IndexedDBTransaction* GetTransaction(int64_t id) const;

  // We ignore calls where the id doesn't exist to facilitate the AbortAll call.
  // TODO(dmurph): Change that so this doesn't need to ignore unknown ids.
  void RemoveTransaction(int64_t id);

  // Checks if the client is in inactive state and disallow it from activation
  // if so. This is called when the client is not supposed to be inactive,
  // otherwise it may affect the IndexedDB service (e.g. blocking others from
  // acquiring the locks).
  void DisallowInactiveClient(
      storage::mojom::DisallowInactiveClientReason reason,
      base::OnceCallback<void(bool)> callback);

  const std::map<int64_t, std::unique_ptr<IndexedDBTransaction>>& transactions()
      const {
    return transactions_;
  }

  IndexedDBBucketContext* bucket_context() {
    return bucket_context_handle_.bucket_context();
  }

 private:
  const int32_t id_;

  // Keeps the factory for this bucket alive.
  IndexedDBBucketContextHandle bucket_context_handle_;

  base::WeakPtr<IndexedDBDatabase> database_;
  base::RepeatingClosure on_version_change_ignored_;
  base::OnceCallback<void(IndexedDBConnection*)> on_close_;

  // The connection owns transactions created on this connection. It's important
  // to preserve ordering.
  std::map<int64_t, std::unique_ptr<IndexedDBTransaction>> transactions_;

  // The callbacks_ member is cleared when the connection is closed.
  // May be nullptr in unit tests.
  std::unique_ptr<IndexedDBDatabaseCallbacks> callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<IndexedDBClientStateCheckerWrapper> client_state_checker_;
  mojo::RemoteSet<storage::mojom::IndexedDBClientKeepActive>
      client_keep_active_remotes_;

  base::WeakPtrFactory<IndexedDBConnection> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_
