// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_connection.h"

#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/trace_event/base_tracing.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "content/browser/indexed_db/indexed_db_callback_helpers.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

using blink::IndexedDBIndexKeys;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBKeyRange;
using std::swap;

namespace blink {
class IndexedDBKeyRange;
}

namespace content {
namespace {

static int32_t g_next_indexed_db_connection_id;

const char kBadTransactionMode[] = "Bad transaction mode";
const char kTransactionAlreadyExists[] = "Transaction already exists";

}  // namespace

// static
mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase>
IndexedDBConnection::MakeSelfOwnedReceiverAndBindRemote(
    std::unique_ptr<IndexedDBConnection> connection) {
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_remote;
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::move(connection),
      pending_remote.InitWithNewEndpointAndPassReceiver());
  return pending_remote;
}

IndexedDBConnection::IndexedDBConnection(
    IndexedDBBucketContext& bucket_context,
    base::WeakPtr<IndexedDBDatabase> database,
    base::RepeatingClosure on_version_change_ignored,
    base::OnceCallback<void(IndexedDBConnection*)> on_close,
    std::unique_ptr<IndexedDBDatabaseCallbacks> callbacks,
    mojo::Remote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker,
    base::UnguessableToken client_token)
    : id_(g_next_indexed_db_connection_id++),
      bucket_context_handle_(bucket_context),
      database_(std::move(database)),
      on_version_change_ignored_(std::move(on_version_change_ignored)),
      on_close_(std::move(on_close)),
      callbacks_(std::move(callbacks)),
      client_state_checker_(std::move(client_state_checker)),
      client_token_(client_token) {
  bucket_context_handle_->quota_manager()->NotifyBucketAccessed(
      bucket_context_handle_->bucket_locator(), base::Time::Now());
}

IndexedDBConnection::~IndexedDBConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::Status status;
  if (!IsConnected()) {
    return;
  }

  AbortTransactionsAndClose(CloseErrorHandling::kAbortAllReturnLastError);
}

bool IndexedDBConnection::IsConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return callbacks_.get();
}

IndexedDBTransaction* IndexedDBConnection::CreateVersionChangeTransaction(
    int64_t id,
    const std::set<int64_t>& scope,
    IndexedDBBackingStore::Transaction* backing_store_transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(GetTransaction(id), nullptr) << "Duplicate transaction id." << id;
  return (transactions_[id] = std::make_unique<IndexedDBTransaction>(
              id, this, scope, blink::mojom::IDBTransactionMode::VersionChange,
              bucket_context_handle_, backing_store_transaction))
      .get();
}

void IndexedDBConnection::DisallowInactiveClient(
    storage::mojom::DisallowInactiveClientReason reason,
    base::OnceCallback<void(bool)> callback) {
  if (!client_state_checker_.is_bound()) {
    // If the remote is no longer connected, we expect the client will terminate
    // the connection soon, so marking `was_active` true here.
    std::move(callback).Run(/*was_active=*/true);
    return;
  }

  if (reason ==
      storage::mojom::DisallowInactiveClientReason::kVersionChangeEvent) {
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

void IndexedDBConnection::RemoveTransaction(int64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transactions_.erase(id);
}

void IndexedDBConnection::AbortTransactionAndTearDownOnError(
    IndexedDBTransaction* transaction,
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::Abort(error)", "txn.id",
               transaction->id());
  leveldb::Status status = transaction->Abort(error);
  if (!status.ok()) {
    bucket_context_handle_->OnDatabaseError(status, {});
  }
}

void IndexedDBConnection::CloseAndReportForceClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    return;
  }

  AbortTransactionsAndClose(CloseErrorHandling::kAbortAllReturnLastError)
      ->OnForcedClose();
}

void IndexedDBConnection::RenameObjectStore(int64_t transaction_id,
                                            int64_t object_store_id,
                                            const std::u16string& new_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction) {
    return;
  }

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "RenameObjectStore must be called from a version change transaction.");
    return;
  }

  if (!transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      BindWeakOperation(&IndexedDBDatabase::RenameObjectStoreOperation,
                        database_, object_store_id, new_name));
}

