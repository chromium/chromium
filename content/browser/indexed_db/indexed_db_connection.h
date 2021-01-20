// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_

#include <memory>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_origin_state_handle.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace content {
class IndexedDBDatabaseCallbacks;
class IndexedDBDatabaseError;
class IndexedDBObserver;
class IndexedDBTransaction;
class IndexedDBOriginStateHandle;

class CONTENT_EXPORT IndexedDBConnection {
 public:
  IndexedDBConnection(IndexedDBOriginStateHandle origin_state_handle,
                      IndexedDBClassFactory* indexed_db_class_factory,
                      base::WeakPtr<IndexedDBDatabase> database,
                      base::RepeatingClosure on_version_change_ignored,
                      base::OnceCallback<void(IndexedDBConnection*)> on_close,
                      scoped_refptr<IndexedDBDatabaseCallbacks> callbacks);
  virtual ~IndexedDBConnection();

  enum class CloseErrorHandling {
    // Returns from the function on the first encounter with an error.
    kReturnOnFirstError,
    // Continues to call Abort() on all transactions despite any errors.
    // The last error encountered is returned.
    kAbortAllReturnLastError,
  };

  leveldb::Status AbortTransactionsAndClose(CloseErrorHandling error_handling);

  leveldb::Status CloseAndReportForceClose();
  bool IsConnected();

  void VersionChangeIgnored();

  virtual void ActivatePendingObservers(
      std::vector<std::unique_ptr<IndexedDBObserver>> pending_observers);
  // Removes observer listed in |remove_observer_ids| from active_observer of
  // connection or pending_observer of transactions associated with this
  // connection.
  virtual void RemoveObservers(const std::vector<int32_t>& remove_observer_ids);

  int32_t id() const { return id_; }

  base::WeakPtr<IndexedDBDatabase> database() const { return database_; }
  IndexedDBDatabaseCallbacks* callbacks() const { return callbacks_.get(); }
  const std::vector<std::unique_ptr<IndexedDBObserver>>& active_observers()
      const {
    return active_observers_;
  }
  base::WeakPtr<IndexedDBConnection> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Creates a transaction for this connection.
  IndexedDBTransaction* CreateTransaction(
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

  base::WeakPtr<IndexedDBTransaction> AddTransactionForTesting(
      std::unique_ptr<IndexedDBTransaction> transaction);

  // We ignore calls where the id doesn't exist to facilitate the AbortAll call.
  // TODO(dmurph): Change that so this doesn't need to ignore unknown ids.
  void RemoveTransaction(int64_t id);

  const base::flat_map<int64_t, std::unique_ptr<IndexedDBTransaction>>&
  transactions() const {
    return transactions_;
  }

 private:
  void ClearStateAfterClose();

  const int32_t id_;

  // Keeps the factory for this origin alive.
  IndexedDBOriginStateHandle origin_state_handle_;
  IndexedDBClassFactory* const indexed_db_class_factory_;

  base::WeakPtr<IndexedDBDatabase> database_;
  base::RepeatingClosure on_version_change_ignored_;
  base::OnceCallback<void(IndexedDBConnection*)> on_close_;

  // The connection owns transactions created on this connection.
  // This is |flat_map| to preserve ordering, and because the vast majority of
  // users have less than 200 transactions.
  base::flat_map<int64_t, std::unique_ptr<IndexedDBTransaction>> transactions_;

  // The callbacks_ member is cleared when the connection is closed.
  // May be nullptr in unit tests.
  scoped_refptr<IndexedDBDatabaseCallbacks> callbacks_;
  std::vector<std::unique_ptr<IndexedDBObserver>> active_observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IndexedDBConnection> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IndexedDBConnection);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_
