// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/connection_coordinator.h"

#include <atomic>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/callback_tags.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/bucket_context_handle.h"
#include "content/browser/indexed_db/instance/callback_helpers.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/database.h"
#include "content/browser/indexed_db/instance/database_callbacks.h"
#include "content/browser/indexed_db/instance/factory_client.h"
#include "content/browser/indexed_db/instance/lock_request_data.h"
#include "content/browser/indexed_db/instance/pending_connection.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "content/browser/indexed_db/list_set.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-shared.h"
#include "third_party/leveldatabase/env_chromium.h"

using base::NumberToString16;
using blink::IndexedDBDatabaseMetadata;

namespace content::indexed_db {
namespace {
enum class RequestState {
  kNotStarted,
  kPendingNoConnections,
  kPendingLocks,
  kPendingTransactionComplete,
  kError,
  kDone,
};
}  // namespace

// This represents what script calls an 'IDBOpenDBRequest' - either a database
// open or delete call. These may be blocked on other connections. After every
// callback, the request must call
// ConnectionCoordinator::RequestComplete() or be expecting a further
// callback.
class ConnectionCoordinator::ConnectionRequest {
 public:
  ConnectionRequest(BucketContext& bucket_context,
                    Database* db,
                    ConnectionCoordinator* connection_coordinator)
      : bucket_context_handle_(bucket_context),
        db_(db),
        connection_coordinator_(connection_coordinator),
        tasks_available_callback_(
            base::BindRepeating(&BucketContext::QueueRunTasks,
                                bucket_context.AsWeakPtr())) {}

  ConnectionRequest(const ConnectionRequest&) = delete;
  ConnectionRequest& operator=(const ConnectionRequest&) = delete;

  virtual ~ConnectionRequest() {}

  // Called when the request makes it to the front of the queue. The state()
  // will be checked after this call, so there is no need to run the
  // `tasks_available_callback_`.
  virtual void Perform(bool has_connections) = 0;

  // Called if a front-end signals that it is ignoring a "versionchange"
  // event. This should result in firing a "blocked" event at the request.
  virtual void OnVersionChangeIgnored() const = 0;

  // Called when a connection is closed; if it corresponds to this connection,
  // need to do cleanup.
  // Not called during a force close.
  virtual void OnConnectionClosed(Connection* connection) = 0;

  // Called when there are no connections to the database.
  virtual void OnNoConnections() = 0;

  // Called when the transaction should be bound.
  virtual void BindTransactionReceiver() = 0;

  // Called when the upgrade transaction has started executing.
  virtual void UpgradeTransactionStarted(int64_t old_version) = 0;

  // Called when the upgrade transaction has finished.
  virtual void UpgradeTransactionFinished(bool committed) = 0;

  // Called on all pending tasks during a force close. Returns if the task
  // should be pruned (removed) from the task queue during the force close.
  virtual bool ShouldPruneForForceClose() = 0;

  RequestState state() const { return state_; }

  // Relevant if state() is kError.
  Status status() const { return saved_status_; }

 protected:
  void ContinueAfterAcquiringLocks(base::OnceClosure next_step) {
    if (!lock_receiver_.locks.empty()) {
      std::move(next_step).Run();
      return;
    }

    std::vector<PartitionedLockManager::PartitionedLockRequest> lock_requests =
        {{GetDatabaseLockId(db_->metadata().name),
          PartitionedLockManager::LockType::kExclusive}};
    state_ = RequestState::kPendingLocks;

    db_->lock_manager().AcquireLocks(
        std::move(lock_requests), lock_receiver_, std::move(next_step),
        base::BindRepeating(&Connection::HasHigherPriorityThan,
                            &lock_receiver_));
  }

  RequestState state_ = RequestState::kNotStarted;

  BucketContextHandle bucket_context_handle_;
  // This is safe because Database owns this object.
  raw_ptr<Database> db_;

  // Rawptr safe because ConnectionCoordinator owns this object.
  raw_ptr<ConnectionCoordinator> connection_coordinator_;

  base::RepeatingClosure tasks_available_callback_;

  Status saved_status_;