void IndexedDBConnection::CreateTransaction(
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        transaction_receiver,
    int64_t transaction_id,
    const std::vector<int64_t>& object_store_ids,
    blink::mojom::IDBTransactionMode mode,
    blink::mojom::IDBTransactionDurability durability) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    return;
  }

  if (mode != blink::mojom::IDBTransactionMode::ReadOnly &&
      mode != blink::mojom::IDBTransactionMode::ReadWrite) {
    mojo::ReportBadMessage(kBadTransactionMode);
    return;
  }

  if (GetTransaction(transaction_id)) {
    mojo::ReportBadMessage(kTransactionAlreadyExists);
    return;
  }

  if (durability == blink::mojom::IDBTransactionDurability::Default) {
    switch (GetBucketInfo().durability) {
      case blink::mojom::BucketDurability::kStrict:
        durability = blink::mojom::IDBTransactionDurability::Strict;
        break;
      case blink::mojom::BucketDurability::kRelaxed:
        durability = blink::mojom::IDBTransactionDurability::Relaxed;
        break;
    }
  }

  std::set<int64_t> scope(object_store_ids.begin(), object_store_ids.end());
  IndexedDBBackingStore::Transaction* backing_store_transaction =
      database_->backing_store()->CreateTransaction(durability, mode).release();
  IndexedDBTransaction* transaction =
      (transactions_[transaction_id] = std::make_unique<IndexedDBTransaction>(
           transaction_id, this, std::move(scope), mode, bucket_context_handle_,
           backing_store_transaction))
          .get();

  transaction->BindReceiver(std::move(transaction_receiver));
  database_->RegisterAndScheduleTransaction(transaction);
}

void IndexedDBConnection::VersionChangeIgnored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    return;
  }

  on_version_change_ignored_.Run();
}

void IndexedDBConnection::Get(int64_t transaction_id,
                              int64_t object_store_id,
                              int64_t index_id,
                              const IndexedDBKeyRange& key_range,
                              bool key_only,
                              blink::mojom::IDBDatabase::GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Not connected.");
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Unknown transaction.");
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  if (!transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  blink::mojom::IDBDatabase::GetCallback aborting_callback =
      CreateCallbackAbortOnDestruct<blink::mojom::IDBDatabase::GetCallback,
                                    blink::mojom::IDBDatabaseGetResultPtr>(
          std::move(callback), transaction->AsWeakPtr());

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::GetOperation, database_, object_store_id, index_id,
      std::make_unique<IndexedDBKeyRange>(key_range),
      key_only ? indexed_db::CursorType::kKeyOnly
               : indexed_db::CursorType::kKeyAndValue,
      std::move(aborting_callback)));
}

void IndexedDBConnection::GetAll(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    bool key_only,
    int64_t max_count,
    blink::mojom::IDBDatabase::GetAllCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsConnected()) {
    // TODO(enne): see note below.  It can be incorrect for result ordering to
    // run the callback directly from this function.
    mojo::Remote<blink::mojom::IDBDatabaseGetAllResultSink> result_sink;
    auto receiver = result_sink.BindNewPipeAndPassReceiver();
    std::move(callback).Run(std::move(receiver));

    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Not connected.");
    result_sink->OnError(
        blink::mojom::IDBError::New(error.code(), error.message()));
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction) {
    mojo::Remote<blink::mojom::IDBDatabaseGetAllResultSink> result_sink;
    auto receiver = result_sink.BindNewPipeAndPassReceiver();
    std::move(callback).Run(std::move(receiver));

    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Unknown transaction.");
    result_sink->OnError(
        blink::mojom::IDBError::New(error.code(), error.message()));
    return;
  }

  if (!transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  // Hypothetically, this could pass the receiver to the callback immediately.
  // However, for result ordering issues, we need to PostTask to mimic
  // all of the other operations.
  // TODO(enne): Consider rewriting the renderer side to order results based
  // on initial request ordering and not on when the results are returned.
  blink::mojom::IDBDatabase::GetAllCallback aborting_callback =
      CreateCallbackAbortOnDestruct<
          blink::mojom::IDBDatabase::GetAllCallback,
          mojo::PendingReceiver<blink::mojom::IDBDatabaseGetAllResultSink>>(
          std::move(callback), transaction->AsWeakPtr());

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::GetAllOperation, database_, object_store_id, index_id,
      std::make_unique<IndexedDBKeyRange>(key_range),
      key_only ? indexed_db::CursorType::kKeyOnly
               : indexed_db::CursorType::kKeyAndValue,
      max_count, std::move(aborting_callback)));
}

