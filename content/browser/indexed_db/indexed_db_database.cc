// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database.h"

#include <math.h>
#include <algorithm>
#include <cstddef>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/base_tracing.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"
#include "content/browser/indexed_db/cursor_impl.h"
#include "content/browser/indexed_db/indexed_db_bucket_state_handle.h"
#include "content/browser/indexed_db/indexed_db_callback_helpers.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_index_writer.h"
#include "content/browser/indexed_db/indexed_db_metadata_coding.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/indexed_db_return_value.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/transaction_impl.h"
#include "ipc/ipc_channel.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "url/origin.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexKeys;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBKeyRange;
using blink::IndexedDBObjectStoreMetadata;
using leveldb::Status;

namespace content {
namespace {

std::vector<blink::mojom::IDBReturnValuePtr> CreateMojoValues(
    std::vector<IndexedDBReturnValue>& found_values,
    IndexedDBDispatcherHost* dispatcher_host,
    const storage::BucketLocator& bucket_locator) {
  std::vector<blink::mojom::IDBReturnValuePtr> mojo_values;
  mojo_values.reserve(found_values.size());
  for (size_t i = 0; i < found_values.size(); ++i) {
    mojo_values.push_back(
        IndexedDBReturnValue::ConvertReturnValue(&found_values[i]));
    dispatcher_host->CreateAllExternalObjects(
        bucket_locator, found_values[i].external_objects,
        &mojo_values[i]->value->external_objects);
  }
  return mojo_values;
}

IndexedDBDatabaseError CreateError(blink::mojom::IDBException code,
                                   const char* message,
                                   IndexedDBTransaction* transaction) {
  transaction->IncrementNumErrorsSent();
  return IndexedDBDatabaseError(code, message);
}

IndexedDBDatabaseError CreateError(blink::mojom::IDBException code,
                                   const std::u16string& message,
                                   IndexedDBTransaction* transaction) {
  transaction->IncrementNumErrorsSent();
  return IndexedDBDatabaseError(code, message);
}

std::unique_ptr<IndexedDBKey> GenerateKey(IndexedDBBackingStore* backing_store,
                                          IndexedDBTransaction* transaction,
                                          int64_t database_id,
                                          int64_t object_store_id) {
  // Maximum integer uniquely representable as ECMAScript number.
  const int64_t max_generator_value = 9007199254740992LL;
  int64_t current_number;
  Status s = backing_store->GetKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(), database_id, object_store_id,
      &current_number);
  if (!s.ok()) {
    LOG(ERROR) << "Failed to GetKeyGeneratorCurrentNumber";
    return std::make_unique<IndexedDBKey>();
  }
  if (current_number < 0 || current_number > max_generator_value)
    return std::make_unique<IndexedDBKey>();

  return std::make_unique<IndexedDBKey>(current_number,
                                        blink::mojom::IDBKeyType::Number);
}

// Called at the end of a "put" operation. The key is a number that was either
// generated by the generator which now needs to be incremented (so
// `check_current` is false) or was user-supplied so we only conditionally use
// (and `check_current` is true).
Status UpdateKeyGenerator(IndexedDBBackingStore* backing_store,
                          IndexedDBTransaction* transaction,
                          int64_t database_id,
                          int64_t object_store_id,
                          const IndexedDBKey& key,
                          bool check_current) {
  DCHECK_EQ(blink::mojom::IDBKeyType::Number, key.type());
  // Maximum integer uniquely representable as ECMAScript number.
  const double max_generator_value = 9007199254740992.0;
  int64_t value = base::saturated_cast<int64_t>(
      floor(std::min(key.number(), max_generator_value)));
  return backing_store->MaybeUpdateKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(), database_id, object_store_id,
      value + 1, check_current);
}

}  // namespace

IndexedDBDatabase::PutOperationParams::PutOperationParams() = default;
IndexedDBDatabase::PutOperationParams::~PutOperationParams() = default;

IndexedDBDatabase::OpenCursorOperationParams::OpenCursorOperationParams() =
    default;
IndexedDBDatabase::OpenCursorOperationParams::~OpenCursorOperationParams() =
    default;

IndexedDBDatabase::IndexedDBDatabase(
    const std::u16string& name,
    IndexedDBBackingStore* backing_store,
    IndexedDBFactory* factory,
    IndexedDBClassFactory* class_factory,
    TasksAvailableCallback tasks_available_callback,
    std::unique_ptr<IndexedDBMetadataCoding> metadata_coding,
    const Identifier& unique_identifier,
    PartitionedLockManager* transaction_lock_manager)
    : backing_store_(backing_store),
      metadata_(name,
                kInvalidId,
                IndexedDBDatabaseMetadata::NO_VERSION,
                kInvalidId),
      identifier_(unique_identifier),
      factory_(factory),
      class_factory_(class_factory),
      metadata_coding_(std::move(metadata_coding)),
      lock_manager_(transaction_lock_manager),
      tasks_available_callback_(tasks_available_callback),
      connection_coordinator_(this, tasks_available_callback) {
  DCHECK(factory != nullptr);
}

IndexedDBDatabase::~IndexedDBDatabase() = default;

std::vector<PartitionedLockManager::PartitionedLockRequest>
IndexedDBDatabase::BuildLockRequestsFromTransaction(
    IndexedDBTransaction* transaction) const {
  std::vector<PartitionedLockManager::PartitionedLockRequest> lock_requests;
  lock_requests.reserve(1 + transaction->scope().size());
  lock_requests.emplace_back(
      GetDatabaseLockId(id()),
      transaction->mode() == blink::mojom::IDBTransactionMode::VersionChange
          ? PartitionedLockManager::LockType::kExclusive
          : PartitionedLockManager::LockType::kShared);
  PartitionedLockManager::LockType lock_type =
      transaction->mode() == blink::mojom::IDBTransactionMode::ReadOnly
          ? PartitionedLockManager::LockType::kShared
          : PartitionedLockManager::LockType::kExclusive;
  for (int64_t object_store : transaction->scope()) {
    lock_requests.emplace_back(GetObjectStoreLockId(id(), object_store),
                               lock_type);
  }
  return lock_requests;
}

void IndexedDBDatabase::RequireBlockingTransactionClientsToBeActive(
    IndexedDBTransaction* current_transaction,
    std::vector<PartitionedLockManager::PartitionedLockRequest>&
        lock_requests) {
  std::vector<PartitionedLockId> blocked_lock_ids =
      lock_manager_->GetUnacquirableLocks(lock_requests);

  if (blocked_lock_ids.empty()) {
    return;
  }

  for (IndexedDBConnection* connection : connections_) {
    bool should_require_connection_to_be_active = false;
    for (const auto& [existing_transaction_id, existing_transaction] :
         connection->transactions()) {
      if (connection->id() == current_transaction->connection()->id() &&
          existing_transaction_id == current_transaction->id()) {
        // This is the current transaction itself, we skip the check.
        continue;
      }
      for (PartitionedLockId blocked_lock_id : blocked_lock_ids) {
        if (existing_transaction->lock_ids().contains(blocked_lock_id)) {
          should_require_connection_to_be_active = true;
          break;
        }
      }
    }
    if (should_require_connection_to_be_active) {
      connection->DisallowInactiveClient(
          storage::mojom::DisallowInactiveClientReason::
              kTransactionIsBlockingOthers,
          base::DoNothing());
    }
  }
}

