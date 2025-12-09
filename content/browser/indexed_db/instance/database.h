// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_DATABASE_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_DATABASE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/connection_coordinator.h"
#include "content/browser/indexed_db/instance/factory_client.h"
#include "content/browser/indexed_db/instance/pending_connection.h"
#include "content/browser/indexed_db/list_set.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace blink {
class IndexedDBKeyRange;
struct IndexedDBDatabaseMetadata;
}  // namespace blink

namespace content::indexed_db {
class BucketContext;
class Connection;
class DatabaseCallbacks;
class Transaction;
enum class CursorType;

// This class maps to a single IDB database:
// https://www.w3.org/TR/IndexedDB/#database
//
// It is created and operated on a bucket thread.
class CONTENT_EXPORT Database {
 public:
  // Used to report irrecoverable backend errors. The second argument can be
  // null.
  using ErrorCallback = base::RepeatingCallback<void(Status, const char*)>;

  static const int64_t kMinimumIndexId = 30;

  Database(const std::u16string& name, BucketContext& bucket_context);

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  virtual ~Database();

  BackingStore* backing_store();
  BackingStore::Database* backing_store_db() { return backing_store_db_.get(); }
  PartitionedLockManager& lock_manager();

  const blink::IndexedDBDatabaseMetadata& metadata() const {
    return backing_store_db_->GetMetadata();
  }
  const std::u16string& name() const { return name_; }
  int64_t version() const;
  bool IsInitialized() const;

  const list_set<Connection*>& connections() const { return connections_; }

  Status RunTasks();
  void RegisterAndScheduleTransaction(Transaction* transaction);

  // The database object (this object) must be kept alive for the duration of
  // this call. This means the caller should own an
  // BucketContextHandle while calling this methods.
  Status ForceCloseAndRunTasks(const std::string& message);

  void ScheduleOpenConnection(std::unique_ptr<PendingConnection> connection);

  void ScheduleDeleteDatabase(std::unique_ptr<FactoryClient> factory_client,
                              base::OnceClosure on_deletion_complete);

  // Number of connections that have progressed passed initial open call.
  size_t ConnectionCount() const { return connections_.size(); }

  bool IsAcceptingConnections() const { return !force_closing_; }

  // Number of active open/delete calls (running or blocked on other
  // connections).
  size_t ActiveOpenDeleteCount() const {
    return connection_coordinator_.ActiveOpenDeleteCount();
  }

  // Number of open/delete calls that are waiting their turn.
  size_t PendingOpenDeleteCount() const {
    return connection_coordinator_.PendingOpenDeleteCount();
  }

  Status VersionChangeOperation(int64_t version, Transaction* transaction);

  Status GetOperation(int64_t object_store_id,
                      int64_t index_id,
                      blink::IndexedDBKeyRange key_range,
                      indexed_db::CursorType cursor_type,
                      blink::mojom::IDBDatabase::GetCallback callback,
                      Transaction* transaction);

  Status SetIndexKeysOperation(
      int64_t object_store_id,
      blink::IndexedDBKey primary_key,
      std::vector<blink::IndexedDBIndexKeys> index_keys,
      Transaction* transaction);

  Status SetIndexesReadyOperation(size_t index_count, Transaction* transaction);

  struct OpenCursorOperationParams {
    OpenCursorOperationParams();

    OpenCursorOperationParams(const OpenCursorOperationParams&) = delete;
    OpenCursorOperationParams& operator=(const OpenCursorOperationParams&) =
        delete;

    ~OpenCursorOperationParams();
    int64_t object_store_id;
    int64_t index_id;
    blink::IndexedDBKeyRange key_range;
    blink::mojom::IDBCursorDirection direction;
    indexed_db::CursorType cursor_type;
    blink::mojom::IDBTaskType task_type;
    blink::mojom::IDBDatabase::OpenCursorCallback callback;
  };
  Status OpenCursorOperation(std::unique_ptr<OpenCursorOperationParams> params,
                             const storage::BucketLocator& bucket_locator,
                             Transaction* transaction);

  Status CountOperation(int64_t object_store_id,
                        int64_t index_id,
                        blink::IndexedDBKeyRange key_range,
                        blink::mojom::IDBDatabase::CountCallback callback,
                        Transaction* transaction);

  Status DeleteRangeOperation(
      int64_t object_store_id,
      blink::IndexedDBKeyRange key_range,
      blink::mojom::IDBDatabase::DeleteRangeCallback success_callback,
      Transaction* transaction);

  Status GetKeyGeneratorCurrentNumberOperation(
      int64_t object_store_id,
      blink::mojom::IDBDatabase::GetKeyGeneratorCurrentNumberCallback callback,
      Transaction* transaction);

  Status ClearOperation(int64_t object_store_id,
                        blink::mojom::IDBDatabase::ClearCallback callback,
                        Transaction* transaction);

  // Use this factory function for GetAll instead of creating the operation
  // directly.
  base::OnceCallback<Status(Transaction*)> CreateGetAllOperation(
      int64_t object_store_id,
      int64_t index_id,
      blink::IndexedDBKeyRange key_range,
      blink::mojom::IDBGetAllResultType result_type,
      int64_t max_count,
      blink::mojom::IDBCursorDirection direction,
      blink::mojom::IDBDatabase::GetAllCallback callback,
      Transaction* transaction);

  bool IsObjectStoreIdInMetadata(int64_t object_store_id) const;
  bool IsObjectStoreIdAndMaybeIndexIdInMetadata(int64_t object_store_id,
                                                int64_t index_id) const;