  PartitionedLockHolder lock_receiver_;
};

class ConnectionCoordinator::OpenRequest
    : public ConnectionCoordinator::ConnectionRequest {
 public:
  OpenRequest(BucketContext& bucket_context,
              Database* db,
              std::unique_ptr<PendingConnection> pending_connection,
              ConnectionCoordinator* connection_coordinator)
      : ConnectionRequest(bucket_context, db, connection_coordinator),
        pending_(std::move(pending_connection)) {
    db_->metadata_.was_cold_open = pending_->was_cold_open;

    // Note that the `scheduling_priority` on this lock receiver isn't very
    // important because locks are only acquired when upgrading the version, and
    // that requires that all other connections be closed. So there shouldn't be
    // a queue of outstanding lock requests to contend with.
    lock_receiver_.SetUserData(
        LockRequestData::kKey,
        std::make_unique<LockRequestData>(pending_->client_token,
                                          /*scheduling_priority=*/0));
  }

  OpenRequest(const OpenRequest&) = delete;
  OpenRequest& operator=(const OpenRequest&) = delete;

  void Perform(bool has_connections) override {
    // Since `state_` is checked after the call to `Perform()`, temporarily make
    // `tasks_available_callback_` a no-op.
    base::AutoReset suspend_callback(&tasks_available_callback_,
                                     base::DoNothing());

    // If the metadata is in an uninitialized state, that means one of two
    // things:
    //
    // 1. The `Database` was just constructed, or
    // 2. The database was deleted and a new one was created with the same name
    //    within the lifespan of a single `Database`. Then the metadata
    //    must have been reset in `DeleteRequest::DoDelete`. `InitDatabase` will
    //    create the record for the database in the backing store and fill in
    //    the new metadata.
    //
    // Initialization of the metadata occurs here because it requires a lock. If
    // metadata is read from the database without a lock, then we may get a
    // stale version. See crbug.com/1472028
    if (db_->metadata().id == kInvalidDatabaseId) {
      ContinueAfterAcquiringLocks(
          base::BindOnce(&ConnectionCoordinator::OpenRequest::InitDatabase,
                         weak_factory_.GetWeakPtr(), has_connections));
      return;
    }

    ContinueOpening(has_connections);
  }

  void InitDatabase(bool has_connections) {
    saved_status_ = db_->OpenInternal();
    if (!saved_status_.ok()) {
      // TODO(jsbell): Consider including sanitized leveldb status message.
      std::u16string message;
      if (pending_->version == IndexedDBDatabaseMetadata::NO_VERSION) {
        message = u"Internal error opening database with no version specified.";
      } else {
        message = u"Internal error opening database with version " +
                  NumberToString16(pending_->version);
      }
      pending_->factory_client->OnError(
          DatabaseError(blink::mojom::IDBException::kUnknownError, message));
      state_ = RequestState::kError;
      tasks_available_callback_.Run();
      return;
    }

    ContinueOpening(has_connections);
  }

  void ContinueOpening(bool has_connections) {
    base::ScopedClosureRunner scoped_tasks_available(tasks_available_callback_);
    const int64_t old_version = db_->metadata().version;
    int64_t& new_version = pending_->version;

    bool is_new_database = old_version == IndexedDBDatabaseMetadata::NO_VERSION;

    if (new_version == IndexedDBDatabaseMetadata::DEFAULT_VERSION) {
      // For unit tests only - skip upgrade steps. (Calling from script with
      // DEFAULT_VERSION throws exception.)
      DCHECK(is_new_database);
      pending_->factory_client->OnOpenSuccess(
          db_->CreateConnection(std::move(pending_->database_callbacks),
                                std::move(pending_->client_state_checker),
                                pending_->client_token,
                                pending_->scheduling_priority),
          db_->metadata_);
      bucket_context_handle_.Release();
      state_ = RequestState::kDone;
      return;
    }

    if (!is_new_database &&
        (new_version == old_version ||
         new_version == IndexedDBDatabaseMetadata::NO_VERSION)) {
      pending_->factory_client->OnOpenSuccess(
          db_->CreateConnection(std::move(pending_->database_callbacks),
                                std::move(pending_->client_state_checker),
                                pending_->client_token,
                                pending_->scheduling_priority),
          db_->metadata_);
      state_ = RequestState::kDone;
      bucket_context_handle_.Release();
      return;
    }

    if (new_version == IndexedDBDatabaseMetadata::NO_VERSION) {
      // If no version is specified and no database exists, upgrade the
      // database version to 1.
      DCHECK(is_new_database);
      new_version = 1;
    } else if (new_version < old_version) {
      // Requested version is lower than current version - fail the request.
      DCHECK(!is_new_database);
      pending_->factory_client->OnError(DatabaseError(
          blink::mojom::IDBException::kVersionError,
          u"The requested version (" + NumberToString16(pending_->version) +
              u") is less than the existing version (" +
              NumberToString16(db_->metadata_.version) + u")."));
      state_ = RequestState::kDone;
      return;
    }

    // Requested version is higher than current version - upgrade needed.
    DCHECK_GT(new_version, old_version);

    if (!has_connections) {
      OnNoConnections();
      return;
    }

    // There are outstanding connections - fire "versionchange" events and
    // wait for the connections to close. Front end ensures the event is not
    // fired at connections that have close_pending set. A "blocked" event
    // will be fired at the request when one of the connections acks that the
    // "versionchange" event was ignored.
    DCHECK_NE(pending_->data_loss_info.status,
              blink::mojom::IDBDataLoss::Total);
    state_ = RequestState::kPendingNoConnections;
    db_->SendVersionChangeToAllConnections(old_version, new_version);

    // When all connections have closed the upgrade can proceed.
  }

  void OnVersionChangeIgnored() const override {
    DCHECK(state_ == RequestState::kPendingNoConnections);
    pending_->factory_client->OnBlocked(db_->metadata_.version);
  }

  void OnConnectionClosed(Connection* connection) override {
    // This connection closed prematurely; signal an error and complete.
    if (connection == connection_ptr_for_close_comparision_) {
      connection_ptr_for_close_comparision_ = nullptr;
      if (!pending_->factory_client->is_complete()) {
        pending_->factory_client->OnError(
            DatabaseError(blink::mojom::IDBException::kAbortError,
                          "The connection was closed."));
      }
      state_ = RequestState::kDone;
      tasks_available_callback_.Run();
      return;
    }
  }

  void OnNoConnections() override {
    ContinueAfterAcquiringLocks(
        base::BindOnce(&ConnectionCoordinator::OpenRequest::StartUpgrade,
                       weak_factory_.GetWeakPtr()));
  }

  // Initiate the upgrade. The bulk of the work actually happens in
  // VersionChangeOperation in order to kick the transaction into the correct
  // state.
  void StartUpgrade() {
    DCHECK(state_ == RequestState::kPendingLocks);

    DCHECK(!lock_receiver_.locks.empty());
    connection_ = db_->CreateConnection(
        std::move(pending_->database_callbacks),
        std::move(pending_->client_state_checker), pending_->client_token,
        pending_->scheduling_priority);
    bucket_context_handle_.Release();
    DCHECK(!connection_ptr_for_close_comparision_);
    connection_ptr_for_close_comparision_ = connection_.get();
    DCHECK_EQ(db_->connections().count(connection_.get()), 1UL);

    std::vector<int64_t> object_store_ids;

    state_ = RequestState::kPendingTransactionComplete;
    Transaction* transaction = connection_->CreateVersionChangeTransaction(
        pending_->transaction_id,
        std::set<int64_t>(object_store_ids.begin(), object_store_ids.end()),
        db_->backing_store()
            ->CreateTransaction(blink::mojom::IDBTransactionDurability::Strict,
                                blink::mojom::IDBTransactionMode::ReadWrite)
            .release());

    // Save a WeakPtr<Transaction> for the BindTransactionReceiver
    // function to use later.
    pending_->transaction = transaction->AsWeakPtr();

    transaction->ScheduleTask(
        BindWeakOperation(&Database::VersionChangeOperation, db_->AsWeakPtr(),
                          pending_->version));
    transaction->mutable_locks_receiver()->locks =
        std::move(lock_receiver_.locks);
    transaction->Start();
  }

  void BindTransactionReceiver() override {
    if (pending_->transaction) {
      pending_->transaction->BindReceiver(
          std::move(pending_->pending_mojo_receiver));
    }
  }

  // Called when the upgrade transaction has started executing.
  void UpgradeTransactionStarted(int64_t old_version) override {
    DCHECK(state_ == RequestState::kPendingTransactionComplete);
    DCHECK(connection_);
    pending_->factory_client->OnUpgradeNeeded(
        old_version, std::move(connection_), db_->metadata_,
        pending_->data_loss_info);
  }

  void UpgradeTransactionFinished(bool committed) override {
    DCHECK(state_ == RequestState::kPendingTransactionComplete);
    // Ownership of connection was already passed along in OnUpgradeNeeded.
    if (committed) {
      DCHECK_EQ(pending_->version, db_->metadata_.version);
      pending_->factory_client->OnOpenSuccess(nullptr, db_->metadata());
    } else {
      DCHECK_NE(pending_->version, db_->metadata_.version);
      pending_->factory_client->OnError(
          DatabaseError(blink::mojom::IDBException::kAbortError,
                        "Version change transaction was aborted in "
                        "upgradeneeded event handler."));
    }
    state_ = RequestState::kDone;
    tasks_available_callback_.Run();
  }

  bool ShouldPruneForForceClose() override {
    DCHECK(pending_);
    if (!pending_->factory_client->is_complete()) {
      pending_->factory_client->OnError(
          DatabaseError(blink::mojom::IDBException::kAbortError,
                        "The connection was closed."));
    }
    if (state_ != RequestState::kError) {
      state_ = RequestState::kDone;
    }

    if (connection_) {
      // CloseAndReportForceClose calls OnForcedClose on the database callbacks,
      // so we don't need to.
      connection_->CloseAndReportForceClose();
      connection_.reset();
    } else if (pending_->database_callbacks) {
      pending_->database_callbacks->OnForcedClose();
    }
    // else: `database_callbacks` has been passed to `connection_`, in which
    // case the Database will have called `CloseAndReportForceClose()`.

    pending_.reset();
    // The tasks_available_callback_ is NOT run here, because we are assuming
    // the caller is doing their own cleanup & execution for ForceClose.
    return true;
  }

 private:
  std::unique_ptr<PendingConnection> pending_;

  // If an upgrade is needed, holds the pending connection until ownership is
  // transferred to the IndexedDBDispatcherHost via OnUpgradeNeeded.
  std::unique_ptr<Connection> connection_;

  // This raw pointer is stored solely for comparison to the connection in
  // OnConnectionClosed. It is not guaranteed to be pointing to a live object.
  raw_ptr<Connection> connection_ptr_for_close_comparision_ = nullptr;

  base::WeakPtrFactory<OpenRequest> weak_factory_{this};
};

class ConnectionCoordinator::DeleteRequest
    : public ConnectionCoordinator::ConnectionRequest {
 public:
  DeleteRequest(BucketContext& bucket_context,
                Database* db,
                std::unique_ptr<FactoryClient> factory_client,
                base::OnceClosure on_database_deleted,
                ConnectionCoordinator* connection_coordinator)
      : ConnectionRequest(bucket_context, db, connection_coordinator),
        factory_client_(std::move(factory_client)),
        on_database_deleted_(std::move(on_database_deleted)) {}

  DeleteRequest(const DeleteRequest&) = delete;
  DeleteRequest& operator=(const DeleteRequest&) = delete;

  void Perform(bool has_connections) override {
    // Since `state_` is checked after the call to `Perform()`, temporarily make
    // `tasks_available_callback_` a no-op.
    base::AutoReset suspend_callback(&tasks_available_callback_,
                                     base::DoNothing());

    if (db_->metadata().id == kInvalidDatabaseId) {
      ContinueAfterAcquiringLocks(
          base::BindOnce(&ConnectionCoordinator::DeleteRequest::InitDatabase,
                         weak_factory_.GetWeakPtr(), has_connections));
      return;
    }
    ContinueDeleting(has_connections);
  }

  void InitDatabase(bool has_connections) {
    base::ScopedClosureRunner scoped_tasks_available(tasks_available_callback_);
    saved_status_ = db_->OpenInternal();
    if (!saved_status_.ok()) {
      DatabaseError error(blink::mojom::IDBException::kUnknownError,
                          u"Internal error creating database backend "
                          u"for indexedDB.deleteDatabase.");
      factory_client_->OnError(error);
      state_ = RequestState::kError;
      return;
    }

    ContinueDeleting(has_connections);
  }

  void ContinueDeleting(bool has_connections) {
    if (!has_connections) {
      OnNoConnections();
      return;
    }

    // Front end ensures the event is not fired at connections that have
    // close_pending set.
    const int64_t old_version = db_->metadata().version;
    const int64_t new_version = IndexedDBDatabaseMetadata::NO_VERSION;
    state_ = RequestState::kPendingNoConnections;
    db_->SendVersionChangeToAllConnections(old_version, new_version);
  }

  void OnVersionChangeIgnored() const override {
    DCHECK(state_ == RequestState::kPendingNoConnections);
    factory_client_->OnBlocked(db_->metadata_.version);
  }

  void OnConnectionClosed(Connection* connection) override {}

  void OnNoConnections() override {
    ContinueAfterAcquiringLocks(
        base::BindOnce(&ConnectionCoordinator::DeleteRequest::DoDelete,
                       weak_factory_.GetWeakPtr()));
  }

  void DoDelete() {
    state_ = RequestState::kPendingTransactionComplete;
    UMA_HISTOGRAM_ENUMERATION(
        indexed_db::kBackingStoreActionUmaName,
        indexed_db::IndexedDBAction::kDatabaseDeleteAttempt);
    // This is used to check if this class is still alive after the destruction
    // of the backing store, which can synchronously cause the system to be shut
    // down if the disk is really bad.
    base::WeakPtr<DeleteRequest> weak_ptr = weak_factory_.GetWeakPtr();
    if (db_->backing_store()) {
      saved_status_ = db_->backing_store()->DeleteDatabase(
          db_->metadata_.name, std::move(lock_receiver_.locks),
          std::move(on_database_deleted_));
      base::UmaHistogramEnumeration(
          "WebCore.IndexedDB.BackingStore.DeleteDatabaseStatus",
          leveldb_env::GetLevelDBStatusUMAValue(saved_status_.leveldb_status()),
          leveldb_env::LEVELDB_STATUS_MAX);
    }

    if (!weak_ptr) {
      return;
    }

    base::ScopedClosureRunner scoped_tasks_available(tasks_available_callback_);
    if (!saved_status_.ok()) {
      // TODO(jsbell): Consider including sanitized leveldb status message.
      DatabaseError error(blink::mojom::IDBException::kUnknownError,
                          "Internal error deleting database.");
      factory_client_->OnError(error);
      state_ = RequestState::kError;
      return;
    }

    int64_t old_version = db_->metadata_.version;
    db_->metadata_.id = kInvalidDatabaseId;
    db_->metadata_.version = IndexedDBDatabaseMetadata::NO_VERSION;
    db_->metadata_.max_object_store_id = 0;
    db_->metadata_.object_stores.clear();
    // Unittests (specifically the Database unittests) can have the
    // backing store be a nullptr, so report deleted here.
    if (on_database_deleted_) {
      std::move(on_database_deleted_).Run();
    }
    factory_client_->OnDeleteSuccess(old_version);

    state_ = RequestState::kDone;
  }

  void BindTransactionReceiver() override { NOTREACHED_IN_MIGRATION(); }

  void UpgradeTransactionStarted(int64_t old_version) override {
    NOTREACHED_IN_MIGRATION();
  }

  void UpgradeTransactionFinished(bool committed) override {
    NOTREACHED_IN_MIGRATION();
  }

  // The delete requests should always be run during force close.
  bool ShouldPruneForForceClose() override { return false; }

 private:
  std::unique_ptr<FactoryClient> factory_client_;
  base::OnceClosure on_database_deleted_;

  base::WeakPtrFactory<DeleteRequest> weak_factory_{this};
};

ConnectionCoordinator::ConnectionCoordinator(Database* db,
                                             BucketContext& bucket_context)
    : db_(db), bucket_context_(bucket_context) {}
ConnectionCoordinator::~ConnectionCoordinator() = default;

void ConnectionCoordinator::ScheduleOpenConnection(
    std::unique_ptr<PendingConnection> connection) {
  request_queue_.push(std::make_unique<OpenRequest>(
      *bucket_context_, db_, std::move(connection), this));
  bucket_context_->QueueRunTasks();
}

void ConnectionCoordinator::ScheduleDeleteDatabase(
    std::unique_ptr<FactoryClient> factory_client,
    base::OnceClosure on_deletion_complete) {
  request_queue_.push(std::make_unique<DeleteRequest>(
      *bucket_context_, db_, std::move(factory_client),
      std::move(on_deletion_complete), this));
  bucket_context_->QueueRunTasks();
}

Status ConnectionCoordinator::PruneTasksForForceClose() {
  // Remove all pending requests that don't want to execute during force close
  // (open requests).
  base::queue<std::unique_ptr<ConnectionRequest>> requests_to_still_run;
  Status last_error;
  while (!request_queue_.empty()) {
    std::unique_ptr<ConnectionRequest> request =
        std::move(request_queue_.front());
    request_queue_.pop();
    Status old_error = request->status();

    if (request->ShouldPruneForForceClose()) {
      if (!old_error.ok()) {
        last_error = old_error;
      }
      request.reset();
    } else {
      requests_to_still_run.push(std::move(request));
    }
  }

  request_queue_ = std::move(requests_to_still_run);
  return last_error;
}

void ConnectionCoordinator::OnConnectionClosed(Connection* connection) {
  DCHECK(connection->database().get() == db_);

  if (!request_queue_.empty()) {
    request_queue_.front()->OnConnectionClosed(connection);
  }
}

void ConnectionCoordinator::OnNoConnections() {
  if (request_queue_.empty() ||
      request_queue_.front()->state() != RequestState::kPendingNoConnections) {
    return;
  }
  request_queue_.front()->OnNoConnections();
}

// TODO(dmurph): Attach an ID to the connection change events to prevent
// mis-propogation to the wrong connection request.
void ConnectionCoordinator::OnVersionChangeIgnored() {
  if (request_queue_.empty() ||
      request_queue_.front()->state() != RequestState::kPendingNoConnections) {
    return;
  }
  request_queue_.front()->OnVersionChangeIgnored();
}

void ConnectionCoordinator::OnUpgradeTransactionStarted(int64_t old_version) {
  if (request_queue_.empty() || request_queue_.front()->state() !=
                                    RequestState::kPendingTransactionComplete) {
    return;
  }
  request_queue_.front()->UpgradeTransactionStarted(old_version);
}

void ConnectionCoordinator::BindVersionChangeTransactionReceiver() {
  if (request_queue_.empty() || request_queue_.front()->state() !=
                                    RequestState::kPendingTransactionComplete) {
    return;
  }
  request_queue_.front()->BindTransactionReceiver();
}

void ConnectionCoordinator::OnUpgradeTransactionFinished(bool committed) {
  if (request_queue_.empty() || request_queue_.front()->state() !=
                                    RequestState::kPendingTransactionComplete) {
    return;
  }
  request_queue_.front()->UpgradeTransactionFinished(committed);
}

std::tuple<ConnectionCoordinator::ExecuteTaskResult, Status>
ConnectionCoordinator::ExecuteTask(bool has_connections) {
  if (request_queue_.empty()) {
    return {ExecuteTaskResult::kDone, Status()};
  }

  auto& request = request_queue_.front();
  if (request->state() == RequestState::kNotStarted) {
    request->Perform(has_connections);
    DCHECK(request->state() != RequestState::kNotStarted);
  }

  switch (request->state()) {
    case RequestState::kNotStarted:
      NOTREACHED_IN_MIGRATION();
      return {ExecuteTaskResult::kError, Status::OK()};
    case RequestState::kPendingNoConnections:
    case RequestState::kPendingLocks:
    case RequestState::kPendingTransactionComplete:
      return {ExecuteTaskResult::kPendingAsyncWork, Status()};
    case RequestState::kDone: {
      // Move `request_to_discard` out of `request_queue_` then
      // `request_queue_.pop()`. We do this because `request_to_discard`'s dtor
      // calls OnConnectionClosed and OnNoConnections, which interact with
      // `request_queue_` assuming the queue no longer holds
      // `request_to_discard`.
      auto request_to_discard = std::move(request_queue_.front());
      request_queue_.pop();
      request_to_discard.reset();
      return {request_queue_.empty() ? ExecuteTaskResult::kDone
                                     : ExecuteTaskResult::kMoreTasks,
              Status::OK()};
    }
    case RequestState::kError: {
      Status status = request->status();
      // Move `request_to_discard` out of `request_queue_` then
      // `request_queue_.pop()`. We do this because `request_to_discard`'s dtor
      // calls OnConnectionClosed and OnNoConnections, which interact with
      // `request_queue_` assuming the queue no longer holds
      // `request_to_discard`.
      auto request_to_discard = std::move(request_queue_.front());
      request_queue_.pop();
      request_to_discard.reset();
      return {ExecuteTaskResult::kError, status};
    }
  }
  NOTREACHED_IN_MIGRATION();
}

size_t ConnectionCoordinator::ActiveOpenDeleteCount() const {
  return request_queue_.empty() ? 0 : 1;
}

// Number of open/delete calls that are waiting their turn.
size_t ConnectionCoordinator::PendingOpenDeleteCount() const {
  if (request_queue_.empty()) {
    return 0;
  }
  if (request_queue_.front()->state() == RequestState::kNotStarted) {
    return request_queue_.size();
  }
  return request_queue_.size() - 1;
}

}  // namespace content::indexed_db