void IndexedDBDatabase::RegisterAndScheduleTransaction(
    IndexedDBTransaction* transaction) {
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::RegisterAndScheduleTransaction",
               "txn.id", transaction->id());
  std::vector<PartitionedLockManager::PartitionedLockRequest> lock_requests =
      BuildLockRequestsFromTransaction(transaction);

  if (blink::features::
          IsAllowPageWithIDBConnectionAndTransactionInBFCacheEnabled()) {
    RequireBlockingTransactionClientsToBeActive(transaction, lock_requests);
  }

  lock_manager_->AcquireLocks(
      std::move(lock_requests),
      transaction->mutable_locks_receiver()->AsWeakPtr(),
      base::BindOnce(&IndexedDBTransaction::Start, transaction->AsWeakPtr()));
}

std::tuple<IndexedDBDatabase::RunTasksResult, leveldb::Status>
IndexedDBDatabase::RunTasks() {
  // First execute any pending tasks in the connection coordinator.
  IndexedDBConnectionCoordinator::ExecuteTaskResult task_state;
  leveldb::Status status;
  do {
    std::tie(task_state, status) =
        connection_coordinator_.ExecuteTask(!connections_.empty());
  } while (task_state ==
           IndexedDBConnectionCoordinator::ExecuteTaskResult::kMoreTasks);

  if (task_state == IndexedDBConnectionCoordinator::ExecuteTaskResult::kError)
    return {RunTasksResult::kError, status};

  bool transactions_removed = true;

  // Finally, execute transactions that have tasks & remove those that are
  // complete.
  while (transactions_removed) {
    transactions_removed = false;
    IndexedDBTransaction* finished_upgrade_transaction = nullptr;
    bool upgrade_transaction_commmitted = false;
    for (IndexedDBConnection* connection : connections_) {
      std::vector<int64_t> txns_to_remove;
      for (const auto& id_txn_pair : connection->transactions()) {
        IndexedDBTransaction* txn = id_txn_pair.second.get();
        // Determine if the transaction's task queue should be processed.
        switch (txn->state()) {
          case IndexedDBTransaction::FINISHED:
            if (txn->mode() ==
                blink::mojom::IDBTransactionMode::VersionChange) {
              finished_upgrade_transaction = txn;
              upgrade_transaction_commmitted = !txn->aborted();
            }
            txns_to_remove.push_back(id_txn_pair.first);
            continue;
          case IndexedDBTransaction::CREATED:
            continue;
          case IndexedDBTransaction::STARTED:
          case IndexedDBTransaction::COMMITTING:
            break;
        }

        // Process the queue for transactions that are STARTED or COMMITTING.
        // Add transactions that can be removed to a queue.
        IndexedDBTransaction::RunTasksResult task_result;
        leveldb::Status transaction_status;
        std::tie(task_result, transaction_status) = txn->RunTasks();
        switch (task_result) {
          case IndexedDBTransaction::RunTasksResult::kError:
            return {RunTasksResult::kError, transaction_status};
          case IndexedDBTransaction::RunTasksResult::kCommitted:
          case IndexedDBTransaction::RunTasksResult::kAborted:
            if (txn->mode() ==
                blink::mojom::IDBTransactionMode::VersionChange) {
              DCHECK(!finished_upgrade_transaction);
              finished_upgrade_transaction = txn;
              upgrade_transaction_commmitted = !txn->aborted();
            }
            txns_to_remove.push_back(txn->id());
            break;
          case IndexedDBTransaction::RunTasksResult::kNotFinished:
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
  if (CanBeDestroyed())
    return {RunTasksResult::kCanBeDestroyed, leveldb::Status::OK()};
  return {RunTasksResult::kDone, leveldb::Status::OK()};
}

leveldb::Status IndexedDBDatabase::ForceCloseAndRunTasks() {
  leveldb::Status status;
  DCHECK(!force_closing_);
  force_closing_ = true;
  for (IndexedDBConnection* connection : connections_) {
    leveldb::Status last_error = connection->CloseAndReportForceClose();
    if (UNLIKELY(!last_error.ok())) {
      base::UmaHistogramEnumeration(
          "WebCore.IndexedDB.ErrorDuringForceCloseAborts",
          leveldb_env::GetLevelDBStatusUMAValue(last_error),
          leveldb_env::LEVELDB_STATUS_MAX);
    }
  }
  connections_.clear();
  leveldb::Status abort_status =
      connection_coordinator_.PruneTasksForForceClose();
  if (UNLIKELY(!abort_status.ok()))
    return abort_status;
  connection_coordinator_.OnNoConnections();

  // Execute any pending tasks in the connection coordinator.
  IndexedDBConnectionCoordinator::ExecuteTaskResult task_state;
  do {
    std::tie(task_state, status) = connection_coordinator_.ExecuteTask(false);
    DCHECK(task_state !=
           IndexedDBConnectionCoordinator::ExecuteTaskResult::kPendingAsyncWork)
        << "There are no more connections, so all tasks should be able to "
           "complete synchronously.";
  } while (
      task_state != IndexedDBConnectionCoordinator::ExecuteTaskResult::kDone &&
      task_state != IndexedDBConnectionCoordinator::ExecuteTaskResult::kError);
  DCHECK(connections_.empty());
  force_closing_ = false;
  if (CanBeDestroyed())
    tasks_available_callback_.Run();
  return status;
}

void IndexedDBDatabase::Commit(IndexedDBTransaction* transaction) {
  // The frontend suggests that we commit, but we may have previously initiated
  // an abort, and so have disposed of the transaction. on_abort has already
  // been dispatched to the frontend, so it will find out about that
  // asynchronously.
  if (transaction) {
    transaction->SetCommitFlag();
  }
}

void IndexedDBDatabase::TransactionCreated() {
  ++transaction_count_;
}

void IndexedDBDatabase::TransactionFinished(
    blink::mojom::IDBTransactionMode mode,
    bool committed) {
  --transaction_count_;
  DCHECK_GE(transaction_count_, 0);

  // TODO(dmurph): To help remove this integration with IndexedDBDatabase, make
  // a 'committed' listener closure on all transactions. Then the request can
  // just listen for that.

  // This may be an unrelated transaction finishing while waiting for
  // connections to close, or the actual upgrade transaction from an active
  // request. Notify the active request if it's the latter.
  if (mode == blink::mojom::IDBTransactionMode::VersionChange) {
    connection_coordinator_.OnUpgradeTransactionFinished(committed);
  }
}

void IndexedDBDatabase::ScheduleOpenConnection(
    IndexedDBBucketStateHandle bucket_state_handle,
    std::unique_ptr<IndexedDBPendingConnection> connection,
    scoped_refptr<IndexedDBClientStateCheckerWrapper> client_state_checker) {
  connection_coordinator_.ScheduleOpenConnection(
      std::move(bucket_state_handle), std::move(connection),
      std::move(client_state_checker));
}

void IndexedDBDatabase::ScheduleDeleteDatabase(
    IndexedDBBucketStateHandle bucket_state_handle,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    base::OnceClosure on_deletion_complete) {
  connection_coordinator_.ScheduleDeleteDatabase(
      std::move(bucket_state_handle), std::move(callbacks),
      std::move(on_deletion_complete));
}

void IndexedDBDatabase::AddObjectStoreToMetadata(
    IndexedDBObjectStoreMetadata object_store,
    int64_t new_max_object_store_id) {
  DCHECK(metadata_.object_stores.find(object_store.id) ==
         metadata_.object_stores.end());
  if (new_max_object_store_id != IndexedDBObjectStoreMetadata::kInvalidId) {
    DCHECK_LT(metadata_.max_object_store_id, new_max_object_store_id);
    metadata_.max_object_store_id = new_max_object_store_id;
  }
  metadata_.object_stores[object_store.id] = std::move(object_store);
}

IndexedDBObjectStoreMetadata IndexedDBDatabase::RemoveObjectStoreFromMetadata(
    int64_t object_store_id) {
  auto it = metadata_.object_stores.find(object_store_id);
  CHECK(it != metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata metadata = std::move(it->second);
  metadata_.object_stores.erase(it);
  return metadata;
}

void IndexedDBDatabase::AddIndexToMetadata(int64_t object_store_id,
                                           IndexedDBIndexMetadata index,
                                           int64_t new_max_index_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index.id) == object_store.indexes.end());
  object_store.indexes[index.id] = std::move(index);
  if (new_max_index_id != IndexedDBIndexMetadata::kInvalidId) {
    DCHECK_LT(object_store.max_index_id, new_max_index_id);
    object_store.max_index_id = new_max_index_id;
  }
}

IndexedDBIndexMetadata IndexedDBDatabase::RemoveIndexFromMetadata(
    int64_t object_store_id,
    int64_t index_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[object_store_id];

  auto it = object_store.indexes.find(index_id);
  CHECK(it != object_store.indexes.end());
  IndexedDBIndexMetadata metadata = std::move(it->second);
  object_store.indexes.erase(it);
  return metadata;
}

leveldb::Status IndexedDBDatabase::CreateObjectStoreOperation(
    int64_t object_store_id,
    const std::u16string& name,
    const IndexedDBKeyPath& key_path,
    bool auto_increment,
    IndexedDBTransaction* transaction) {
  DCHECK(transaction);
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::CreateObjectStoreOperation",
               "txn.id", transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (base::Contains(metadata_.object_stores, object_store_id))
    return leveldb::Status::InvalidArgument("Invalid object_store_id");

  IndexedDBObjectStoreMetadata object_store_metadata;
  Status s = metadata_coding_->CreateObjectStore(
      transaction->BackingStoreTransaction()->transaction(),
      transaction->database()->id(), object_store_id, name, key_path,
      auto_increment, &object_store_metadata);

  if (!s.ok())
    return s;

  AddObjectStoreToMetadata(std::move(object_store_metadata), object_store_id);

  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::CreateObjectStoreAbortOperation,
                     AsWeakPtr(), object_store_id));
  return Status::OK();
}

void IndexedDBDatabase::CreateObjectStoreAbortOperation(
    int64_t object_store_id) {
  TRACE_EVENT0("IndexedDB",
               "IndexedDBDatabase::CreateObjectStoreAbortOperation");
  RemoveObjectStoreFromMetadata(object_store_id);
}

Status IndexedDBDatabase::DeleteObjectStoreOperation(
    int64_t object_store_id,
    IndexedDBTransaction* transaction) {
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::DeleteObjectStoreOperation",
               "txn.id", transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!IsObjectStoreIdInMetadata(object_store_id))
    return leveldb::Status::InvalidArgument("Invalid object_store_id.");

  IndexedDBObjectStoreMetadata object_store_metadata =
      RemoveObjectStoreFromMetadata(object_store_id);

  // First remove metadata.
  Status s = metadata_coding_->DeleteObjectStore(
      transaction->BackingStoreTransaction()->transaction(),
      transaction->database()->id(), object_store_metadata);

  if (!s.ok()) {
    AddObjectStoreToMetadata(std::move(object_store_metadata),
                             IndexedDBObjectStoreMetadata::kInvalidId);
    return s;
  }

  // Then remove object store contents.
  s = backing_store_->ClearObjectStore(transaction->BackingStoreTransaction(),
                                       transaction->database()->id(),
                                       object_store_id);

  if (!s.ok()) {
    AddObjectStoreToMetadata(std::move(object_store_metadata),
                             IndexedDBObjectStoreMetadata::kInvalidId);
    return s;
  }
  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::DeleteObjectStoreAbortOperation,
                     AsWeakPtr(), std::move(object_store_metadata)));
  return s;
}

