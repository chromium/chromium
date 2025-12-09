// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/database.h"

#include <math.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/base_tracing.h"
#include "base/types/expected_macros.h"
#include "base/unguessable_token.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom-shared.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_internals_types.mojom-forward.h"
#include "components/services/storage/privileged/mojom/indexed_db_internals_types.mojom.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_return_value.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/bucket_context_handle.h"
#include "content/browser/indexed_db/instance/callback_helpers.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/cursor.h"
#include "content/browser/indexed_db/instance/database_callbacks.h"
#include "content/browser/indexed_db/instance/factory_client.h"
#include "content/browser/indexed_db/instance/index_writer.h"
#include "content/browser/indexed_db/instance/pending_connection.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "content/browser/indexed_db/status.h"
#include "ipc/ipc_channel.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/leveldatabase/env_chromium.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexKeys;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBKeyRange;
using blink::IndexedDBObjectStoreMetadata;

namespace content::indexed_db {
namespace {

// Returns an `IDBReturnValuePtr` created from the cursor's current position.
blink::mojom::IDBReturnValuePtr ExtractReturnValueFromCursorValue(
    BucketContext& bucket_context,
    const IndexedDBObjectStoreMetadata& object_store_metadata,
    BackingStore::Cursor& cursor) {
  IndexedDBReturnValue idb_return_value(std::move(cursor.GetValue()));

  const bool is_generated_key =
      (!idb_return_value.empty() && object_store_metadata.auto_increment &&
       !object_store_metadata.key_path.IsNull());
  if (is_generated_key) {
    idb_return_value.primary_key = cursor.GetPrimaryKey().Clone();
    idb_return_value.key_path = object_store_metadata.key_path;
  }

  blink::mojom::IDBReturnValuePtr mojo_return_value =
      IndexedDBReturnValue::ConvertReturnValue(&idb_return_value);
  bucket_context.CreateAllExternalObjects(
      idb_return_value.external_objects,
      &mojo_return_value->value->external_objects);
  return mojo_return_value;
}

blink::mojom::IDBErrorPtr CreateIDBErrorPtr(blink::mojom::IDBException code,
                                            const std::string& message,
                                            Transaction* transaction) {
  transaction->IncrementNumErrorsSent();
  return blink::mojom::IDBError::New(code, base::UTF8ToUTF16(message));
}

}  // namespace

Database::OpenCursorOperationParams::OpenCursorOperationParams() = default;
Database::OpenCursorOperationParams::~OpenCursorOperationParams() = default;

Database::Database(const std::u16string& name, BucketContext& bucket_context)
    : name_(name),
      bucket_context_(bucket_context),
      connection_coordinator_(this, bucket_context) {}

Database::~Database() = default;

BackingStore* Database::backing_store() {
  return bucket_context_->backing_store();
}

PartitionedLockManager& Database::lock_manager() {
  return bucket_context_->lock_manager();
}

int64_t Database::version() const {
  return backing_store_db_ ? metadata().version
                           : blink::IndexedDBDatabaseMetadata::NO_VERSION;
}

bool Database::IsInitialized() const {
  return backing_store_db_ != nullptr;
}

bool Database::OnlyHasOneClient() const {
  if (connections_.empty()) {
    return true;
  }

  const base::UnguessableToken& token = (*connections_.begin())->client_token();
  return std::all_of(connections_.begin(), connections_.end(),
                     [&token](Connection* connection) {
                       return connection->client_token() == token;
                     });
}

void Database::RequireBlockingTransactionClientsToBeActive(
    Transaction* current_transaction,
    std::vector<PartitionedLockManager::PartitionedLockRequest>&
        lock_requests) {
  if (OnlyHasOneClient()) {
    return;
  }

  std::vector<PartitionedLockId> blocked_lock_ids =
      lock_manager().GetUnacquirableLocks(lock_requests);

  if (blocked_lock_ids.empty()) {
    return;
  }

  for (Connection* connection : connections_) {
    if (connection->client_token() ==
        current_transaction->connection()->client_token()) {
      continue;
    }

    // If any of the connection's transactions is holding one of the blocked
    // lock IDs, require that client to be active.
    if (connection->IsHoldingLocks(blocked_lock_ids)) {
      connection->DisallowInactiveClient(
          storage::mojom::DisallowInactiveClientReason::
              kTransactionIsAcquiringLocks,
          base::DoNothing());
    }
  }
}

void Database::RegisterAndScheduleTransaction(Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::RegisterAndScheduleTransaction",
               "txn.id", transaction->id());
  // Locks for version change transactions are covered by `ConnectionRequest`.
  DCHECK_NE(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);
  std::vector<PartitionedLockManager::PartitionedLockRequest> lock_requests =
      transaction->BuildLockRequests();

  RequireBlockingTransactionClientsToBeActive(transaction, lock_requests);