void IndexedDBConnection::SetIndexKeys(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKey& primary_key,
    const std::vector<IndexedDBIndexKeys>& index_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction) {
    return;
  }

  if (!primary_key.IsValid()) {
    mojo::ReportBadMessage("SetIndexKeys used with invalid key.");
    return;
  }

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "SetIndexKeys must be called from a version change transaction.");
    return;
  }

  if (!transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      BindWeakOperation(
          &IndexedDBDatabase::SetIndexKeysOperation, database_, object_store_id,
          std::make_unique<IndexedDBKey>(primary_key), index_keys));
}

void IndexedDBConnection::SetIndexesReady(
    int64_t transaction_id,
    int64_t object_store_id,
    const std::vector<int64_t>& index_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction) {
    return;
  }

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "SetIndexesReady must be called from a version change transaction.");
    return;
  }

  if (!transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      BindWeakOperation(&IndexedDBDatabase::SetIndexesReadyOperation, database_,
                        index_ids.size()));
}

void IndexedDBConnection::OpenCursor(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction,
    bool key_only,
    blink::mojom::IDBTaskType task_type,
    blink::mojom::IDBDatabase::OpenCursorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Not connected.");
    std::move(callback).Run(
        blink::mojom::IDBDatabaseOpenCursorResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Unknown transaction.");
    std::move(callback).Run(
        blink::mojom::IDBDatabaseOpenCursorResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  if (!transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  blink::mojom::IDBDatabase::OpenCursorCallback aborting_callback =
      CreateCallbackAbortOnDestruct<
          blink::mojom::IDBDatabase::OpenCursorCallback,
          blink::mojom::IDBDatabaseOpenCursorResultPtr>(
          std::move(callback), transaction->AsWeakPtr());

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange &&
      task_type == blink::mojom::IDBTaskType::Preemptive) {
    mojo::ReportBadMessage(
        "OpenCursor with |Preemptive| task type must be called from a version "
        "change transaction.");
    return;
  }

  std::unique_ptr<IndexedDBDatabase::OpenCursorOperationParams> params(
      std::make_unique<IndexedDBDatabase::OpenCursorOperationParams>());
  params->object_store_id = object_store_id;
  params->index_id = index_id;
  params->key_range = std::make_unique<IndexedDBKeyRange>(key_range);
  params->direction = direction;
  params->cursor_type = key_only ? indexed_db::CursorType::kKeyOnly
                                 : indexed_db::CursorType::kKeyAndValue;
  params->task_type = task_type;
  params->callback = std::move(aborting_callback);
  transaction->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::OpenCursorOperation, database_,
                        std::move(params), GetBucketLocator()));
}

void IndexedDBConnection::Count(int64_t transaction_id,
                                int64_t object_store_id,
                                int64_t index_id,
                                const IndexedDBKeyRange& key_range,
                                CountCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), /*success=*/false, 0);

  if (!IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction || !transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::CountOperation, database_, object_store_id, index_id,
      std::make_unique<blink::IndexedDBKeyRange>(key_range),
      std::move(wrapped_callback)));
}

void IndexedDBConnection::DeleteRange(int64_t transaction_id,
                                      int64_t object_store_id,
                                      const IndexedDBKeyRange& key_range,
                                      DeleteRangeCallback success_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(success_callback), /*success=*/false);

  if (!IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction) {
    return;
  }

  if (!transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::DeleteRangeOperation, database_, object_store_id,
      std::make_unique<IndexedDBKeyRange>(key_range),
      std::move(wrapped_callback)));
}

