// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_connection.h"

#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_observer.h"
#include "content/browser/indexed_db/indexed_db_origin_state.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {

namespace {

static int32_t g_next_indexed_db_connection_id;

}  // namespace

IndexedDBConnection::IndexedDBConnection(
    IndexedDBExecutionContextConnectionTracker::Handle
        execution_context_connection_handle,
    IndexedDBOriginStateHandle origin_state_handle,
    IndexedDBClassFactory* indexed_db_class_factory,
    base::WeakPtr<IndexedDBDatabase> database,
    base::RepeatingClosure on_version_change_ignored,
    base::OnceCallback<void(IndexedDBConnection*)> on_close,
    scoped_refptr<IndexedDBDatabaseCallbacks> callbacks)
    : id_(g_next_indexed_db_connection_id++),
      execution_context_connection_handle_(
          std::move(execution_context_connection_handle)),
      origin_state_handle_(std::move(origin_state_handle)),
      indexed_db_class_factory_(indexed_db_class_factory),
      database_(std::move(database)),
      on_version_change_ignored_(std::move(on_version_change_ignored)),
      on_close_(std::move(on_close)),
      callbacks_(callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!execution_context_connection_handle_.is_null());
}

IndexedDBConnection::~IndexedDBConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected())
    return;

  // TODO(dmurph): Enforce that IndexedDBConnection cannot have any transactions
  // during destruction. This is likely the case during regular execution, but
  // is definitely not the case in unit tests.

  leveldb::Status status =
      AbortTransactionsAndClose(CloseErrorHandling::kAbortAllReturnLastError);
  if (!status.ok())
    origin_state_handle_.origin_state()->tear_down_callback().Run(status);
}

leveldb::Status IndexedDBConnection::AbortTransactionsAndClose(
    CloseErrorHandling error_handling) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected())
    return leveldb::Status::OK();

  DCHECK(database_);
  active_observers_.clear();
  callbacks_ = nullptr;

  // Finish up any transaction, in case there were any running.
  IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                               "Connection is closing.");
  leveldb::Status status;
  switch (error_handling) {
    case CloseErrorHandling::kReturnOnFirstError:
      status = AbortAllTransactions(error);
      break;
    case CloseErrorHandling::kAbortAllReturnLastError:
      status = AbortAllTransactionsAndIgnoreErrors(error);
      break;
  }

  std::move(on_close_).Run(this);
  origin_state_handle_.Release();
  active_observers_.clear();
  return status;
}

leveldb::Status IndexedDBConnection::CloseAndReportForceClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected())
    return leveldb::Status::OK();

  scoped_refptr<IndexedDBDatabaseCallbacks> callbacks(callbacks_);
  leveldb::Status last_error =
      AbortTransactionsAndClose(CloseErrorHandling::kAbortAllReturnLastError);
  callbacks->OnForcedClose();
  return last_error;
}

void IndexedDBConnection::VersionChangeIgnored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_version_change_ignored_.Run();
}

bool IndexedDBConnection::IsConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return callbacks_.get();
}

// The observers begin listening to changes only once they are activated.
void IndexedDBConnection::ActivatePendingObservers(
    std::vector<std::unique_ptr<IndexedDBObserver>> pending_observers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : pending_observers) {
    active_observers_.push_back(std::move(observer));
  }
  pending_observers.clear();
}

void IndexedDBConnection::RemoveObservers(
    const std::vector<int32_t>& observer_ids_to_remove) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<int32_t> pending_observer_ids;
  for (int32_t id_to_remove : observer_ids_to_remove) {
    const auto& it = std::find_if(
        active_observers_.begin(), active_observers_.end(),
        [&id_to_remove](const std::unique_ptr<IndexedDBObserver>& o) {
          return o->id() == id_to_remove;
        });
    if (it != active_observers_.end())
      active_observers_.erase(it);
    else
      pending_observer_ids.push_back(id_to_remove);
  }
  if (pending_observer_ids.empty())
    return;

  for (const auto& it : transactions_) {
    it.second->RemovePendingObservers(pending_observer_ids);
  }
}

IndexedDBTransaction* IndexedDBConnection::CreateTransaction(
    int64_t id,
    const std::set<int64_t>& scope,
    blink::mojom::IDBTransactionMode mode,
    IndexedDBBackingStore::Transaction* backing_store_transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(GetTransaction(id), nullptr) << "Duplicate transaction id." << id;
  IndexedDBOriginState* origin_state = origin_state_handle_.origin_state();
  std::unique_ptr<IndexedDBTransaction> transaction =
      indexed_db_class_factory_->CreateIndexedDBTransaction(
          id, this, scope, mode, database()->tasks_available_callback(),
          origin_state ? origin_state->tear_down_callback()
                       : IndexedDBTransaction::TearDownCallback(),
          backing_store_transaction);
  IndexedDBTransaction* transaction_ptr = transaction.get();
  transactions_[id] = std::move(transaction);
  return transaction_ptr;
}

void IndexedDBConnection::AbortTransactionAndTearDownOnError(
    IndexedDBTransaction* transaction,
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE1("IndexedDBDatabase::Abort(error)", "txn.id", transaction->id());
  leveldb::Status status = transaction->Abort(error);
  if (!status.ok())
    origin_state_handle_.origin_state()->tear_down_callback().Run(status);
}

leveldb::Status IndexedDBConnection::AbortAllTransactions(
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& pair : transactions_) {
    auto& transaction = pair.second;
    if (transaction->state() != IndexedDBTransaction::FINISHED) {
      IDB_TRACE1("IndexedDBDatabase::Abort(error)", "transaction.id",
                 transaction->id());
      leveldb::Status status = transaction->Abort(error);
      if (!status.ok())
        return status;
    }
  }
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBConnection::AbortAllTransactionsAndIgnoreErrors(
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::Status last_error;
  for (const auto& pair : transactions_) {
    auto& transaction = pair.second;
    if (transaction->state() != IndexedDBTransaction::FINISHED) {
      IDB_TRACE1("IndexedDBDatabase::Abort(error)", "transaction.id",
                 transaction->id());
      leveldb::Status status = transaction->Abort(error);
      if (!status.ok())
        last_error = status;
    }
  }
  return last_error;
}

IndexedDBTransaction* IndexedDBConnection::GetTransaction(int64_t id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = transactions_.find(id);
  if (it == transactions_.end())
    return nullptr;
  return it->second.get();
}

base::WeakPtr<IndexedDBTransaction>
IndexedDBConnection::AddTransactionForTesting(
    std::unique_ptr<IndexedDBTransaction> transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(transactions_, transaction->id()));
  base::WeakPtr<IndexedDBTransaction> transaction_ptr =
      transaction->ptr_factory_.GetWeakPtr();
  transactions_[transaction->id()] = std::move(transaction);
  return transaction_ptr;
}

void IndexedDBConnection::RemoveTransaction(int64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transactions_.erase(id);
}

void IndexedDBConnection::ClearStateAfterClose() {
  callbacks_ = nullptr;
  active_observers_.clear();
  origin_state_handle_.Release();
}

}  // namespace content