  lock_manager().AcquireLocks(
      std::move(lock_requests), *transaction->mutable_locks_receiver(),
      base::BindOnce(&Transaction::Start, transaction->AsWeakPtr()),
      base::BindRepeating(&Connection::HasHigherPriorityThan,
                          transaction->mutable_locks_receiver()));
}

Status Database::RunTasks() {
  // First execute any pending tasks in the connection coordinator.
  ConnectionCoordinator::ExecuteTaskResult task_state;
  Status status;
  do {
    std::tie(task_state, status) =
        connection_coordinator_.ExecuteTask(!connections_.empty());
  } while (task_state == ConnectionCoordinator::ExecuteTaskResult::kMoreTasks);

  if (task_state == ConnectionCoordinator::ExecuteTaskResult::kError) {
    CHECK(!status.ok());
    return status;
  }

  bool transactions_removed = true;

  // Finally, execute transactions that have tasks & remove those that are
  // complete.
  while (transactions_removed) {
    transactions_removed = false;
    Transaction* finished_upgrade_transaction = nullptr;
    bool upgrade_transaction_commmitted = false;
    for (Connection* connection : connections_) {
      std::vector<int64_t> txns_to_remove;
      for (const auto& id_txn_pair : connection->transactions()) {
        Transaction* txn = id_txn_pair.second.get();
        // Determine if the transaction's task queue should be processed.
        switch (txn->state()) {
          case Transaction::FINISHED:
            if (txn->mode() ==
                blink::mojom::IDBTransactionMode::VersionChange) {
              finished_upgrade_transaction = txn;
              upgrade_transaction_commmitted = !txn->aborted();
            }
            txns_to_remove.push_back(id_txn_pair.first);
            continue;
          case Transaction::CREATED:
            continue;
          case Transaction::STARTED:
          case Transaction::COMMITTING:
            break;
        }

        // Process the queue for transactions that are STARTED or COMMITTING.
        // Add transactions that can be removed to a queue.
        Transaction::RunTasksResult task_result;
        Status transaction_status;
        std::tie(task_result, transaction_status) = txn->RunTasks();
        switch (task_result) {
          case Transaction::RunTasksResult::kError:
            CHECK(!transaction_status.ok());
            return transaction_status;
          case Transaction::RunTasksResult::kCommitted:
          case Transaction::RunTasksResult::kAborted:
            if (txn->mode() ==
                blink::mojom::IDBTransactionMode::VersionChange) {
              DCHECK(!finished_upgrade_transaction);
              finished_upgrade_transaction = txn;
              upgrade_transaction_commmitted = !txn->aborted();
            }
            txns_to_remove.push_back(txn->id());
            break;
          case Transaction::RunTasksResult::kNotFinished:
            continue;
        }
      }
      // Do the removals.
      for (int64_t id : txns_to_remove) {
        connection->RemoveTransaction(id);
        transactions_removed = true;
      }
      if (finished_upgrade_transaction) {
        connection_coordinator_.OnUpgradeTransactionFinished(
            upgrade_transaction_commmitted);
      }
    }
  }
  return Status::OK();
}

Status Database::ForceCloseAndRunTasks(const std::string& message) {
  DCHECK(!force_closing_);
  force_closing_ = true;
  for (Connection* connection : connections_) {
    connection->CloseAndReportForceClose(message);
  }
  connections_.clear();
  IDB_RETURN_IF_ERROR(connection_coordinator_.PruneTasksForForceClose(message));
  connection_coordinator_.OnNoConnections();

  // Execute any pending tasks in the connection coordinator.
  ConnectionCoordinator::ExecuteTaskResult task_state;
  Status status;
  do {
    std::tie(task_state, status) = connection_coordinator_.ExecuteTask(false);
    DCHECK(task_state !=
           ConnectionCoordinator::ExecuteTaskResult::kPendingAsyncWork)
        << "There are no more connections, so all tasks should be able to "
           "complete synchronously.";
  } while (task_state != ConnectionCoordinator::ExecuteTaskResult::kDone &&
           task_state != ConnectionCoordinator::ExecuteTaskResult::kError);
  DCHECK(connections_.empty());
  force_closing_ = false;
  bucket_context_->QueueRunTasks();
  return status;
}

void Database::ScheduleOpenConnection(
    std::unique_ptr<PendingConnection> connection) {
  CHECK(IsAcceptingConnections());

  connection_coordinator_.ScheduleOpenConnection(std::move(connection));
}

void Database::ScheduleDeleteDatabase(
    std::unique_ptr<FactoryClient> factory_client,
    base::OnceClosure on_deletion_complete) {
  connection_coordinator_.ScheduleDeleteDatabase(
      std::move(factory_client), std::move(on_deletion_complete));
}

Status Database::VersionChangeOperation(int64_t version,
                                        Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::VersionChangeOperation", "txn.id",
               transaction->id());
  int64_t old_version = metadata().version;
  DCHECK_GT(version, old_version);

  IDB_RETURN_IF_ERROR(
      transaction->BackingStoreTransaction()->SetDatabaseVersion(version));

  connection_coordinator_.BindVersionChangeTransactionReceiver();
  connection_coordinator_.OnUpgradeTransactionStarted(old_version);
  return Status::OK();
}