void IndexedDBDatabase::DeleteObjectStoreAbortOperation(
    IndexedDBObjectStoreMetadata object_store_metadata) {
  TRACE_EVENT0("IndexedDB",
               "IndexedDBDatabase::DeleteObjectStoreAbortOperation");
  AddObjectStoreToMetadata(std::move(object_store_metadata),
                           IndexedDBObjectStoreMetadata::kInvalidId);
}

leveldb::Status IndexedDBDatabase::RenameObjectStoreOperation(
    int64_t object_store_id,
    const std::u16string& new_name,
    IndexedDBTransaction* transaction) {
  DCHECK(transaction);
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::RenameObjectStore", "txn.id",
               transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!IsObjectStoreIdInMetadata(object_store_id))
    return leveldb::Status::InvalidArgument("Invalid object_store_id.");

  // Store renaming is done synchronously, as it may be followed by
  // index creation (also sync) since preemptive OpenCursor/SetIndexKeys
  // may follow.
  IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  std::u16string old_name;

  Status s = metadata_coding_->RenameObjectStore(
      transaction->BackingStoreTransaction()->transaction(),
      transaction->database()->id(), new_name, &old_name,
      &object_store_metadata);

  if (!s.ok())
    return s;
  DCHECK_EQ(object_store_metadata.name, new_name);

  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::RenameObjectStoreAbortOperation,
                     AsWeakPtr(), object_store_id, std::move(old_name)));
  return leveldb::Status::OK();
}

void IndexedDBDatabase::RenameObjectStoreAbortOperation(
    int64_t object_store_id,
    std::u16string old_name) {
  TRACE_EVENT0("IndexedDB",
               "IndexedDBDatabase::RenameObjectStoreAbortOperation");

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  metadata_.object_stores[object_store_id].name = std::move(old_name);
}

Status IndexedDBDatabase::VersionChangeOperation(
    int64_t version,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::VersionChangeOperation",
               "txn.id", transaction->id());
  int64_t old_version = metadata_.version;
  DCHECK_GT(version, old_version);

  leveldb::Status s = metadata_coding_->SetDatabaseVersion(
      transaction->BackingStoreTransaction()->transaction(), id(), version,
      &metadata_);
  if (!s.ok())
    return s;

  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::VersionChangeAbortOperation,
                     AsWeakPtr(), old_version));

  connection_coordinator_.CreateAndBindUpgradeTransaction();
  connection_coordinator_.OnUpgradeTransactionStarted(old_version);
  return Status::OK();
}

void IndexedDBDatabase::VersionChangeAbortOperation(int64_t previous_version) {
  TRACE_EVENT0("IndexedDB", "IndexedDBDatabase::VersionChangeAbortOperation");
  metadata_.version = previous_version;
}

leveldb::Status IndexedDBDatabase::CreateIndexOperation(
    int64_t object_store_id,
    int64_t index_id,
    const std::u16string& name,
    const IndexedDBKeyPath& key_path,
    bool unique,
    bool multi_entry,
    IndexedDBTransaction* transaction) {
  DCHECK(transaction);
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::CreateIndexOperation", "txn.id",
               transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!IsObjectStoreIdInMetadataAndIndexNotInMetadata(object_store_id,
                                                      index_id)) {
    return leveldb::Status::InvalidArgument(
        "Invalid object_store_id and/or index_id.");
  }

  IndexedDBIndexMetadata index_metadata;
  Status s = metadata_coding_->CreateIndex(
      transaction->BackingStoreTransaction()->transaction(),
      transaction->database()->id(), object_store_id, index_id, name, key_path,
      unique, multi_entry, &index_metadata);

  if (!s.ok())
    return s;

  AddIndexToMetadata(object_store_id, std::move(index_metadata), index_id);
  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::CreateIndexAbortOperation, AsWeakPtr(),
                     object_store_id, index_id));
  return s;
}

void IndexedDBDatabase::CreateIndexAbortOperation(int64_t object_store_id,
                                                  int64_t index_id) {
  TRACE_EVENT0("IndexedDB", "IndexedDBDatabase::CreateIndexAbortOperation");
  RemoveIndexFromMetadata(object_store_id, index_id);
}

