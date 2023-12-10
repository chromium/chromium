// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/database_impl.h"

#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_callback_helpers.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/common/features.h"
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

const char kBadTransactionMode[] = "Bad transaction mode";
const char kTransactionAlreadyExists[] = "Transaction already exists";

}  // namespace

// static
mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase>
DatabaseImpl::CreateAndBind(std::unique_ptr<IndexedDBConnection> connection) {
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_remote;
  mojo::MakeSelfOwnedAssociatedReceiver(
      base::WrapUnique(new DatabaseImpl(std::move(connection))),
      pending_remote.InitWithNewEndpointAndPassReceiver());
  return pending_remote;
}

DatabaseImpl::DatabaseImpl(std::unique_ptr<IndexedDBConnection> connection)
    : connection_(std::move(connection)) {
  DCHECK(connection_);
}

DatabaseImpl::~DatabaseImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::Status status;
  if (!connection_->IsConnected()) {
    return;
  }

  connection_->AbortTransactionsAndClose(
      IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);
}

void DatabaseImpl::RenameObjectStore(int64_t transaction_id,
                                     int64_t object_store_id,
                                     const std::u16string& new_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

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
                        connection_->database()->AsWeakPtr(), object_store_id,
                        new_name));
}

void DatabaseImpl::CreateTransaction(
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        transaction_receiver,
    int64_t transaction_id,
    const std::vector<int64_t>& object_store_ids,
    blink::mojom::IDBTransactionMode mode,
    blink::mojom::IDBTransactionDurability durability) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  if (mode != blink::mojom::IDBTransactionMode::ReadOnly &&
      mode != blink::mojom::IDBTransactionMode::ReadWrite) {
    mojo::ReportBadMessage(kBadTransactionMode);
    return;
  }

  if (connection_->GetTransaction(transaction_id)) {
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

  IndexedDBTransaction* transaction = connection_->CreateTransaction(
      std::move(transaction_receiver), transaction_id,
      std::set<int64_t>(object_store_ids.begin(), object_store_ids.end()), mode,
      connection_->database()
          ->backing_store()
          ->CreateTransaction(durability, mode)
          .release());
  connection_->database()->RegisterAndScheduleTransaction(transaction);
}

void DatabaseImpl::VersionChangeIgnored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  connection_->VersionChangeIgnored();
}

void DatabaseImpl::Get(int64_t transaction_id,
                       int64_t object_store_id,
                       int64_t index_id,
                       const IndexedDBKeyRange& key_range,
                       bool key_only,
                       blink::mojom::IDBDatabase::GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected()) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Not connected.");
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
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
      &IndexedDBDatabase::GetOperation, connection_->database()->AsWeakPtr(),
      object_store_id, index_id, std::make_unique<IndexedDBKeyRange>(key_range),
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE,
      std::move(aborting_callback)));
}

void DatabaseImpl::GetAll(int64_t transaction_id,
                          int64_t object_store_id,
                          int64_t index_id,
                          const IndexedDBKeyRange& key_range,
                          bool key_only,
                          int64_t max_count,
                          blink::mojom::IDBDatabase::GetAllCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!connection_->IsConnected()) {
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

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
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
      &IndexedDBDatabase::GetAllOperation, connection_->database()->AsWeakPtr(),
      object_store_id, index_id, std::make_unique<IndexedDBKeyRange>(key_range),
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE,
      max_count, std::move(aborting_callback)));
}

void DatabaseImpl::SetIndexKeys(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKey& primary_key,
    const std::vector<IndexedDBIndexKeys>& index_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

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
      BindWeakOperation(&IndexedDBDatabase::SetIndexKeysOperation,
                        connection_->database()->AsWeakPtr(), object_store_id,
                        std::make_unique<IndexedDBKey>(primary_key),
                        index_keys));
}

void DatabaseImpl::SetIndexesReady(int64_t transaction_id,
                                   int64_t object_store_id,
                                   const std::vector<int64_t>& index_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

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
      BindWeakOperation(&IndexedDBDatabase::SetIndexesReadyOperation,
                        connection_->database()->AsWeakPtr(),
                        index_ids.size()));
}

void DatabaseImpl::OpenCursor(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction,
    bool key_only,
    blink::mojom::IDBTaskType task_type,
    blink::mojom::IDBDatabase::OpenCursorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected()) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Not connected.");
    std::move(callback).Run(
        blink::mojom::IDBDatabaseOpenCursorResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
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
  params->cursor_type =
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE;
  params->task_type = task_type;
  params->callback = std::move(aborting_callback);
  transaction->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::OpenCursorOperation,
                        connection_->database()->AsWeakPtr(), std::move(params),
                        GetBucketLocator()));
}