Status Database::GetOperation(int64_t object_store_id,
                              int64_t index_id,
                              IndexedDBKeyRange key_range,
                              CursorType cursor_type,
                              blink::mojom::IDBDatabase::GetCallback callback,
                              Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::GetOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(object_store_id, index_id)) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        CreateIDBErrorPtr(blink::mojom::IDBException::kUnknownError,
                          "Bad request", transaction)));
    return Status::InvalidArgument("Invalid object_store_id and/or index_id.");
  }

  const IndexedDBObjectStoreMetadata& object_store_metadata =
      GetObjectStoreMetadata(object_store_id);

  IndexedDBKey key;
  if (key_range.IsOnlyKey()) {
    key = std::move(key_range).TakeOnlyKey();
  } else {
    StatusOr<std::unique_ptr<BackingStore::Cursor>> backing_store_cursor;
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // ObjectStore Retrieval Operation
      if (cursor_type == CursorType::kKeyOnly) {
        backing_store_cursor =
            transaction->BackingStoreTransaction()->OpenObjectStoreKeyCursor(
                object_store_id, key_range,
                blink::mojom::IDBCursorDirection::Next);
      } else {
        backing_store_cursor =
            transaction->BackingStoreTransaction()->OpenObjectStoreCursor(
                object_store_id, key_range,
                blink::mojom::IDBCursorDirection::Next);
      }
    } else if (cursor_type == CursorType::kKeyOnly) {
      // Index Value Retrieval Operation
      backing_store_cursor =
          transaction->BackingStoreTransaction()->OpenIndexKeyCursor(
              object_store_id, index_id, key_range,
              blink::mojom::IDBCursorDirection::Next);
    } else {
      // Index Referenced Value Retrieval Operation
      backing_store_cursor =
          transaction->BackingStoreTransaction()->OpenIndexCursor(
              object_store_id, index_id, key_range,
              blink::mojom::IDBCursorDirection::Next);
    }

    if (!backing_store_cursor.has_value()) {
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewErrorResult(CreateIDBErrorPtr(
              blink::mojom::IDBException::kUnknownError,
              "Corruption detected, unable to continue", transaction)));
      return backing_store_cursor.error();
    }

    if (!*backing_store_cursor) {
      // This means we've run out of data.
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
      return Status::OK();
    }

    key = std::move(**backing_store_cursor).TakeKey();
  }

  if (index_id == IndexedDBIndexMetadata::kInvalidId) {
    // Object Store Retrieval Operation
    IndexedDBReturnValue value;
    Status s = transaction->BackingStoreTransaction()->GetRecord(
        object_store_id, key, &value);
    if (!s.ok()) {
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewErrorResult(
              CreateIDBErrorPtr(blink::mojom::IDBException::kUnknownError,
                                "Unknown error", transaction)));
      return s;
    }

    if (value.empty()) {
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
      return s;
    }

    if (cursor_type == CursorType::kKeyOnly) {
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewKey(std::move(key)));
      return s;
    }

    if (object_store_metadata.auto_increment &&
        !object_store_metadata.key_path.IsNull()) {
      value.primary_key = std::move(key);
      value.key_path = object_store_metadata.key_path;
    }

    blink::mojom::IDBReturnValuePtr mojo_value =
        IndexedDBReturnValue::ConvertReturnValue(&value);
    bucket_context_->CreateAllExternalObjects(
        value.external_objects, &mojo_value->value->external_objects);
    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetResult::NewValue(std::move(mojo_value)));
    return s;
  }

  // From here we are dealing only with indexes.
  std::unique_ptr<IndexedDBKey> primary_key;
  Status s = transaction->BackingStoreTransaction()->GetPrimaryKeyViaIndex(
      object_store_id, index_id, key, &primary_key);
  if (!s.ok()) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        CreateIDBErrorPtr(blink::mojom::IDBException::kUnknownError,
                          "Unknown error", transaction)));
    return s;
  }

  if (!primary_key) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
    return s;
  }
  if (cursor_type == CursorType::kKeyOnly) {
    // Index Value Retrieval Operation
    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetResult::NewKey(std::move(*primary_key)));
    return s;
  }

  // Index Referenced Value Retrieval Operation
  IndexedDBReturnValue value;
  s = transaction->BackingStoreTransaction()->GetRecord(object_store_id,
                                                        *primary_key, &value);
  if (!s.ok()) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        CreateIDBErrorPtr(blink::mojom::IDBException::kUnknownError,
                          "Unknown error", transaction)));
    return s;
  }

  if (value.empty()) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
    return s;
  }
  if (object_store_metadata.auto_increment &&
      !object_store_metadata.key_path.IsNull()) {
    value.primary_key = std::move(*primary_key);
    value.key_path = object_store_metadata.key_path;
  }

  blink::mojom::IDBReturnValuePtr mojo_value =
      IndexedDBReturnValue::ConvertReturnValue(&value);
  bucket_context_->CreateAllExternalObjects(
      value.external_objects, &mojo_value->value->external_objects);
  std::move(callback).Run(
      blink::mojom::IDBDatabaseGetResult::NewValue(std::move(mojo_value)));
  return s;
}