Status IndexedDBDatabase::DeleteIndexOperation(
    int64_t object_store_id,
    int64_t index_id,
    IndexedDBTransaction* transaction) {
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::DeleteIndexOperation", "txn.id",
               transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!IsObjectStoreIdAndIndexIdInMetadata(object_store_id, index_id)) {
    return leveldb::Status::InvalidArgument(
        "Invalid object_store_id and/or index_id.");
  }

  IndexedDBIndexMetadata index_metadata =
      RemoveIndexFromMetadata(object_store_id, index_id);

  Status s = metadata_coding_->DeleteIndex(
      transaction->BackingStoreTransaction()->transaction(),
      transaction->database()->id(), object_store_id, index_metadata);

  if (!s.ok())
    return s;

  s = backing_store_->ClearIndex(transaction->BackingStoreTransaction(),
                                 transaction->database()->id(), object_store_id,
                                 index_id);
  if (!s.ok()) {
    AddIndexToMetadata(object_store_id, std::move(index_metadata),
                       IndexedDBIndexMetadata::kInvalidId);
    return s;
  }

  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::DeleteIndexAbortOperation, AsWeakPtr(),
                     object_store_id, std::move(index_metadata)));
  return s;
}

void IndexedDBDatabase::DeleteIndexAbortOperation(
    int64_t object_store_id,
    IndexedDBIndexMetadata index_metadata) {
  TRACE_EVENT0("IndexedDB", "IndexedDBDatabase::DeleteIndexAbortOperation");
  AddIndexToMetadata(object_store_id, std::move(index_metadata),
                     IndexedDBIndexMetadata::kInvalidId);
}

leveldb::Status IndexedDBDatabase::RenameIndexOperation(
    int64_t object_store_id,
    int64_t index_id,
    const std::u16string& new_name,
    IndexedDBTransaction* transaction) {
  DCHECK(transaction);
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::RenameIndex", "txn.id",
               transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!IsObjectStoreIdAndIndexIdInMetadata(object_store_id, index_id)) {
    return leveldb::Status::InvalidArgument(
        "Invalid object_store_id and/or index_id.");
  }

  IndexedDBIndexMetadata& index_metadata =
      metadata_.object_stores[object_store_id].indexes[index_id];

  std::u16string old_name;
  Status s = metadata_coding_->RenameIndex(
      transaction->BackingStoreTransaction()->transaction(),
      transaction->database()->id(), object_store_id, new_name, &old_name,
      &index_metadata);
  if (!s.ok())
    return s;

  DCHECK_EQ(index_metadata.name, new_name);
  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::RenameIndexAbortOperation, AsWeakPtr(),
                     object_store_id, index_id, std::move(old_name)));
  return leveldb::Status::OK();
}

void IndexedDBDatabase::RenameIndexAbortOperation(int64_t object_store_id,
                                                  int64_t index_id,
                                                  std::u16string old_name) {
  TRACE_EVENT0("IndexedDB", "IndexedDBDatabase::RenameIndexAbortOperation");

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index_id) != object_store.indexes.end());
  object_store.indexes[index_id].name = std::move(old_name);
}

Status IndexedDBDatabase::GetOperation(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    indexed_db::CursorType cursor_type,
    blink::mojom::IDBDatabase::GetCallback callback,
    IndexedDBTransaction* transaction) {
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::GetOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(object_store_id, index_id)) {
    IndexedDBDatabaseError error = CreateError(
        blink::mojom::IDBException::kUnknownError, "Bad request", transaction);
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return leveldb::Status::InvalidArgument(
        "Invalid object_store_id and/or index_id.");
  }

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  const IndexedDBKey* key;

  Status s = Status::OK();
  if (!dispatcher_host) {
    IndexedDBDatabaseError error =
        CreateError(blink::mojom::IDBException::kUnknownError, "Unknown error",
                    transaction);
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  std::unique_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;
  if (key_range->IsOnlyKey()) {
    key = &key_range->lower();
  } else {
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // ObjectStore Retrieval Operation
      if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
        backing_store_cursor = backing_store_->OpenObjectStoreKeyCursor(
            transaction->BackingStoreTransaction(), id(), object_store_id,
            *key_range, blink::mojom::IDBCursorDirection::Next, &s);
      } else {
        backing_store_cursor = backing_store_->OpenObjectStoreCursor(
            transaction->BackingStoreTransaction(), id(), object_store_id,
            *key_range, blink::mojom::IDBCursorDirection::Next, &s);
      }
    } else if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      // Index Value Retrieval Operation
      backing_store_cursor = backing_store_->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    } else {
      // Index Referenced Value Retrieval Operation
      backing_store_cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    }

    if (!s.ok()) {
      IndexedDBDatabaseError error =
          CreateError(blink::mojom::IDBException::kUnknownError,
                      "Corruption detected, unable to continue", transaction);
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewErrorResult(
              blink::mojom::IDBError::New(error.code(), error.message())));
      return s;
    }

    if (!backing_store_cursor) {
      // This means we've run out of data.
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
      return s;
    }

    key = &backing_store_cursor->key();
  }

  if (index_id == IndexedDBIndexMetadata::kInvalidId) {
    // Object Store Retrieval Operation
    IndexedDBReturnValue value;
    s = backing_store_->GetRecord(transaction->BackingStoreTransaction(), id(),
                                  object_store_id, *key, &value);
    if (!s.ok()) {
      IndexedDBDatabaseError error =
          CreateError(blink::mojom::IDBException::kUnknownError,
                      "Unknown error", transaction);
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewErrorResult(
              blink::mojom::IDBError::New(error.code(), error.message())));
      return s;
    }

    if (value.empty()) {
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
      return s;
    }

    if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewKey(std::move(*key)));
      return s;
    }

    if (object_store_metadata.auto_increment &&
        !object_store_metadata.key_path.IsNull()) {
      value.primary_key = *key;
      value.key_path = object_store_metadata.key_path;
    }

    blink::mojom::IDBReturnValuePtr mojo_value =
        IndexedDBReturnValue::ConvertReturnValue(&value);
    dispatcher_host->CreateAllExternalObjects(
        bucket_locator(), value.external_objects,
        &mojo_value->value->external_objects);
    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetResult::NewValue(std::move(mojo_value)));
    return s;
  }

  // From here we are dealing only with indexes.
  std::unique_ptr<IndexedDBKey> primary_key;
  s = backing_store_->GetPrimaryKeyViaIndex(
      transaction->BackingStoreTransaction(), id(), object_store_id, index_id,
      *key, &primary_key);
  if (!s.ok()) {
    IndexedDBDatabaseError error =
        CreateError(blink::mojom::IDBException::kUnknownError, "Unknown error",
                    transaction);
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  if (!primary_key) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
    return s;
  }
  if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
    // Index Value Retrieval Operation
    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetResult::NewKey(std::move(*primary_key)));
    return s;
  }

  // Index Referenced Value Retrieval Operation
  IndexedDBReturnValue value;
  s = backing_store_->GetRecord(transaction->BackingStoreTransaction(), id(),
                                object_store_id, *primary_key, &value);
  if (!s.ok()) {
    IndexedDBDatabaseError error =
        CreateError(blink::mojom::IDBException::kUnknownError, "Unknown error",
                    transaction);
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  if (value.empty()) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
    return s;
  }
  if (object_store_metadata.auto_increment &&
      !object_store_metadata.key_path.IsNull()) {
    value.primary_key = *primary_key;
    value.key_path = object_store_metadata.key_path;
  }

  blink::mojom::IDBReturnValuePtr mojo_value =
      IndexedDBReturnValue::ConvertReturnValue(&value);
  dispatcher_host->CreateAllExternalObjects(
      bucket_locator(), value.external_objects,
      &mojo_value->value->external_objects);
  std::move(callback).Run(
      blink::mojom::IDBDatabaseGetResult::NewValue(std::move(mojo_value)));
  return s;
}