void DatabaseImpl::Count(int64_t transaction_id,
                         int64_t object_store_id,
                         int64_t index_id,
                         const IndexedDBKeyRange& key_range,
                         CountCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), /*success=*/false, 0);

  if (!connection_->IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction || !transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::CountOperation, connection_->database()->AsWeakPtr(),
      object_store_id, index_id,
      std::make_unique<blink::IndexedDBKeyRange>(key_range),
      std::move(wrapped_callback)));
}

void DatabaseImpl::DeleteRange(int64_t transaction_id,
                               int64_t object_store_id,
                               const IndexedDBKeyRange& key_range,
                               DeleteRangeCallback success_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(success_callback), /*success=*/false);

  if (!connection_->IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
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

  transaction->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::DeleteRangeOperation,
                        connection_->database()->AsWeakPtr(), object_store_id,
                        std::make_unique<IndexedDBKeyRange>(key_range),
                        std::move(wrapped_callback)));
}

void DatabaseImpl::GetKeyGeneratorCurrentNumber(
    int64_t transaction_id,
    int64_t object_store_id,
    GetKeyGeneratorCurrentNumberCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), -1,
      blink::mojom::IDBError::New(
          blink::mojom::IDBException::kIgnorableAbortError,
          u"Aborting due to unknown failure."));

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (!transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::GetKeyGeneratorCurrentNumberOperation,
      connection_->database()->AsWeakPtr(), object_store_id,
      std::move(wrapped_callback)));
}

void DatabaseImpl::Clear(int64_t transaction_id,
                         int64_t object_store_id,
                         ClearCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), /*success=*/false);

  if (!connection_->IsConnected()) {
    return;
  }

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction || !transaction->IsAcceptingRequests()) {
    // TODO(https://crbug.com/1249908): If the transaction was already committed
    // (or is in the process of being committed) we should kill the renderer.
    // This branch however also includes cases where the browser process aborted
    // the transaction, as currently we don't distinguish that state from the
    // transaction having been committed. So for now simply ignore the request.
    return;
  }

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::ClearOperation, connection_->database()->AsWeakPtr(),
      object_store_id, std::move(wrapped_callback)));
}

void DatabaseImpl::CreateIndex(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               const std::u16string& name,
                               const IndexedDBKeyPath& key_path,
                               bool unique,
                               bool multi_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

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
      BindWeakOperation(&IndexedDBDatabase::CreateIndexOperation,
                        connection_->database()->AsWeakPtr(), object_store_id,
                        index_id, name, key_path, unique, multi_entry));
}

void DatabaseImpl::DeleteIndex(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

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

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::DeleteIndexOperation,
      connection_->database()->AsWeakPtr(), object_store_id, index_id));
}

void DatabaseImpl::RenameIndex(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               const std::u16string& new_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

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
      BindWeakOperation(&IndexedDBDatabase::RenameIndexOperation,
                        connection_->database()->AsWeakPtr(), object_store_id,
                        index_id, new_name));
}

void DatabaseImpl::Abort(int64_t transaction_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->AbortTransactionAndTearDownOnError(
      transaction,
      IndexedDBDatabaseError(blink::mojom::IDBException::kAbortError,
                             "Transaction aborted by user."));
}

void DatabaseImpl::DidBecomeInactive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected()) {
    return;
  }

  for (const auto& [_, transaction] : connection_->transactions()) {
    switch (transaction->state()) {
      case IndexedDBTransaction::State::CREATED: {
        // The transaction is created but not started yet, which means it may be
        // blocked by others and waiting for the lock to be acquired. We should
        // disallow the activation for the client.
        connection_->DisallowInactiveClient(
            storage::mojom::DisallowInactiveClientReason::
                kTransactionIsAcquiringLocks,
            base::DoNothing());
        return;
      }
      case IndexedDBTransaction::State::STARTED: {
        if (connection_->database()->IsTransactionBlockingOthers(
                transaction.get())) {
          // The transaction is holding the locks while others are waiting for
          // the acquisition. We should disallow the activation for this client
          // so the lock is immediately available.
          connection_->DisallowInactiveClient(
              storage::mojom::DisallowInactiveClientReason::
                  kTransactionIsBlockingOthers,
              base::DoNothing());
          return;
        }
        break;
      }
      case IndexedDBTransaction::State::COMMITTING:
      case IndexedDBTransaction::State::FINISHED:
        break;
    }
  }
}

const storage::BucketInfo& DatabaseImpl::GetBucketInfo() {
  CHECK(connection_->bucket_context());
  return connection_->bucket_context()->bucket_info();
}

storage::BucketLocator DatabaseImpl::GetBucketLocator() {
  CHECK(connection_->bucket_context());
  return connection_->bucket_context()->bucket_locator();
}

}  // namespace content