Transaction::Operation Database::CreateGetAllOperation(
    int64_t object_store_id,
    int64_t index_id,
    blink::IndexedDBKeyRange key_range,
    blink::mojom::IDBGetAllResultType result_type,
    int64_t max_count,
    blink::mojom::IDBCursorDirection direction,
    blink::mojom::IDBDatabase::GetAllCallback callback,
    Transaction* transaction) {
  return BindWeakOperation(&Database::GetAllOperation, AsWeakPtr(),
                           object_store_id, index_id, std::move(key_range),
                           result_type, max_count, direction,
                           std::make_unique<GetAllResultSinkWrapper>(
                               transaction->AsWeakPtr(), std::move(callback)));
}

static_assert(sizeof(size_t) >= sizeof(int32_t),
              "Size of size_t is less than size of int32");
static_assert(blink::mojom::kIDBMaxMessageOverhead <= INT32_MAX,
              "kIDBMaxMessageOverhead is more than INT32_MAX");

Database::GetAllResultSinkWrapper::GetAllResultSinkWrapper(
    base::WeakPtr<Transaction> transaction,
    blink::mojom::IDBDatabase::GetAllCallback callback)
    : transaction_(transaction), callback_(std::move(callback)) {}

Database::GetAllResultSinkWrapper::~GetAllResultSinkWrapper() {
  if (!callback_) {
    return;
  }

  if (transaction_) {
    transaction_->IncrementNumErrorsSent();
    // If we're reaching this line because the Connection has been
    // disconnected from its remote, then `result_sink_` won't have been
    // successfully associated, and invoking any methods on it will CHECK.
    // See crbug.com/346955148.
    // TODO(crbug.com/347047640): remove this workaround when 347047640 is
    // fixed.
    if (!transaction_->connection()->is_shutting_down()) {
      DatabaseError error(blink::mojom::IDBException::kIgnorableAbortError,
                          "Backend aborted error");
      Get()->OnError(
          blink::mojom::IDBError::New(error.code(), error.message()));
    }
  } else {
    // Make sure `callback_` is invoked because the Mojo client is waiting for a
    // response.
    Get();
  }
}

mojo::AssociatedRemote<blink::mojom::IDBDatabaseGetAllResultSink>&
Database::GetAllResultSinkWrapper::Get() {
  if (!result_sink_) {
    mojo::PendingAssociatedReceiver<blink::mojom::IDBDatabaseGetAllResultSink>
        pending_receiver;
    if (use_dedicated_receiver_for_testing_) {
      pending_receiver = result_sink_.BindNewEndpointAndPassDedicatedReceiver();
    } else {
      pending_receiver = result_sink_.BindNewEndpointAndPassReceiver();
    }
    std::move(callback_).Run(std::move(pending_receiver));
  }
  return result_sink_;
}