static_assert(sizeof(size_t) >= sizeof(int32_t),
              "Size of size_t is less than size of int32");
static_assert(blink::mojom::kIDBMaxMessageOverhead <= INT32_MAX,
              "kIDBMaxMessageOverhead is more than INT32_MAX");

Status IndexedDBDatabase::GetAllOperation(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    indexed_db::CursorType cursor_type,
    int64_t max_count,
    blink::mojom::IDBDatabase::GetAllCallback callback,
    IndexedDBTransaction* transaction) {
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::GetAllOperation", "txn.id",
               transaction->id());

  mojo::Remote<blink::mojom::IDBDatabaseGetAllResultSink> result_sink;
  std::move(callback).Run(result_sink.BindNewPipeAndPassReceiver());

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(object_store_id, index_id)) {
    IndexedDBDatabaseError error = CreateError(
        blink::mojom::IDBException::kUnknownError, "Bad request", transaction);
    result_sink->OnError(
        blink::mojom::IDBError::New(error.code(), error.message()));
    return leveldb::Status::InvalidArgument("Invalid object_store_id.");
  }

  DCHECK_GT(max_count, 0);

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  Status s = Status::OK();
  if (!dispatcher_host) {
    IndexedDBDatabaseError error =
        CreateError(blink::mojom::IDBException::kUnknownError, "Unknown error",
                    transaction);
    result_sink->OnError(
        blink::mojom::IDBError::New(error.code(), error.message()));
    return s;
  }

  std::unique_ptr<IndexedDBBackingStore::Cursor> cursor;

  if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
    // Retrieving keys
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // Object Store: Key Retrieval Operation
      cursor = backing_store_->OpenObjectStoreKeyCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    } else {
      // Index Value: (Primary Key) Retrieval Operation
      cursor = backing_store_->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    }
  } else {
    // Retrieving values
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // Object Store: Value Retrieval Operation
      cursor = backing_store_->OpenObjectStoreCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    } else {
      // Object Store: Referenced Value Retrieval Operation
      cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    }
  }

  if (!s.ok()) {
    DLOG(ERROR) << "Unable to open cursor operation: " << s.ToString();
    IndexedDBDatabaseError error =
        CreateError(blink::mojom::IDBException::kUnknownError,
                    "Corruption detected, unable to continue", transaction);
    result_sink->OnError(
        blink::mojom::IDBError::New(error.code(), error.message()));
    return s;
  }

  std::vector<IndexedDBKey> found_keys;
  std::vector<IndexedDBReturnValue> found_values;
  if (!cursor) {
    // No values or keys found.
    return s;
  }

  bool did_first_seek = false;
  bool generated_key = object_store_metadata.auto_increment &&
                       !object_store_metadata.key_path.IsNull();

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
    if (did_first_seek) {
      cursor_valid = cursor->Continue(&s);
    } else {
      cursor_valid = cursor->FirstSeek(&s);
      did_first_seek = true;
    }
    if (!s.ok()) {
      IndexedDBDatabaseError error =
          CreateError(blink::mojom::IDBException::kUnknownError,
                      "Seek failure, unable to continue", transaction);
      result_sink->OnError(
          blink::mojom::IDBError::New(error.code(), error.message()));
      return s;
    }

    if (!cursor_valid)
      break;

    IndexedDBReturnValue return_value;
    IndexedDBKey return_key;

    if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      return_key = cursor->primary_key();
    } else {
      // Retrieving values
      return_value.swap(*cursor->value());
      if (!return_value.empty() && generated_key) {
        return_value.primary_key = cursor->primary_key();
        return_value.key_path = object_store_metadata.key_path;
      }
    }

    if (cursor_type == indexed_db::CURSOR_KEY_ONLY)
      found_keys.push_back(return_key);
    else
      found_values.push_back(return_value);

    // Periodically stream values and keys if we have too many.
    if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      if (found_keys.size() >= max_values_before_sending) {
        result_sink->ReceiveKeys(std::move(found_keys));
        found_keys.clear();
      }
    } else {
      if (found_values.size() >= max_values_before_sending) {
        result_sink->ReceiveValues(CreateMojoValues(
            found_values, dispatcher_host.get(), bucket_locator()));
        found_values.clear();
      }
    }
  }

  if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
    if (!found_keys.empty()) {
      result_sink->ReceiveKeys(std::move(found_keys));
    }
  } else {
    if (!found_values.empty()) {
      result_sink->ReceiveValues(CreateMojoValues(
          found_values, dispatcher_host.get(), bucket_locator()));
    }
  }
  return s;
}

