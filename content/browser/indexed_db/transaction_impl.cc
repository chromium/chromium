// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/transaction_impl.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "content/browser/indexed_db/indexed_db_callback_helpers.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {

TransactionImpl::TransactionImpl(
    base::WeakPtr<IndexedDBTransaction> transaction,
    const url::Origin& origin,
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : dispatcher_host_(dispatcher_host),
      indexed_db_context_(dispatcher_host->context()),
      transaction_(std::move(transaction)),
      origin_(origin),
      idb_runner_(std::move(idb_runner)) {
  DCHECK(idb_runner_->RunsTasksInCurrentSequence());
  DCHECK(dispatcher_host_);
  DCHECK(transaction_);
}

TransactionImpl::~TransactionImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TransactionImpl::CreateObjectStore(int64_t object_store_id,
                                        const base::string16& name,
                                        const blink::IndexedDBKeyPath& key_path,
                                        bool auto_increment) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!transaction_)
    return;

  if (transaction_->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "CreateObjectStore must be called from a version change transaction.");
    return;
  }

  if (!transaction_->IsAcceptingRequests()) {
    mojo::ReportBadMessage(
        "CreateObjectStore was called after committing or aborting the "
        "transaction");
    return;
  }

  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected())
    return;

  transaction_->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      BindWeakOperation(&IndexedDBDatabase::CreateObjectStoreOperation,
                        connection->database()->AsWeakPtr(), object_store_id,
                        name, key_path, auto_increment));
}

void TransactionImpl::DeleteObjectStore(int64_t object_store_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!transaction_)
    return;

  if (transaction_->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "DeleteObjectStore must be called from a version change transaction.");
    return;
  }

  if (!transaction_->IsAcceptingRequests()) {
    mojo::ReportBadMessage(
        "DeleteObjectStore was called after committing or aborting the "
        "transaction");
    return;
  }

  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected())
    return;

  transaction_->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::DeleteObjectStoreOperation,
                        connection->database()->AsWeakPtr(), object_store_id));
}

void TransactionImpl::Put(
    int64_t object_store_id,
    blink::mojom::IDBValuePtr input_value,
    const blink::IndexedDBKey& key,
    blink::mojom::IDBPutMode mode,
    const std::vector<blink::IndexedDBIndexKeys>& index_keys,
    blink::mojom::IDBTransaction::PutCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(dispatcher_host_);

  std::vector<IndexedDBExternalObject> external_objects;
  if (!input_value->external_objects.empty())
    CreateExternalObjects(input_value, &external_objects);

  if (!transaction_) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Unknown transaction.");
    std::move(callback).Run(
        blink::mojom::IDBTransactionPutResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  if (!transaction_->IsAcceptingRequests()) {
    mojo::ReportBadMessage(
        "Put was called after committing or aborting the transaction");
    return;
  }

  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected()) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Not connected.");
    std::move(callback).Run(
        blink::mojom::IDBTransactionPutResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  uint64_t commit_size = input_value->bits.size() + key.size_estimate();
  std::unique_ptr<IndexedDBDatabase::PutOperationParams> params(
      std::make_unique<IndexedDBDatabase::PutOperationParams>());
  IndexedDBValue& output_value = params->value;

  // TODO(crbug.com/902498): Use mojom traits to map directly to
  // std::string.
  output_value.bits =
      std::string(input_value->bits.begin(), input_value->bits.end());
  // Release value->bits std::vector.
  input_value->bits.clear();
  swap(output_value.external_objects, external_objects);

  blink::mojom::IDBTransaction::PutCallback aborting_callback =
      CreateCallbackAbortOnDestruct<blink::mojom::IDBTransaction::PutCallback,
                                    blink::mojom::IDBTransactionPutResultPtr>(
          std::move(callback), transaction_->AsWeakPtr());

  params->object_store_id = object_store_id;
  params->key = std::make_unique<blink::IndexedDBKey>(key);
  params->put_mode = mode;
  params->callback = std::move(aborting_callback);
  params->index_keys = index_keys;
  // This is decremented in IndexedDBDatabase::PutOperation.
  transaction_->in_flight_memory() += output_value.SizeEstimate();
  transaction_->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::PutOperation, connection->database()->AsWeakPtr(),
      std::move(params)));

  // Size can't be big enough to overflow because it represents the
  // actual bytes passed through IPC.
  transaction_->set_size(transaction_->size() + commit_size);
}