Status Database::GetAllOperation(
    int64_t object_store_id,
    int64_t index_id,
    IndexedDBKeyRange key_range,
    blink::mojom::IDBGetAllResultType result_type,
    int64_t max_count,
    blink::mojom::IDBCursorDirection direction,
    std::unique_ptr<GetAllResultSinkWrapper> result_sink,
    Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::GetAllOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(object_store_id, index_id)) {
    result_sink->Get()->OnError(CreateIDBErrorPtr(
        blink::mojom::IDBException::kUnknownError, "Bad request", transaction));
    return Status::InvalidArgument("Invalid object_store_id.");
  }

  DCHECK_GT(max_count, 0);

  const IndexedDBObjectStoreMetadata& object_store_metadata =
      GetObjectStoreMetadata(object_store_id);

  StatusOr<std::unique_ptr<BackingStore::Cursor>> cursor;

  if (result_type == blink::mojom::IDBGetAllResultType::Keys) {
    // Retrieving keys
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // Object Store: Key Retrieval Operation
      cursor = transaction->BackingStoreTransaction()->OpenObjectStoreKeyCursor(
          object_store_id, key_range, direction);
    } else {
      // Index Value: (Primary Key) Retrieval Operation
      cursor = transaction->BackingStoreTransaction()->OpenIndexKeyCursor(
          object_store_id, index_id, key_range, direction);
    }
  } else {
    // Retrieving values
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // Object Store: Value Retrieval Operation
      cursor = transaction->BackingStoreTransaction()->OpenObjectStoreCursor(
          object_store_id, key_range, direction);
    } else {
      // Object Store: Referenced Value Retrieval Operation
      cursor = transaction->BackingStoreTransaction()->OpenIndexCursor(
          object_store_id, index_id, key_range, direction);
    }
  }

  if (!cursor.has_value()) {
    DLOG(ERROR) << "Unable to open cursor operation: "
                << cursor.error().ToString();
    result_sink->Get()->OnError(CreateIDBErrorPtr(
        blink::mojom::IDBException::kUnknownError,
        "Corruption detected, unable to continue", transaction));
    return cursor.error();
  }

  std::vector<blink::mojom::IDBRecordPtr> found_records;

  auto send_records = [&](bool done) {
    result_sink->Get()->ReceiveResults(std::move(found_records), done);
    found_records.clear();
  };

  // No records found.
  if (!*cursor) {
    send_records(/*done=*/true);
    return Status::OK();
  }

  bool did_first_seek = false;

  // Max idbvalue size before blob wrapping is 64k, so make an assumption
  // that max key/value size is 128kb tops, to fit under 128mb mojo limit.
  // This value is just a heuristic and is an attempt to make sure that
  // GetAll fits under the message limit size.
  static_assert(
      blink::mojom::kIDBMaxMessageSize >
          blink::mojom::kIDBGetAllChunkSize * blink::mojom::kIDBWrapThreshold,
      "Chunk heuristic too large");

  const size_t max_values_before_sending = blink::mojom::kIDBGetAllChunkSize;
  int64_t num_found_items = 0;
  while (num_found_items++ < max_count) {
    bool cursor_valid;
    Status s;
    if (did_first_seek) {
      cursor_valid = (*cursor)->Continue(&s);
    } else {
      // Cursor creation performs the first seek, returning a nullptr cursor
      // when invalid.
      cursor_valid = true;
      did_first_seek = true;
    }
    IDB_RETURN_IF_ERROR_AND_DO(
        s, result_sink->Get()->OnError(CreateIDBErrorPtr(
               blink::mojom::IDBException::kUnknownError,
               "Seek failure, unable to continue", transaction)));

    if (!cursor_valid) {
      break;
    }

    blink::mojom::IDBRecordPtr return_record;

    if (result_type == blink::mojom::IDBGetAllResultType::Keys) {
      return_record =
          blink::mojom::IDBRecord::New((*cursor)->GetPrimaryKey().Clone(),
                                       /*value=*/nullptr,
                                       /*index_key=*/std::nullopt);
    } else if (result_type == blink::mojom::IDBGetAllResultType::Values) {
      blink::mojom::IDBReturnValuePtr return_value =
          ExtractReturnValueFromCursorValue(bucket_context_.get(),
                                            object_store_metadata, **cursor);
      return_record = blink::mojom::IDBRecord::New(
          /*primary_key=*/std::nullopt, std::move(return_value),
          /*index_key=*/std::nullopt);
    } else if (result_type == blink::mojom::IDBGetAllResultType::Records) {
      // Construct the record, which includes the primary key, value and index
      // key.
      blink::mojom::IDBReturnValuePtr return_value =
          ExtractReturnValueFromCursorValue(bucket_context_.get(),
                                            object_store_metadata, **cursor);
      std::optional<IndexedDBKey> index_key;
      if (index_id != IndexedDBIndexMetadata::kInvalidId) {
        // The index key only exists for `IDBIndex::getAllRecords()`.
        index_key = (*cursor)->GetKey().Clone();
      }
      return_record = blink::mojom::IDBRecord::New(
          (*cursor)->GetPrimaryKey().Clone(), std::move(return_value),
          std::move(index_key));
    } else {
      NOTREACHED();
    }

    found_records.emplace_back(std::move(return_record));

    // Periodically stream records if we have too many.
    if (found_records.size() >= max_values_before_sending) {
      send_records(/*done=*/false);
    }
  }
  send_records(/*done=*/true);
  return Status::OK();
}