Status IndexedDBDatabase::PutOperation(
    std::unique_ptr<PutOperationParams> params,
    IndexedDBTransaction* transaction) {
  TRACE_EVENT2("IndexedDB", "IndexedDBDatabase::PutOperation", "txn.id",
               transaction->id(), "size", params->value.SizeEstimate());
  DCHECK_NE(transaction->mode(), blink::mojom::IDBTransactionMode::ReadOnly);
  bool key_was_generated = false;
  Status s = Status::OK();
  transaction->in_flight_memory() -= params->value.SizeEstimate();
  DCHECK(transaction->in_flight_memory().IsValid());

  if (!IsObjectStoreIdInMetadata(params->object_store_id)) {
    IndexedDBDatabaseError error = CreateError(
        blink::mojom::IDBException::kUnknownError, "Bad request", transaction);
    std::move(params->callback)
        .Run(blink::mojom::IDBTransactionPutResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return leveldb::Status::InvalidArgument("Invalid object_store_id.");
  }

  DCHECK(metadata_.object_stores.find(params->object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[params->object_store_id];
  DCHECK(object_store.auto_increment || params->key->IsValid());

  std::unique_ptr<IndexedDBKey> key;
  if (params->put_mode != blink::mojom::IDBPutMode::CursorUpdate &&
      object_store.auto_increment && !params->key->IsValid()) {
    std::unique_ptr<IndexedDBKey> auto_inc_key =
        GenerateKey(backing_store_, transaction, id(), params->object_store_id);
    key_was_generated = true;
    if (!auto_inc_key->IsValid()) {
      IndexedDBDatabaseError error =
          CreateError(blink::mojom::IDBException::kConstraintError,
                      "Maximum key generator value reached.", transaction);
      std::move(params->callback)
          .Run(blink::mojom::IDBTransactionPutResult::NewErrorResult(
              blink::mojom::IDBError::New(error.code(), error.message())));
      return s;
    }
    key = std::move(auto_inc_key);
  } else {
    key = std::move(params->key);
  }

  DCHECK(key->IsValid());

  IndexedDBBackingStore::RecordIdentifier record_identifier;
  if (params->put_mode == blink::mojom::IDBPutMode::AddOnly) {
    bool found = false;
    Status found_status = backing_store_->KeyExistsInObjectStore(
        transaction->BackingStoreTransaction(), id(), params->object_store_id,
        *key, &record_identifier, &found);
    if (!found_status.ok())
      return found_status;
    if (found) {
      IndexedDBDatabaseError error =
          CreateError(blink::mojom::IDBException::kConstraintError,
                      "Key already exists in the object store.", transaction);
      std::move(params->callback)
          .Run(blink::mojom::IDBTransactionPutResult::NewErrorResult(
              blink::mojom::IDBError::New(error.code(), error.message())));
      return found_status;
    }
  }

  std::vector<std::unique_ptr<IndexWriter>> index_writers;
  std::u16string error_message;
  bool obeys_constraints = false;
  bool backing_store_success = MakeIndexWriters(
      transaction, backing_store_, id(), object_store, *key, key_was_generated,
      params->index_keys, &index_writers, &error_message, &obeys_constraints);
  if (!backing_store_success) {
    IndexedDBDatabaseError error =
        CreateError(blink::mojom::IDBException::kUnknownError,
                    "Internal error: backing store error updating index keys.",
                    transaction);
    std::move(params->callback)
        .Run(blink::mojom::IDBTransactionPutResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }
  if (!obeys_constraints) {
    IndexedDBDatabaseError error =
        CreateError(blink::mojom::IDBException::kConstraintError, error_message,
                    transaction);
    std::move(params->callback)
        .Run(blink::mojom::IDBTransactionPutResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  // Before this point, don't do any mutation. After this point, rollback the
  // transaction in case of error.
  s = backing_store_->PutRecord(transaction->BackingStoreTransaction(), id(),
                                params->object_store_id, *key, &params->value,
                                &record_identifier);
  if (!s.ok())
    return s;

  {
    TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::PutOperation.UpdateIndexes",
                 "txn.id", transaction->id());
    for (const auto& writer : index_writers) {
      writer->WriteIndexKeys(record_identifier, backing_store_,
                             transaction->BackingStoreTransaction(), id(),
                             params->object_store_id);
    }
  }

  if (object_store.auto_increment &&
      params->put_mode != blink::mojom::IDBPutMode::CursorUpdate &&
      key->type() == blink::mojom::IDBKeyType::Number) {
    TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::PutOperation.AutoIncrement",
                 "txn.id", transaction->id());
    s = UpdateKeyGenerator(backing_store_, transaction, id(),
                           params->object_store_id, *key, !key_was_generated);
    if (!s.ok())
      return s;
  }
  {
    TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::PutOperation.Callbacks",
                 "txn.id", transaction->id());
    std::move(params->callback)
        .Run(blink::mojom::IDBTransactionPutResult::NewKey(*key));
  }
  factory_->NotifyIndexedDBContentChanged(
      bucket_locator(), metadata_.name,
      metadata_.object_stores[params->object_store_id].name);
  return s;
}

Status IndexedDBDatabase::SetIndexKeysOperation(
    int64_t object_store_id,
    std::unique_ptr<IndexedDBKey> primary_key,
    const std::vector<IndexedDBIndexKeys>& index_keys,
    IndexedDBTransaction* transaction) {
  DCHECK(transaction);
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::SetIndexKeysOperation",
               "txn.id", transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  IndexedDBBackingStore::RecordIdentifier record_identifier;
  bool found = false;
  Status s = backing_store_->KeyExistsInObjectStore(
      transaction->BackingStoreTransaction(), metadata_.id, object_store_id,
      *primary_key, &record_identifier, &found);
  if (!s.ok())
    return s;
  if (!found) {
    return transaction->Abort(IndexedDBDatabaseError(
        blink::mojom::IDBException::kUnknownError,
        "Internal error setting index keys for object store."));
  }

  std::vector<std::unique_ptr<IndexWriter>> index_writers;
  std::u16string error_message;
  bool obeys_constraints = false;
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];
  bool backing_store_success = MakeIndexWriters(
      transaction, backing_store_, id(), object_store_metadata, *primary_key,
      false, index_keys, &index_writers, &error_message, &obeys_constraints);
  if (!backing_store_success) {
    return transaction->Abort(IndexedDBDatabaseError(
        blink::mojom::IDBException::kUnknownError,
        "Internal error: backing store error updating index keys."));
  }
  if (!obeys_constraints) {
    return transaction->Abort(IndexedDBDatabaseError(
        blink::mojom::IDBException::kConstraintError, error_message));
  }

  for (const auto& writer : index_writers) {
    s = writer->WriteIndexKeys(record_identifier, backing_store_,
                               transaction->BackingStoreTransaction(), id(),
                               object_store_id);
    if (!s.ok())
      return s;
  }
  return leveldb::Status::OK();
}

Status IndexedDBDatabase::BatchGetAllOperation(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    int64_t object_store_id,
    int64_t index_id,
    const std::vector<IndexedDBKeyRange>& key_ranges,
    uint64_t max_count,
    blink::mojom::IDBDatabase::BatchGetAllCallback callback,
    IndexedDBTransaction* transaction) {
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::BatchGetAllOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(object_store_id, index_id)) {
    IndexedDBDatabaseError error = CreateError(
        blink::mojom::IDBException::kUnknownError, "Bad request", transaction);
    std::move(callback).Run(
        blink::mojom::IDBDatabaseBatchGetAllResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return leveldb::Status::InvalidArgument("Invalid object_store_id.");
  }

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  Status s = Status::OK();
  if (!dispatcher_host) {
    IndexedDBDatabaseError error =
        CreateError(blink::mojom::IDBException::kUnknownError, "Unknown error",
                    transaction);
    std::move(callback).Run(
        blink::mojom::IDBDatabaseBatchGetAllResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  std::vector<std::vector<IndexedDBReturnValue>> all_found_values;
  std::unique_ptr<IndexedDBBackingStore::Cursor> cursor;

  size_t response_size = blink::mojom::kIDBMaxMessageOverhead;
  for (auto key_range : key_ranges) {
    std::vector<IndexedDBReturnValue> found_values;
    cursor.reset();

    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // Object Store: Value Retrieval Operation
      cursor = backing_store_->OpenObjectStoreCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          key_range, blink::mojom::IDBCursorDirection::Next, &s);
    } else {
      // Object Store: Referenced Value Retrieval Operation
      cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, key_range, blink::mojom::IDBCursorDirection::Next, &s);
    }

    if (!s.ok()) {
      DLOG(ERROR) << "Unable to open cursor operation: " << s.ToString();
      IndexedDBDatabaseError error =
          CreateError(blink::mojom::IDBException::kUnknownError,
                      "Corruption detected, unable to continue", transaction);
      std::move(callback).Run(
          blink::mojom::IDBDatabaseBatchGetAllResult::NewErrorResult(
              blink::mojom::IDBError::New(error.code(), error.message())));
      return s;
    }

    if (!cursor) {
      all_found_values.push_back(std::move(found_values));
      continue;
    }

    bool did_first_seek = false;
    bool generated_key = object_store_metadata.auto_increment &&
                         !object_store_metadata.key_path.IsNull();

    uint64_t num_found_items = 0;

    while (num_found_items++ < max_count) {
      bool cursor_valid;
      IndexedDBReturnValue return_value;

      if (did_first_seek) {
        cursor_valid = cursor->Continue(&s);
      } else {
        cursor_valid = cursor->FirstSeek(&s);
        did_first_seek = true;
      }
      if (!s.ok()) {
        IndexedDBDatabaseError error =
            CreateError(blink::mojom::IDBException::kUnknownError,
                        "Seek failure, unable to continue", transaction);
        std::move(callback).Run(
            blink::mojom::IDBDatabaseBatchGetAllResult::NewErrorResult(
                blink::mojom::IDBError::New(error.code(), error.message())));
        return s;
      }

      if (!cursor_valid)
        break;

      return_value.swap(*cursor->value());
      if (!return_value.empty() && generated_key) {
        return_value.primary_key = cursor->primary_key();
        return_value.key_path = object_store_metadata.key_path;
      }

      response_size += return_value.SizeEstimate();
      found_values.push_back(std::move(return_value));

      if (response_size > GetUsableMessageSizeInBytes()) {
        IndexedDBDatabaseError error =
            CreateError(blink::mojom::IDBException::kUnknownError,
                        "Maximum IPC message size exceeded.", transaction);
        std::move(callback).Run(
            blink::mojom::IDBDatabaseBatchGetAllResult::NewErrorResult(
                blink::mojom::IDBError::New(error.code(), error.message())));
        return s;
      }
    }
    all_found_values.push_back(std::move(found_values));
  }

  std::vector<std::vector<blink::mojom::IDBReturnValuePtr>> all_mojo_values;
  all_mojo_values.reserve(all_found_values.size());
  for (auto& found_values : all_found_values) {
    std::vector<blink::mojom::IDBReturnValuePtr> mojo_values;
    mojo_values.reserve(found_values.size());
    for (size_t j = 0; j < found_values.size(); ++j) {
      mojo_values.push_back(
          IndexedDBReturnValue::ConvertReturnValue(&found_values[j]));
      dispatcher_host->CreateAllExternalObjects(
          bucket_locator(), found_values[j].external_objects,
          &mojo_values[j]->value->external_objects);
    }
    all_mojo_values.push_back(std::move(mojo_values));
  }

  std::move(callback).Run(blink::mojom::IDBDatabaseBatchGetAllResult::NewValues(
      std::move(all_mojo_values)));
  return s;
}

Status IndexedDBDatabase::SetIndexesReadyOperation(
    size_t index_count,
    IndexedDBTransaction* transaction) {
  // TODO(dmurph): This method should be refactored out for something more
  // reliable.
  for (size_t i = 0; i < index_count; ++i)
    transaction->DidCompletePreemptiveEvent();
  return Status::OK();
}

Status IndexedDBDatabase::OpenCursorOperation(
    std::unique_ptr<OpenCursorOperationParams> params,
    const storage::BucketLocator& bucket_locator,
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    IndexedDBTransaction* transaction) {
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::OpenCursorOperation", "txn.id",
               transaction->id());

  Status s;
  if (!dispatcher_host) {
    IndexedDBDatabaseError error =
        CreateError(blink::mojom::IDBException::kUnknownError,
                    "Dispatcher not connected.", transaction);
    std::move(params->callback)
        .Run(blink::mojom::IDBDatabaseOpenCursorResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(params->object_store_id,
                                                params->index_id)) {
    return leveldb::Status::InvalidArgument(
        "Invalid object_store_id and/or index_id.");
  }

  // The frontend has begun indexing, so this pauses the transaction
  // until the indexing is complete. This can't happen any earlier
  // because we don't want to switch to early mode in case multiple
  // indexes are being created in a row, with Put()'s in between.
  if (params->task_type == blink::mojom::IDBTaskType::Preemptive)
    transaction->AddPreemptiveEvent();

  std::unique_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;
  if (params->index_id == IndexedDBIndexMetadata::kInvalidId) {
    if (params->cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      DCHECK_EQ(params->task_type, blink::mojom::IDBTaskType::Normal);
      backing_store_cursor = backing_store_->OpenObjectStoreKeyCursor(
          transaction->BackingStoreTransaction(), id(), params->object_store_id,
          *params->key_range, params->direction, &s);
    } else {
      backing_store_cursor = backing_store_->OpenObjectStoreCursor(
          transaction->BackingStoreTransaction(), id(), params->object_store_id,
          *params->key_range, params->direction, &s);
    }
  } else {
    DCHECK_EQ(params->task_type, blink::mojom::IDBTaskType::Normal);
    if (params->cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      backing_store_cursor = backing_store_->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(), id(), params->object_store_id,
          params->index_id, *params->key_range, params->direction, &s);
    } else {
      backing_store_cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(), id(), params->object_store_id,
          params->index_id, *params->key_range, params->direction, &s);
    }
  }

  if (!s.ok()) {
    DLOG(ERROR) << "Unable to open cursor operation: " << s.ToString();
    return s;
  }

  if (!backing_store_cursor) {
    // Occurs when we've reached the end of cursor's data.
    std::move(params->callback)
        .Run(blink::mojom::IDBDatabaseOpenCursorResult::NewEmpty(true));
    return s;
  }

  std::unique_ptr<IndexedDBCursor> cursor = std::make_unique<IndexedDBCursor>(
      std::move(backing_store_cursor), params->cursor_type, params->task_type,
      transaction->AsWeakPtr());
  IndexedDBCursor* cursor_ptr = cursor.get();
  transaction->RegisterOpenCursor(cursor_ptr);

  blink::mojom::IDBValuePtr mojo_value;
  std::vector<IndexedDBExternalObject> external_objects;
  if (cursor_ptr->Value()) {
    mojo_value = IndexedDBValue::ConvertAndEraseValue(cursor_ptr->Value());
    external_objects.swap(cursor_ptr->Value()->external_objects);
  }

  if (mojo_value) {
    dispatcher_host->CreateAllExternalObjects(bucket_locator, external_objects,
                                              &mojo_value->external_objects);
  }

  std::move(params->callback)
      .Run(blink::mojom::IDBDatabaseOpenCursorResult::NewValue(
          blink::mojom::IDBDatabaseOpenCursorValue::New(
              dispatcher_host->CreateCursorBinding(bucket_locator,
                                                   std::move(cursor)),
              cursor_ptr->key(), cursor_ptr->primary_key(),
              std::move(mojo_value))));
  return s;
}