void IndexedDBConnection::GetKeyGeneratorCurrentNumber(
    int64_t transaction_id,
    int64_t object_store_id,
    GetKeyGeneratorCurrentNumberCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), -1,
      blink::mojom::IDBError::New(
          blink::mojom::IDBException::kIgnorableAbortError,
          u"Aborting due to unknown failure."));

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction) {
    return;
  }

  if (!transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::GetKeyGeneratorCurrentNumberOperation, database_,
      object_store_id, std::move(wrapped_callback)));
}

void IndexedDBConnection::Clear(int64_t transaction_id,
                                int64_t object_store_id,
                                ClearCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), /*success=*/false);

  if (!IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction || !transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::ClearOperation, database_,
                        object_store_id, std::move(wrapped_callback)));
}

void IndexedDBConnection::CreateIndex(int64_t transaction_id,
                                      int64_t object_store_id,
                                      int64_t index_id,
                                      const std::u16string& name,
                                      const IndexedDBKeyPath& key_path,
                                      bool unique,
                                      bool multi_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction) {
    return;
  }

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "CreateIndex must be called from a version change transaction.");
    return;
  }

  if (!transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      BindWeakOperation(&IndexedDBDatabase::CreateIndexOperation, database_,
                        object_store_id, index_id, name, key_path, unique,
                        multi_entry));
}

void IndexedDBConnection::DeleteIndex(int64_t transaction_id,
                                      int64_t object_store_id,
                                      int64_t index_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction) {
    return;
  }

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "DeleteIndex must be called from a version change transaction.");
    return;
  }

  if (!transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::DeleteIndexOperation, database_,
                        object_store_id, index_id));
}

void IndexedDBConnection::RenameIndex(int64_t transaction_id,
                                      int64_t object_store_id,
                                      int64_t index_id,
                                      const std::u16string& new_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction) {
    return;
  }

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "RenameIndex must be called from a version change transaction.");
    return;
  }

  if (!transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::RenameIndexOperation, database_,
                        object_store_id, index_id, new_name));
}

void IndexedDBConnection::Abort(int64_t transaction_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction) {
    return;
  }

  AbortTransactionAndTearDownOnError(
      transaction,
      IndexedDBDatabaseError(blink::mojom::IDBException::kAbortError,
                             "Transaction aborted by user."));
}

void IndexedDBConnection::DidBecomeInactive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    return;
  }

  for (const auto& [_, transaction] : transactions_) {
    // If the transaction is holding the locks while others are waiting for
    // the acquisition, we should disallow the activation for this client
    // so the lock is immediately available.
    transaction->DontAllowInactiveClientToBlockOthers(
        storage::mojom::DisallowInactiveClientReason::
            kTransactionIsOngoingAndBlockingOthers);
  }
}

const storage::BucketInfo& IndexedDBConnection::GetBucketInfo() {
  CHECK(bucket_context());
  return bucket_context()->bucket_info();
}

storage::BucketLocator IndexedDBConnection::GetBucketLocator() {
  CHECK(bucket_context());
  return bucket_context()->bucket_locator();
}

IndexedDBTransaction* IndexedDBConnection::GetTransaction(int64_t id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = transactions_.find(id);
  if (it == transactions_.end()) {
    return nullptr;
  }
  return it->second.get();
}

std::unique_ptr<IndexedDBDatabaseCallbacks>
IndexedDBConnection::AbortTransactionsAndClose(
    CloseErrorHandling error_handling) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected()) {
    return {};
  }

  DCHECK(database_);

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

  std::unique_ptr<IndexedDBDatabaseCallbacks> callbacks = std::move(callbacks_);
  std::move(on_close_).Run(this);
  client_keep_active_remotes_.Clear();
  bucket_context_handle_->quota_manager()->NotifyBucketAccessed(
      bucket_context_handle_->bucket_locator(), base::Time::Now());
  if (!status.ok()) {
    bucket_context_handle_->OnDatabaseError(status, {});
  }
  bucket_context_handle_.Release();
  return callbacks;
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
      if (!status.ok()) {
        last_error = status;
      }
    }
  }
  return last_error;
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
      if (!status.ok()) {
        return status;
      }
    }
  }
  return leveldb::Status::OK();
}

}  // namespace content