void TransactionImpl::PutAll(int64_t object_store_id,
                             std::vector<blink::mojom::IDBPutParamsPtr> puts,
                             PutAllCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(dispatcher_host_);

  if (!transaction_) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Unknown transaction.");
    std::move(callback).Run(
        blink::mojom::IDBTransactionPutAllResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  if (!transaction_->IsAcceptingRequests()) {
    mojo::ReportBadMessage(
        "PutAll was called after committing or aborting the transaction");
    return;
  }

  std::vector<std::vector<IndexedDBExternalObject>> external_objects_per_put(
      puts.size());
  for (size_t i = 0; i < puts.size(); i++) {
    if (!puts[i]->value->external_objects.empty())
      CreateExternalObjects(puts[i]->value, &external_objects_per_put[i]);
  }

  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected()) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Not connected.");
    std::move(callback).Run(
        blink::mojom::IDBTransactionPutAllResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  base::CheckedNumeric<uint64_t> commit_size = 0;
  base::CheckedNumeric<size_t> size_estimate = 0;
  std::vector<std::unique_ptr<IndexedDBDatabase::PutAllOperationParams>>
      put_params(puts.size());
  for (size_t i = 0; i < puts.size(); i++) {
    commit_size += puts[i]->value->bits.size();
    commit_size += puts[i]->key.size_estimate();
    put_params[i] =
        std::make_unique<IndexedDBDatabase::PutAllOperationParams>();
    // TODO(crbug.com/902498): Use mojom traits to map directly to
    // std::string.
    put_params[i]->value.bits =
        std::string(puts[i]->value->bits.begin(), puts[i]->value->bits.end());
    size_estimate += put_params[i]->value.SizeEstimate();
    puts[i]->value->bits.clear();
    put_params[i]->value.external_objects =
        std::move(external_objects_per_put[i]);
    put_params[i]->key = std::make_unique<blink::IndexedDBKey>(puts[i]->key);
    put_params[i]->index_keys = std::move(puts[i]->index_keys);
  }

  blink::mojom::IDBTransaction::PutAllCallback aborting_callback =
      CreateCallbackAbortOnDestruct<
          blink::mojom::IDBTransaction::PutAllCallback,
          blink::mojom::IDBTransactionPutAllResultPtr>(
          std::move(callback), transaction_->AsWeakPtr());

  transaction_->in_flight_memory() += size_estimate.ValueOrDefault(0);
  DCHECK(transaction_->in_flight_memory().IsValid());
  transaction_->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::PutAllOperation, connection->database()->AsWeakPtr(),
      object_store_id, std::move(put_params), std::move(aborting_callback)));

  // Size can't be big enough to overflow because it represents the
  // actual bytes passed through IPC.
  transaction_->set_size(transaction_->size() + base::checked_cast<uint64_t>(
                                                    commit_size.ValueOrDie()));
}

void TransactionImpl::CreateExternalObjects(
    blink::mojom::IDBValuePtr& value,
    std::vector<IndexedDBExternalObject>* external_objects) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Should only be called if there are external objects to process.
  CHECK(!value->external_objects.empty());

  base::CheckedNumeric<uint64_t> total_blob_size = 0;
  external_objects->resize(value->external_objects.size());
  for (size_t i = 0; i < value->external_objects.size(); ++i) {
    auto& object = value->external_objects[i];
    switch (object->which()) {
      case blink::mojom::IDBExternalObject::Tag::BLOB_OR_FILE: {
        blink::mojom::IDBBlobInfoPtr& info = object->get_blob_or_file();
        uint64_t size = info->size;
        total_blob_size += size;

        if (info->file) {
          DCHECK_NE(info->size, IndexedDBExternalObject::kUnknownSize);
          (*external_objects)[i] = IndexedDBExternalObject(
              std::move(info->blob), info->uuid, info->file->name,
              info->mime_type, info->file->last_modified, info->size);
        } else {
          (*external_objects)[i] = IndexedDBExternalObject(
              std::move(info->blob), info->uuid, info->mime_type, info->size);
        }
        break;
      }
      case blink::mojom::IDBExternalObject::Tag::FILE_SYSTEM_ACCESS_TOKEN:
        (*external_objects)[i] = IndexedDBExternalObject(
            std::move(object->get_file_system_access_token()));
        break;
    }
  }
}

void TransactionImpl::Commit(int64_t num_errors_handled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!transaction_)
    return;

  if (!transaction_->IsAcceptingRequests()) {
    // This really shouldn't be happening, but seems to be happening anyway. So
    // rather than killing the renderer, simply ignore the request.
    return;
  }

  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected())
    return;

  transaction_->SetNumErrorsHandled(num_errors_handled);

  // Always allow empty or delete-only transactions.
  if (transaction_->size() == 0) {
    connection->database()->Commit(transaction_.get());
    return;
  }

  indexed_db_context_->quota_manager_proxy()->GetUsageAndQuota(
      origin_, blink::mojom::StorageType::kTemporary,
      indexed_db_context_->IDBTaskRunner(),
      base::BindOnce(&TransactionImpl::OnGotUsageAndQuotaForCommit,
                     weak_factory_.GetWeakPtr()));
}

void TransactionImpl::OnGotUsageAndQuotaForCommit(
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!transaction_)
    return;

  // May have disconnected while quota check was pending.
  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected())
    return;

  if (status == blink::mojom::QuotaStatusCode::kOk &&
      usage + transaction_->size() <= quota) {
    connection->database()->Commit(transaction_.get());
  } else {
    connection->AbortTransactionAndTearDownOnError(
        transaction_.get(),
        IndexedDBDatabaseError(blink::mojom::IDBException::kQuotaError));
  }
}

}  // namespace content