Status IndexedDBDatabase::CountOperation(
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::CountOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(object_store_id, index_id))
    return leveldb::Status::InvalidArgument(
        "Invalid object_store_id and/or index_id.");

  uint32_t count = 0;
  std::unique_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;

  Status s = Status::OK();
  if (index_id == IndexedDBIndexMetadata::kInvalidId) {
    backing_store_cursor = backing_store_->OpenObjectStoreKeyCursor(
        transaction->BackingStoreTransaction(), id(), object_store_id,
        *key_range, blink::mojom::IDBCursorDirection::Next, &s);
  } else {
    backing_store_cursor = backing_store_->OpenIndexKeyCursor(
        transaction->BackingStoreTransaction(), id(), object_store_id, index_id,
        *key_range, blink::mojom::IDBCursorDirection::Next, &s);
  }
  if (!s.ok()) {
    DLOG(ERROR) << "Unable perform count operation: " << s.ToString();
    return s;
  }
  if (!backing_store_cursor) {
    callbacks->OnSuccess(count);
    return s;
  }

  do {
    if (!s.ok())
      return s;
    ++count;
  } while (backing_store_cursor->Continue(&s));

  callbacks->OnSuccess(count);
  return s;
}

Status IndexedDBDatabase::DeleteRangeOperation(
    int64_t object_store_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::DeleteRangeOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdInMetadata(object_store_id))
    return leveldb::Status::InvalidArgument("Invalid object_store_id.");

  Status s = backing_store_->DeleteRange(transaction->BackingStoreTransaction(),
                                         id(), object_store_id, *key_range);
  if (!s.ok())
    return s;
  callbacks->OnSuccess();
  factory_->NotifyIndexedDBContentChanged(
      bucket_locator(), metadata_.name,
      metadata_.object_stores[object_store_id].name);
  return s;
}

Status IndexedDBDatabase::GetKeyGeneratorCurrentNumberOperation(
    int64_t object_store_id,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  if (!IsObjectStoreIdInMetadata(object_store_id)) {
    callbacks->OnError(CreateError(blink::mojom::IDBException::kDataError,
                                   "Object store id not valid.", transaction));
    return leveldb::Status::InvalidArgument("Invalid object_store_id.");
  }

  int64_t current_number;
  Status s = backing_store_->GetKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(), id(), object_store_id,
      &current_number);
  if (!s.ok()) {
    callbacks->OnError(CreateError(
        blink::mojom::IDBException::kDataError,
        "Failed to get the current number of key generator.", transaction));
    return s;
  }
  callbacks->OnSuccess(current_number);
  return s;
}

