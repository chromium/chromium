// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/database_impl.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/indexed_db/indexed_db_callback_helpers.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/transaction_impl.h"
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

DatabaseImpl::DatabaseImpl(std::unique_ptr<IndexedDBConnection> connection,
                           const storage::BucketInfo& bucket,
                           IndexedDBDispatcherHost* dispatcher_host,
                           scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : dispatcher_host_(dispatcher_host),
      indexed_db_context_(dispatcher_host->context()),
      connection_(std::move(connection)),
      bucket_info_(bucket),
      idb_runner_(std::move(idb_runner)) {
  DCHECK(idb_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(connection_);
  indexed_db_context_->ConnectionOpened(bucket_locator());
}

DatabaseImpl::~DatabaseImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::Status status;
  if (connection_->IsConnected()) {
    status = connection_->AbortTransactionsAndClose(
        IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);
  }
  indexed_db_context_->ConnectionClosed(bucket_locator());
  if (!status.ok()) {
    indexed_db_context_->GetIDBFactory()->OnDatabaseError(
        bucket_locator(), status, "Error during rollbacks.");
  }
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
    switch (bucket_info_.durability) {
      case blink::mojom::BucketDurability::kStrict:
        durability = blink::mojom::IDBTransactionDurability::Strict;
        break;
      case blink::mojom::BucketDurability::kRelaxed:
        durability = blink::mojom::IDBTransactionDurability::Relaxed;
        break;
    }
  }

  IndexedDBTransaction* transaction = connection_->CreateTransaction(
      transaction_id,
      std::set<int64_t>(object_store_ids.begin(), object_store_ids.end()), mode,
      connection_->database()
          ->backing_store()
          ->CreateTransaction(durability, mode)
          .release());
  connection_->database()->RegisterAndScheduleTransaction(transaction);

  dispatcher_host_->CreateAndBindTransactionImpl(
      std::move(transaction_receiver), bucket_locator(),
      transaction->AsWeakPtr());
}

void DatabaseImpl::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  leveldb::Status status = connection_->AbortTransactionsAndClose(
      IndexedDBConnection::CloseErrorHandling::kReturnOnFirstError);

  if (!status.ok()) {
    indexed_db_context_->GetIDBFactory()->OnDatabaseError(
        bucket_locator(), status, "Error during rollbacks.");
  }
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
      dispatcher_host_->AsWeakPtr(), object_store_id, index_id,
      std::make_unique<IndexedDBKeyRange>(key_range),
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
      dispatcher_host_->AsWeakPtr(), object_store_id, index_id,
      std::make_unique<IndexedDBKeyRange>(key_range),
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE,
      max_count, std::move(aborting_callback)));
}

void DatabaseImpl::BatchGetAll(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const std::vector<blink::IndexedDBKeyRange>& key_ranges,
    uint32_t max_count,
    blink::mojom::IDBDatabase::BatchGetAllCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!connection_->IsConnected()) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Not connected.");
    std::move(callback).Run(
        blink::mojom::IDBDatabaseBatchGetAllResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Unknown transaction.");
    std::move(callback).Run(
        blink::mojom::IDBDatabaseBatchGetAllResult::NewErrorResult(
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

  if (key_ranges.size() > blink::mojom::kIDBBatchGetAllMaxInputSize) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "key_ranges array's size is too large.");
    std::move(callback).Run(
        blink::mojom::IDBDatabaseBatchGetAllResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  blink::mojom::IDBDatabase::BatchGetAllCallback aborting_callback =
      CreateCallbackAbortOnDestruct<
          blink::mojom::IDBDatabase::BatchGetAllCallback,
          blink::mojom::IDBDatabaseBatchGetAllResultPtr>(
          std::move(callback), transaction->AsWeakPtr());

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::BatchGetAllOperation,
      connection_->database()->AsWeakPtr(), dispatcher_host_->AsWeakPtr(),
      object_store_id, index_id, key_ranges, max_count,
      std::move(aborting_callback)));
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
                        bucket_locator(), dispatcher_host_->AsWeakPtr()));
}

void DatabaseImpl::Count(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
        pending_callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto callbacks = base::MakeRefCounted<IndexedDBCallbacks>(
      dispatcher_host_->AsWeakPtr(), bucket_info_, std::move(pending_callbacks),
      idb_runner_);
  if (!connection_->IsConnected())
    return;

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
      &IndexedDBDatabase::CountOperation, connection_->database()->AsWeakPtr(),
      object_store_id, index_id,
      std::make_unique<blink::IndexedDBKeyRange>(key_range),
      std::move(callbacks)));
}

void DatabaseImpl::DeleteRange(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range,
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
        pending_callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto callbacks = base::MakeRefCounted<IndexedDBCallbacks>(
      dispatcher_host_->AsWeakPtr(), bucket_info_, std::move(pending_callbacks),
      idb_runner_);
  if (!connection_->IsConnected())
    return;

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
      &IndexedDBDatabase::DeleteRangeOperation,
      connection_->database()->AsWeakPtr(), object_store_id,
      std::make_unique<IndexedDBKeyRange>(key_range), std::move(callbacks)));
}

void DatabaseImpl::GetKeyGeneratorCurrentNumber(
    int64_t transaction_id,
    int64_t object_store_id,
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
        pending_callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto callbacks = base::MakeRefCounted<IndexedDBCallbacks>(
      dispatcher_host_->AsWeakPtr(), bucket_info_, std::move(pending_callbacks),
      idb_runner_);
  if (!connection_->IsConnected())
    return;

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
      std::move(callbacks)));
}

void DatabaseImpl::Clear(
    int64_t transaction_id,
    int64_t object_store_id,
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
        pending_callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto callbacks = base::MakeRefCounted<IndexedDBCallbacks>(
      dispatcher_host_->AsWeakPtr(), bucket_info_, std::move(pending_callbacks),
      idb_runner_);
  if (!connection_->IsConnected())
    return;

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
      &IndexedDBDatabase::ClearOperation, connection_->database()->AsWeakPtr(),
      object_store_id, std::move(callbacks)));
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

}  // namespace content