Status Database::SetIndexKeysOperation(
    int64_t object_store_id,
    IndexedDBKey primary_key,
    std::vector<IndexedDBIndexKeys> index_keys,
    Transaction* transaction) {
  DCHECK(transaction);
  TRACE_EVENT1("IndexedDB", "Database::SetIndexKeysOperation", "txn.id",
               transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  ASSIGN_OR_RETURN(
      std::optional<BackingStore::RecordIdentifier> found_record,
      transaction->BackingStoreTransaction()->KeyExistsInObjectStore(
          object_store_id, primary_key));
  if (!found_record) {
    return transaction->Abort(
        DatabaseError(blink::mojom::IDBException::kUnknownError,
                      "Internal error setting index keys for object store."));
  }

  std::vector<std::unique_ptr<IndexWriter>> index_writers;
  std::string error_message;
  bool obeys_constraints = false;

  const IndexedDBObjectStoreMetadata& object_store_metadata =
      GetObjectStoreMetadata(object_store_id);
  bool backing_store_success =
      MakeIndexWriters(transaction, object_store_metadata, primary_key, false,
                       std::move(index_keys), &index_writers, &error_message,
                       &obeys_constraints);
  if (!backing_store_success) {
    return transaction->Abort(DatabaseError(
        blink::mojom::IDBException::kUnknownError,
        "Internal error: backing store error updating index keys."));
  }
  if (!obeys_constraints) {
    return transaction->Abort(DatabaseError(
        blink::mojom::IDBException::kConstraintError, error_message));
  }

  for (const auto& writer : index_writers) {
    IDB_RETURN_IF_ERROR(writer->WriteIndexKeys(
        *found_record, transaction->BackingStoreTransaction(),
        object_store_id));
  }
  return Status::OK();
}

Status Database::SetIndexesReadyOperation(size_t index_count,
                                          Transaction* transaction) {
  // TODO(dmurph): This method should be refactored out for something more
  // reliable.
  for (size_t i = 0; i < index_count; ++i) {
    transaction->DidCompletePreemptiveEvent();
  }
  return Status::OK();
}

Status Database::OpenCursorOperation(
    std::unique_ptr<OpenCursorOperationParams> params,
    const storage::BucketLocator& bucket_locator,
    Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::OpenCursorOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(params->object_store_id,
                                                params->index_id)) {
    return Status::InvalidArgument("Invalid object_store_id and/or index_id.");
  }

  // The frontend has begun indexing, so this pauses the transaction
  // until the indexing is complete. This can't happen any earlier
  // because we don't want to switch to early mode in case multiple
  // indexes are being created in a row, with Put()'s in between.
  if (params->task_type == blink::mojom::IDBTaskType::Preemptive) {
    transaction->AddPreemptiveEvent();
  }

  StatusOr<std::unique_ptr<BackingStore::Cursor>> backing_store_cursor;
  if (params->index_id == IndexedDBIndexMetadata::kInvalidId) {
    if (params->cursor_type == CursorType::kKeyOnly) {
      DCHECK_EQ(params->task_type, blink::mojom::IDBTaskType::Normal);
      backing_store_cursor =
          transaction->BackingStoreTransaction()->OpenObjectStoreKeyCursor(
              params->object_store_id, params->key_range, params->direction);
    } else {
      backing_store_cursor =
          transaction->BackingStoreTransaction()->OpenObjectStoreCursor(
              params->object_store_id, params->key_range, params->direction);
    }
  } else {
    DCHECK_EQ(params->task_type, blink::mojom::IDBTaskType::Normal);
    if (params->cursor_type == CursorType::kKeyOnly) {
      backing_store_cursor =
          transaction->BackingStoreTransaction()->OpenIndexKeyCursor(
              params->object_store_id, params->index_id, params->key_range,
              params->direction);
    } else {
      backing_store_cursor =
          transaction->BackingStoreTransaction()->OpenIndexCursor(
              params->object_store_id, params->index_id, params->key_range,
              params->direction);
    }
  }

  if (!backing_store_cursor.has_value()) {
    DLOG(ERROR) << "Unable to open cursor operation: "
                << backing_store_cursor.error().ToString();
    return backing_store_cursor.error();
  }

  if (!*backing_store_cursor) {
    // Occurs when we've reached the end of cursor's data.
    std::move(params->callback)
        .Run(blink::mojom::IDBDatabaseOpenCursorResult::NewEmpty(true));
    return Status::OK();
  }

  mojo::PendingAssociatedRemote<blink::mojom::IDBCursor> pending_remote;
  Cursor* cursor = Cursor::CreateAndBind(
      std::move(*backing_store_cursor), params->cursor_type, params->task_type,
      transaction->AsWeakPtr(), pending_remote);
  transaction->RegisterOpenCursor(cursor);

  blink::mojom::IDBValuePtr mojo_value;
  std::vector<IndexedDBExternalObject> external_objects;
  if (cursor->Value()) {
    mojo_value = IndexedDBValue::ConvertAndEraseValue(cursor->Value());
    external_objects.swap(cursor->Value()->external_objects);
  }

  if (mojo_value) {
    bucket_context_->CreateAllExternalObjects(external_objects,
                                              &mojo_value->external_objects);
  }

  std::move(params->callback)
      .Run(blink::mojom::IDBDatabaseOpenCursorResult::NewValue(
          blink::mojom::IDBDatabaseOpenCursorValue::New(
              std::move(pending_remote), cursor->key().Clone(),
              cursor->primary_key().Clone(), std::move(mojo_value))));
  return Status::OK();
}

