// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_connection.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/trace_event/base_tracing.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom-forward.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {

namespace {

static int32_t g_next_indexed_db_connection_id;

}  // namespace

IndexedDBConnection::IndexedDBConnection(
    IndexedDBBucketContext& bucket_context,
    base::WeakPtr<IndexedDBDatabase> database,
    base::RepeatingClosure on_version_change_ignored,
    base::OnceCallback<void(IndexedDBConnection*)> on_close,
    scoped_refptr<IndexedDBDatabaseCallbacks> callbacks,
    scoped_refptr<IndexedDBClientStateCheckerWrapper> client_state_checker)
    : id_(g_next_indexed_db_connection_id++),
      bucket_context_handle_(bucket_context),
      database_(std::move(database)),
      on_version_change_ignored_(std::move(on_version_change_ignored)),
      on_close_(std::move(on_close)),
      callbacks_(callbacks),
      client_state_checker_(std::move(client_state_checker)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bucket_context_handle_->quota_manager()->NotifyBucketAccessed(
      bucket_context_handle_->bucket_locator(), base::Time::Now());
}

IndexedDBConnection::~IndexedDBConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected())
    return;

  // TODO(dmurph): Enforce that IndexedDBConnection cannot have any transactions
  // during destruction. This is likely the case during regular execution, but
  // is definitely not the case in unit tests.

  AbortTransactionsAndClose(CloseErrorHandling::kAbortAllReturnLastError);
}

void IndexedDBConnection::AbortTransactionsAndClose(
    CloseErrorHandling error_handling) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected())
    return;

  DCHECK(database_);
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
  client_keep_active_remotes_.Clear();
  bucket_context_handle_->quota_manager()->NotifyBucketAccessed(
      bucket_context_handle_->bucket_locator(), base::Time::Now());
  if (!status.ok()) {
    bucket_context_handle_->delegate().on_fatal_error.Run(status);
  }
  bucket_context_handle_.Release();
}

void IndexedDBConnection::CloseAndReportForceClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected())
    return;

  scoped_refptr<IndexedDBDatabaseCallbacks> callbacks(callbacks_);
  AbortTransactionsAndClose(CloseErrorHandling::kAbortAllReturnLastError);
  callbacks->OnForcedClose();
}

void IndexedDBConnection::VersionChangeIgnored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_version_change_ignored_.Run();
}

bool IndexedDBConnection::IsConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return callbacks_.get();
}

IndexedDBTransaction* IndexedDBConnection::CreateTransaction(
    int64_t id,
    const std::set<int64_t>& scope,
    blink::mojom::IDBTransactionMode mode,
    IndexedDBBackingStore::Transaction* backing_store_transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(GetTransaction(id), nullptr) << "Duplicate transaction id." << id;
  auto transaction = std::make_unique<IndexedDBTransaction>(
      id, this, scope, mode, bucket_context_handle_, backing_store_transaction);
  IndexedDBTransaction* transaction_ptr = transaction.get();
  transactions_[id] = std::move(transaction);
  return transaction_ptr;
}

void IndexedDBConnection::AbortTransactionAndTearDownOnError(
    IndexedDBTransaction* transaction,
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::Abort(error)", "txn.id",
               transaction->id());
  leveldb::Status status = transaction->Abort(error);
  if (!status.ok())
    bucket_context_handle_->delegate().on_fatal_error.Run(status);
}

leveldb::Status IndexedDBConnection::AbortAllTransactions(
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& pair : transactions_) {
    auto& transaction = pair.second;
    if (transaction->state() != IndexedDBTransaction::FINISHED) {
      TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::Abort(error)",
                   "transaction.id", transaction->id());
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
      TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::Abort(error)",
                   "transaction.id", transaction->id());
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

void IndexedDBConnection::RemoveTransaction(int64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transactions_.erase(id);
}

void IndexedDBConnection::DisallowInactiveClient(
    storage::mojom::DisallowInactiveClientReason reason,
    base::OnceCallback<void(bool)> callback) {
  if (reason ==
      storage::mojom::DisallowInactiveClientReason::kClientEventIsTriggered) {
    // It's only necessary to keep the client active under this scenario.
    mojo::Remote<storage::mojom::IndexedDBClientKeepActive>
        client_keep_active_remote;
    client_state_checker_->DisallowInactiveClient(
        reason, client_keep_active_remote.BindNewPipeAndPassReceiver(),
        std::move(callback));
    client_keep_active_remotes_.Add(std::move(client_keep_active_remote));
  } else {
    client_state_checker_->DisallowInactiveClient(reason, mojo::NullReceiver(),
                                                  std::move(callback));
  }
}

}  // namespace content