Status IndexedDBDatabase::ClearOperation(
    int64_t object_store_id,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  TRACE_EVENT1("IndexedDB", "IndexedDBDatabase::ClearOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdInMetadata(object_store_id))
    return leveldb::Status::InvalidArgument("Invalid object_store_id.");

  Status s = backing_store_->ClearObjectStore(
      transaction->BackingStoreTransaction(), id(), object_store_id);
  if (!s.ok())
    return s;
  callbacks->OnSuccess();

  factory_->NotifyIndexedDBContentChanged(
      bucket_locator(), metadata_.name,
      metadata_.object_stores[object_store_id].name);
  return s;
}

bool IndexedDBDatabase::IsObjectStoreIdInMetadata(
    int64_t object_store_id) const {
  if (!base::Contains(metadata_.object_stores, object_store_id)) {
    DLOG(ERROR) << "Invalid object_store_id";
    return false;
  }
  return true;
}

bool IndexedDBDatabase::IsObjectStoreIdAndIndexIdInMetadata(
    int64_t object_store_id,
    int64_t index_id) const {
  if (!IsObjectStoreIdInMetadata(object_store_id))
    return false;
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (!base::Contains(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

bool IndexedDBDatabase::IsObjectStoreIdAndMaybeIndexIdInMetadata(
    int64_t object_store_id,
    int64_t index_id) const {
  if (!IsObjectStoreIdInMetadata(object_store_id))
    return false;
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (index_id != IndexedDBIndexMetadata::kInvalidId &&
      !base::Contains(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

bool IndexedDBDatabase::IsObjectStoreIdInMetadataAndIndexNotInMetadata(
    int64_t object_store_id,
    int64_t index_id) const {
  if (!IsObjectStoreIdInMetadata(object_store_id))
    return false;
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (base::Contains(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

// kIDBMaxMessageSize is defined based on the original
// IPC::Channel::kMaximumMessageSize value.  We use kIDBMaxMessageSize to limit
// the size of arguments we pass into our Mojo calls.  We want to ensure this
// value is always no bigger than the current kMaximumMessageSize value which
// also ensures it is always no bigger than the current Mojo message size limit.
static_assert(
    blink::mojom::kIDBMaxMessageSize <= IPC::Channel::kMaximumMessageSize,
    "kIDBMaxMessageSize is bigger than IPC::Channel::kMaximumMessageSize");

size_t IndexedDBDatabase::GetUsableMessageSizeInBytes() const {
  return blink::mojom::kIDBMaxMessageSize -
         blink::mojom::kIDBMaxMessageOverhead;
}

void IndexedDBDatabase::CallUpgradeTransactionStartedForTesting(
    int64_t old_version) {
  connection_coordinator_.OnUpgradeTransactionStarted(old_version);
}

Status IndexedDBDatabase::OpenInternal() {
  bool found = false;
  Status s = metadata_coding_->ReadMetadataForDatabaseName(
      backing_store_->db(), backing_store_->origin_identifier(), metadata_.name,
      &metadata_, &found);
  DCHECK(found == (metadata_.id != kInvalidId))
      << "found = " << found << " id = " << metadata_.id;
  if (!s.ok() || found)
    return s;

  return metadata_coding_->CreateDatabase(
      backing_store_->db(), backing_store_->origin_identifier(), metadata_.name,
      metadata_.version, &metadata_);
}

std::unique_ptr<IndexedDBConnection> IndexedDBDatabase::CreateConnection(
    IndexedDBBucketStateHandle bucket_state_handle,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    scoped_refptr<IndexedDBClientStateCheckerWrapper> client_state_checker) {
  std::unique_ptr<IndexedDBConnection> connection =
      std::make_unique<IndexedDBConnection>(
          std::move(bucket_state_handle), class_factory_,
          weak_factory_.GetWeakPtr(),
          base::BindRepeating(&IndexedDBDatabase::VersionChangeIgnored,
                              weak_factory_.GetWeakPtr()),
          base::BindOnce(&IndexedDBDatabase::ConnectionClosed,
                         weak_factory_.GetWeakPtr()),
          database_callbacks, std::move(client_state_checker));
  connections_.insert(connection.get());
  return connection;
}

void IndexedDBDatabase::VersionChangeIgnored() {
  connection_coordinator_.OnVersionChangeIgnored();
}

bool IndexedDBDatabase::HasNoConnections() const {
  return force_closing_ || connections().empty();
}

void IndexedDBDatabase::SendVersionChangeToAllConnections(int64_t old_version,
                                                          int64_t new_version) {
  if (force_closing_)
    return;
  for (auto* connection : connections()) {
    // Before invoking this method, the `IndexedDBConnectionCoordinator` had set
    // the request state to `kPendingNoConnections`. Now the request will be
    // blocked until all the existing connections to this database is closed.
    // There are three possible ways for the connection to be closed:
    // 1. If the client is already pending close, then the `VersionChange` event
    // will be ignored and the open request will be deemed blocked until the
    // pending close completes.
    // 2. If the client is active, the `VersionChange` event will be enqueued
    // and the registered event listener will be fired asynchronously. The event
    // listener should be responsible for actively closing the IndexedDB
    // connection. The document won't be eligible for BFCache before the
    // connection is closed if it receives the `versionchange` event.
    // 3. While the above two cases rely on the `VersionChange` event to be
    // delivered to the renderer process, the third case happens purely from the
    // IndexedDB/browser context. If the client is inactive, the `VersionChange`
    // event will not be delivered, instead, a mojo call is sent to the browser
    // process to disallow the activation of the inactive client, which will
    // close the connection as part of the destruction.
    // No matter which path it follows, the `SendVersionChangeToAllConnections`
    // method is executed asynchronously.
    if (base::FeatureList::IsEnabled(
            blink::features::kAllowPageWithIDBConnectionInBFCache)) {
      connection->DisallowInactiveClient(
          storage::mojom::DisallowInactiveClientReason::kClientEventIsTriggered,
          base::BindOnce(
              [](base::WeakPtr<IndexedDBConnection> connection,
                 int64_t old_version, int64_t new_version,
                 bool was_client_active) {
                if (connection && connection->IsConnected() &&
                    was_client_active) {
                  connection->callbacks()->OnVersionChange(old_version,
                                                           new_version);
                }
              },
              connection->GetWeakPtr(), old_version, new_version));
    } else {
      connection->callbacks()->OnVersionChange(old_version, new_version);
    }
  }
}

void IndexedDBDatabase::ConnectionClosed(IndexedDBConnection* connection) {
  TRACE_EVENT0("IndexedDB", "IndexedDBDatabase::ConnectionClosed");
  // Ignore connection closes during force close to prevent re-entry.
  if (force_closing_)
    return;
  connections_.erase(connection);
  connection_coordinator_.OnConnectionClosed(connection);
  if (connections_.empty())
    connection_coordinator_.OnNoConnections();
  if (CanBeDestroyed())
    tasks_available_callback_.Run();
}

bool IndexedDBDatabase::CanBeDestroyed() {
  return !connection_coordinator_.HasTasks() && connections_.empty();
}

bool IndexedDBDatabase::IsTransactionBlockingOthers(
    IndexedDBTransaction* transaction) const {
  base::flat_set<PartitionedLockId> lock_ids = transaction->lock_ids();
  for (const auto& lock_id : lock_ids) {
    if (lock_manager_->GetQueuedLockRequestCount(lock_id) > 0) {
      return true;
    }
  }

  return false;
}

}  // namespace content