Status Database::CountOperation(
    int64_t object_store_id,
    int64_t index_id,
    IndexedDBKeyRange key_range,
    blink::mojom::IDBDatabase::CountCallback callback,
    Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::CountOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(object_store_id, index_id)) {
    return Status::InvalidArgument("Invalid object_store_id and/or index_id.");
  }

  StatusOr<std::unique_ptr<BackingStore::Cursor>> backing_store_cursor;
  if (index_id == IndexedDBIndexMetadata::kInvalidId) {
    backing_store_cursor =
        transaction->BackingStoreTransaction()->OpenObjectStoreKeyCursor(
            object_store_id, key_range, blink::mojom::IDBCursorDirection::Next);
  } else {
    backing_store_cursor =
        transaction->BackingStoreTransaction()->OpenIndexKeyCursor(
            object_store_id, index_id, key_range,
            blink::mojom::IDBCursorDirection::Next);
  }
  if (!backing_store_cursor.has_value()) {
    DLOG(ERROR) << "Unable perform count operation: "
                << backing_store_cursor.error().ToString();
    return backing_store_cursor.error();
  }

  if (!*backing_store_cursor) {
    std::move(callback).Run(/*success=*/true, /*count=*/0);
    return Status::OK();
  }

  uint32_t count = 1;
  Status s;
  while ((*backing_store_cursor)->Continue(&s)) {
    IDB_RETURN_IF_ERROR(s);
    ++count;
  }
  std::move(callback).Run(/*success=*/true, count);
  return Status::OK();
}

Status Database::DeleteRangeOperation(
    int64_t object_store_id,
    IndexedDBKeyRange key_range,
    blink::mojom::IDBDatabase::DeleteRangeCallback success_callback,
    Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::DeleteRangeOperation", "txn.id",
               transaction->id());

  Status s;
  if (IsObjectStoreIdInMetadata(object_store_id)) {
    s = transaction->BackingStoreTransaction()->DeleteRange(object_store_id,
                                                            key_range);
  } else {
    s = Status::InvalidArgument("Invalid object_store_id.");
  }
  if (s.ok()) {
    const IndexedDBObjectStoreMetadata& object_store_metadata =
        GetObjectStoreMetadata(object_store_id);
    bucket_context_->delegate().on_content_changed.Run(
        metadata().name, object_store_metadata.name);
  }
  std::move(success_callback).Run(s.ok());
  return s;
}

Status Database::GetKeyGeneratorCurrentNumberOperation(
    int64_t object_store_id,
    blink::mojom::IDBDatabase::GetKeyGeneratorCurrentNumberCallback callback,
    Transaction* transaction) {
  if (!IsObjectStoreIdInMetadata(object_store_id)) {
    std::move(callback).Run(
        -1, CreateIDBErrorPtr(blink::mojom::IDBException::kDataError,
                              "Object store id not valid.", transaction));
    return Status::InvalidArgument("Invalid object_store_id.");
  }

  ASSIGN_OR_RETURN(
      int64_t current_number,
      transaction->BackingStoreTransaction()->GetKeyGeneratorCurrentNumber(
          object_store_id),
      [&callback, transaction](const Status& status) {
        std::move(callback).Run(
            -1, CreateIDBErrorPtr(
                    blink::mojom::IDBException::kDataError,
                    "Failed to get the current number of key generator.",
                    transaction));
        return status;
      });

  std::move(callback).Run(current_number, nullptr);
  return Status::OK();
}

Status Database::ClearOperation(
    int64_t object_store_id,
    blink::mojom::IDBDatabase::ClearCallback success_callback,
    Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::ClearOperation", "txn.id",
               transaction->id());
  Status s = Status::InvalidArgument("Invalid object_store_id.");
  if (IsObjectStoreIdInMetadata(object_store_id)) {
    s = transaction->BackingStoreTransaction()->ClearObjectStore(
        object_store_id);
  }
  if (s.ok()) {
    const IndexedDBObjectStoreMetadata& object_store_metadata =
        GetObjectStoreMetadata(object_store_id);
    bucket_context_->delegate().on_content_changed.Run(
        name_, object_store_metadata.name);
  }
  std::move(success_callback).Run(s.ok());
  return s;
}

bool Database::IsObjectStoreIdInMetadata(int64_t object_store_id) const {
  if (!base::Contains(metadata().object_stores, object_store_id)) {
    DLOG(ERROR) << "Invalid object_store_id";
    return false;
  }
  return true;
}