  // Returns metadata relevant to idb-internals.
  storage::mojom::IdbDatabaseMetadataPtr GetIdbInternalsMetadata() const;
  // Called when the data used to populate the struct in
  // `GetIdbInternalsMetadata` is changed in a significant way.
  void NotifyOfIdbInternalsRelevantChange();

  base::WeakPtr<Database> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

  void AddConnectionForTesting(Connection* connection) {
    if (connections_.empty()) {
      OpenInternal();
    }
    connections_.insert(connection);
  }

  bool CanBeDestroyed();

 protected:
  friend class Transaction;
  friend class ConnectionCoordinator::ConnectionRequest;
  friend class ConnectionCoordinator::OpenRequest;
  friend class ConnectionCoordinator::DeleteRequest;

 private:
  FRIEND_TEST_ALL_PREFIXES(DatabaseTest, OpenDeleteClear);
  FRIEND_TEST_ALL_PREFIXES(DatabaseOperationTest,
                           ObjectStoreGetAllKeysWithInvalidObjectStoreId);
  FRIEND_TEST_ALL_PREFIXES(DatabaseOperationTest,
                           IndexGetAllKeysWithInvalidIndexId);
  friend class DatabaseOperationTest;

  void CallUpgradeTransactionStartedForTesting(int64_t old_version);

  class ConnectionRequest;
  class OpenRequest;
  class DeleteRequest;

  Status OpenInternal();

  // This class informs its result sink of an error if a `GetAllOperation` is
  // deleted without being run. This functionality mimics that of
  // AbortOnDestruct callbacks. `GetAll()` cannot easily be shoe-horned into the
  // abort-on-destruct callback templating.
  class CONTENT_EXPORT GetAllResultSinkWrapper {
   public:
    GetAllResultSinkWrapper(base::WeakPtr<Transaction> transaction,
                            blink::mojom::IDBDatabase::GetAllCallback callback);
    ~GetAllResultSinkWrapper();

    mojo::AssociatedRemote<blink::mojom::IDBDatabaseGetAllResultSink>& Get();

    // An override for unit tests to bind the associated receiver successfully
    // without a pre-existing endpoint entanglement.
    void UseDedicatedReceiverForTesting() {
      use_dedicated_receiver_for_testing_ = true;
    }

   private:
    base::WeakPtr<Transaction> transaction_;
    blink::mojom::IDBDatabase::GetAllCallback callback_;
    mojo::AssociatedRemote<blink::mojom::IDBDatabaseGetAllResultSink>
        result_sink_;
    bool use_dedicated_receiver_for_testing_ = false;
  };

  Status GetAllOperation(int64_t object_store_id,
                         int64_t index_id,
                         blink::IndexedDBKeyRange key_range,
                         blink::mojom::IDBGetAllResultType result_type,
                         int64_t max_count,
                         blink::mojom::IDBCursorDirection direction,
                         std::unique_ptr<GetAllResultSinkWrapper> result_sink,
                         Transaction* transaction);

  // If there is no active request, grab a new one from the pending queue and
  // start it. Afterwards, possibly release the database by calling
  // MaybeReleaseDatabase().
  void ProcessRequestQueueAndMaybeRelease();

  // If there are no connections, pending requests, or an active request, then
  // this function will call `destroy_me_`, which can destruct this object.
  void MaybeReleaseDatabase();

  std::unique_ptr<Connection> CreateConnection(
      std::unique_ptr<DatabaseCallbacks> database_callbacks,
      mojo::Remote<storage::mojom::IndexedDBClientStateChecker>
          client_state_checker,
      base::UnguessableToken client_token,
      int scheduling_priority);

  // Ack that one of the connections notified with a "versionchange" event did
  // not promptly close. Therefore a "blocked" event should be fired at the
  // pending connection.
  void VersionChangeIgnored();

  bool HasNoConnections() const;

  void SendVersionChangeToAllConnections(int64_t old_version,
                                         int64_t new_version);

  // This can only be called when the given connection is closed and no longer
  // has any transaction objects.
  void ConnectionClosed(Connection* connection);

  std::vector<PartitionedLockManager::PartitionedLockRequest>
  BuildLockRequestsFromTransaction(Transaction* transaction) const;

  // In rare cases there are a very large number of queued
  // requests/transactions, so calculations related to blocking or blocked
  // clients can be expensive. See crbug.com/384476946. This method is used for
  // shortcutting such operations when there's only a single client. Also
  // returns true for zero clients.
  bool OnlyHasOneClient() const;

  // Find the transactions that block `current_transaction` from acquiring the
  // locks, and ensure that the clients with blocking transactions are active.
  void RequireBlockingTransactionClientsToBeActive(
      Transaction* current_transaction,
      std::vector<PartitionedLockManager::PartitionedLockRequest>&
          lock_requests);

  // Gets metadata for the given object store ID, asserting that the object
  // store exists.
  const blink::IndexedDBObjectStoreMetadata& GetObjectStoreMetadata(
      int64_t object_store_id) const;

  std::u16string name_;

  // The object that owns `this`.
  raw_ref<BucketContext> bucket_context_;

  list_set<Connection*> connections_;

  bool force_closing_ = false;

  ConnectionCoordinator connection_coordinator_;

  // Null until `OpenInternal()` is called successfully.
  std::unique_ptr<BackingStore::Database> backing_store_db_;

  // `weak_factory_` is used for all callback uses.
  base::WeakPtrFactory<Database> weak_factory_{this};
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_DATABASE_H_