bool Database::IsObjectStoreIdAndMaybeIndexIdInMetadata(
    int64_t object_store_id,
    int64_t index_id) const {
  if (!IsObjectStoreIdInMetadata(object_store_id)) {
    return false;
  }
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      GetObjectStoreMetadata(object_store_id);
  if (index_id != IndexedDBIndexMetadata::kInvalidId &&
      !base::Contains(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

storage::mojom::IdbDatabaseMetadataPtr Database::GetIdbInternalsMetadata()
    const {
  storage::mojom::IdbDatabaseMetadataPtr info =
      storage::mojom::IdbDatabaseMetadata::New();
  info->name = name();
  info->connection_count = ConnectionCount();
  info->active_open_delete = ActiveOpenDeleteCount();
  info->pending_open_delete = PendingOpenDeleteCount();
  for (const Connection* connection : connections()) {
    for (const auto& [_, transaction] : connection->transactions()) {
      info->transactions.push_back(transaction->GetIdbInternalsMetadata());
    }
  }
  return info;
}

void Database::NotifyOfIdbInternalsRelevantChange() {
  // This metadata is included in the context metadata, so call up the chain.
  bucket_context_->NotifyOfIdbInternalsRelevantChange();
}

// kIDBMaxMessageSize is defined based on the original
// IPC::Channel::kMaximumMessageSize value.  We use kIDBMaxMessageSize to
// limit the size of arguments we pass into our Mojo calls.  We want to ensure
// this value is always no bigger than the current kMaximumMessageSize value
// which also ensures it is always no bigger than the current Mojo message
// size limit.
static_assert(
    blink::mojom::kIDBMaxMessageSize <= IPC::Channel::kMaximumMessageSize,
    "kIDBMaxMessageSize is bigger than IPC::Channel::kMaximumMessageSize");

void Database::CallUpgradeTransactionStartedForTesting(int64_t old_version) {
  connection_coordinator_.OnUpgradeTransactionStarted(old_version);
}

Status Database::OpenInternal() {
  auto result = backing_store()->CreateOrOpenDatabase(name_);
  if (result.has_value()) {
    backing_store_db_ = std::move(result.value());
    return Status::OK();
  }
  return result.error();
}

std::unique_ptr<Connection> Database::CreateConnection(
    std::unique_ptr<DatabaseCallbacks> database_callbacks,
    mojo::Remote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker,
    base::UnguessableToken client_token,
    int scheduling_priority) {
  auto connection = std::make_unique<Connection>(
      *bucket_context_, weak_factory_.GetWeakPtr(),
      base::BindRepeating(&Database::VersionChangeIgnored,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&Database::ConnectionClosed, weak_factory_.GetWeakPtr()),
      std::move(database_callbacks), std::move(client_state_checker),
      client_token, scheduling_priority);
  connections_.insert(connection.get());
  bucket_context_->OnConnectionPriorityUpdated();
  return connection;
}

void Database::VersionChangeIgnored() {
  connection_coordinator_.OnVersionChangeIgnored();
}

bool Database::HasNoConnections() const {
  return force_closing_ || connections().empty();
}

void Database::SendVersionChangeToAllConnections(int64_t old_version,
                                                 int64_t new_version) {
  if (force_closing_) {
    return;
  }
  for (auto* connection : connections()) {
    // Before invoking this method, the `ConnectionCoordinator` had
    // set the request state to `kPendingNoConnections`. Now the request will
    // be blocked until all the existing connections to this database is
    // closed. There are three possible ways for the connection to be closed:
    // 1. If the client is already pending close, then the `VersionChange`
    // event will be ignored and the open request will be deemed blocked until
    // the pending close completes.
    // 2. If the client is active, the `VersionChange` event will be enqueued
    // and the registered event listener will be fired asynchronously. The
    // event listener should be responsible for actively closing the IndexedDB
    // connection. The document won't be eligible for BFCache before the
    // connection is closed if it receives the `versionchange` event.
    // 3. While the above two cases rely on the `VersionChange` event to be
    // delivered to the renderer process, the third case happens purely from
    // the IndexedDB/browser context. If the client is inactive, the
    // `VersionChange` event will not be delivered, instead, a mojo call is
    // sent to the browser process to disallow the activation of the inactive
    // client, which will close the connection as part of the destruction. No
    // matter which path it follows, the `SendVersionChangeToAllConnections`
    // method is executed asynchronously.
    connection->DisallowInactiveClient(
        storage::mojom::DisallowInactiveClientReason::kVersionChangeEvent,
        base::BindOnce(
            [](base::WeakPtr<Connection> connection, int64_t old_version,
               int64_t new_version, bool was_client_active) {
              if (connection && connection->IsConnected() &&
                  was_client_active) {
                connection->callbacks()->OnVersionChange(old_version,
                                                         new_version);
              }
            },
            connection->GetWeakPtr(), old_version, new_version));
  }
}

void Database::ConnectionClosed(Connection* connection) {
  TRACE_EVENT0("IndexedDB", "Database::ConnectionClosed");
  // Ignore connection closes during force close to prevent re-entry.
  if (force_closing_) {
    return;
  }
  connections_.erase(connection);
  bucket_context_->OnConnectionPriorityUpdated();
  connection_coordinator_.OnConnectionClosed(connection);
  if (connections_.empty()) {
    connection_coordinator_.OnNoConnections();
  }
  if (CanBeDestroyed()) {
    bucket_context_->QueueRunTasks();
  }
}

bool Database::CanBeDestroyed() {
  return !connection_coordinator_.HasTasks() && connections_.empty();
}

const IndexedDBObjectStoreMetadata& Database::GetObjectStoreMetadata(
    int64_t object_store_id) const {
  auto object_store_it = metadata().object_stores.find(object_store_id);
  DCHECK(object_store_it != metadata().object_stores.end());
  return object_store_it->second;
}

}  // namespace content::indexed_db
