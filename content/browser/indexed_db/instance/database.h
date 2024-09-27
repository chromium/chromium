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
#include <tuple>
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
class IndexedDBKeyPath;
class IndexedDBKeyRange;
struct IndexedDBDatabaseMetadata;
struct IndexedDBIndexMetadata;
struct IndexedDBObjectStoreMetadata;
}  // namespace blink

namespace content::indexed_db {
class BucketContext;
class Connection;
class DatabaseCallbacks;
class Transaction;
struct IndexedDBValue;
enum class CursorType;

// This class maps to a single IDB database:
// https://www.w3.org/TR/IndexedDB/#database
//
// It is created and operated on a bucket thread.
class CONTENT_EXPORT Database {
 public:
  // Identifier is pair of (bucket_locator, database name).
  using Identifier = std::pair<storage::BucketLocator, std::u16string>;
  // Used to report irrecoverable backend errors. The second argument can be
  // null.
  using ErrorCallback = base::RepeatingCallback<void(Status, const char*)>;

  static const int64_t kInvalidId = 0;
  static const int64_t kMinimumIndexId = 30;

  Database(const std::u16string& name,
           BucketContext& bucket_context,
           const Identifier& unique_identifier);

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  virtual ~Database();

  const Identifier& identifier() const { return identifier_; }
  BackingStore* backing_store();
  PartitionedLockManager& lock_manager();

  int64_t id() const { return metadata_.id; }
  const std::u16string& name() const { return metadata_.name; }
  const storage::BucketLocator& bucket_locator() const {
    return identifier_.first;
  }
  const blink::IndexedDBDatabaseMetadata& metadata() const { return metadata_; }

  const list_set<Connection*>& connections() const { return connections_; }

  enum class RunTasksResult { kDone, kError, kCanBeDestroyed };
  std::tuple<RunTasksResult, Status> RunTasks();
  void RegisterAndScheduleTransaction(Transaction* transaction);

  // The database object (this object) must be kept alive for the duration of
  // this call. This means the caller should own an
  // BucketContextHandle while calling this methods.
  Status ForceCloseAndRunTasks();

  void ScheduleOpenConnection(std::unique_ptr<PendingConnection> connection);

  void ScheduleDeleteDatabase(std::unique_ptr<FactoryClient> factory_client,
                              base::OnceClosure on_deletion_complete);

  void AddObjectStoreToMetadata(blink::IndexedDBObjectStoreMetadata metadata,
                                int64_t new_max_object_store_id);
  blink::IndexedDBObjectStoreMetadata RemoveObjectStoreFromMetadata(
      int64_t object_store_id);
  void AddIndexToMetadata(int64_t object_store_id,
                          blink::IndexedDBIndexMetadata metadata,
                          int64_t new_max_index_id);
  blink::IndexedDBIndexMetadata RemoveIndexFromMetadata(int64_t object_store_id,
                                                        int64_t index_id);

  // The following methods all schedule a task on the transaction & modify the
  // database:

  // Number of connections that have progressed passed initial open call.
  size_t ConnectionCount() const { return connections_.size(); }

  // Number of active open/delete calls (running or blocked on other
  // connections).
  size_t ActiveOpenDeleteCount() const {
    return connection_coordinator_.ActiveOpenDeleteCount();
  }

  // Number of open/delete calls that are waiting their turn.
  size_t PendingOpenDeleteCount() const {
    return connection_coordinator_.PendingOpenDeleteCount();
  }

  // The following methods are all of the ones actually scheduled asynchronously
  // within transctions:
  Status CreateObjectStoreOperation(int64_t object_store_id,
                                    const std::u16string& name,
                                    const blink::IndexedDBKeyPath& key_path,
                                    bool auto_increment,
                                    Transaction* transaction);

  void CreateObjectStoreAbortOperation(int64_t object_store_id);

  Status DeleteObjectStoreOperation(int64_t object_store_id,
                                    Transaction* transaction);
  void DeleteObjectStoreAbortOperation(
      blink::IndexedDBObjectStoreMetadata object_store_metadata);

  Status RenameObjectStoreOperation(int64_t object_store_id,
                                    const std::u16string& new_name,
                                    Transaction* transaction);
  void RenameObjectStoreAbortOperation(int64_t object_store_id,
                                       std::u16string old_name);

  Status VersionChangeOperation(int64_t version, Transaction* transaction);
  void VersionChangeAbortOperation(int64_t previous_version);

  Status CreateIndexOperation(int64_t object_store_id,
                              int64_t index_id,
                              const std::u16string& name,
                              const blink::IndexedDBKeyPath& key_path,
                              bool unique,
                              bool multi_entry,
                              Transaction* transaction);
  void CreateIndexAbortOperation(int64_t object_store_id, int64_t index_id);

  Status DeleteIndexOperation(int64_t object_store_id,
                              int64_t index_id,
                              Transaction* transaction);
  void DeleteIndexAbortOperation(int64_t object_store_id,
                                 blink::IndexedDBIndexMetadata index_metadata);

  Status RenameIndexOperation(int64_t object_store_id,
                              int64_t index_id,
                              const std::u16string& new_name,
                              Transaction* transaction);
  void RenameIndexAbortOperation(int64_t object_store_id,
                                 int64_t index_id,
                                 std::u16string old_name);

  Status GetOperation(int64_t object_store_id,
                      int64_t index_id,
                      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
                      indexed_db::CursorType cursor_type,
                      blink::mojom::IDBDatabase::GetCallback callback,
                      Transaction* transaction);

  struct CONTENT_EXPORT PutOperationParams {
    PutOperationParams();

    PutOperationParams(const PutOperationParams&) = delete;
    PutOperationParams& operator=(const PutOperationParams&) = delete;

    ~PutOperationParams();
    int64_t object_store_id;
    IndexedDBValue value;
    std::unique_ptr<blink::IndexedDBKey> key;
    blink::mojom::IDBPutMode put_mode;
    blink::mojom::IDBTransaction::PutCallback callback;
    std::vector<blink::IndexedDBIndexKeys> index_keys;
  };
  Status PutOperation(std::unique_ptr<PutOperationParams> params,
                      Transaction* transaction);

  Status SetIndexKeysOperation(
      int64_t object_store_id,
      std::unique_ptr<blink::IndexedDBKey> primary_key,
      const std::vector<blink::IndexedDBIndexKeys>& index_keys,
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
    std::unique_ptr<blink::IndexedDBKeyRange> key_range;
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
                        std::unique_ptr<blink::IndexedDBKeyRange> key_range,
                        blink::mojom::IDBDatabase::CountCallback callback,
                        Transaction* transaction);

  Status DeleteRangeOperation(
      int64_t object_store_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
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
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      indexed_db::CursorType cursor_type,
      int64_t max_count,
      blink::mojom::IDBDatabase::GetAllCallback callback,
      Transaction* transaction);

  bool IsObjectStoreIdInMetadata(int64_t object_store_id) const;
  bool IsObjectStoreIdAndIndexIdInMetadata(int64_t object_store_id,
                                           int64_t index_id) const;
  bool IsObjectStoreIdAndMaybeIndexIdInMetadata(int64_t object_store_id,
                                                int64_t index_id) const;
  bool IsObjectStoreIdInMetadataAndIndexNotInMetadata(int64_t object_store_id,
                                                      int64_t index_id) const;

  // Returns metadata relevant to idb-internals.
  storage::mojom::IdbDatabaseMetadataPtr GetIdbInternalsMetadata() const;
  // Called when the data used to populate the struct in
  // `GetIdbInternalsMetadata` is changed in a significant way.
  void NotifyOfIdbInternalsRelevantChange();

  base::WeakPtr<Database> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

  void AddConnectionForTesting(Connection* connection) {
    connections_.insert(connection);
  }

 protected:
  friend class Transaction;
  friend class ConnectionCoordinator;
  friend class ConnectionCoordinator::ConnectionRequest;
  friend class ConnectionCoordinator::OpenRequest;
  friend class ConnectionCoordinator::DeleteRequest;

 private:
  FRIEND_TEST_ALL_PREFIXES(DatabaseTest, OpenDeleteClear);

  void CallUpgradeTransactionStartedForTesting(int64_t old_version);

  class ConnectionRequest;
  class OpenRequest;
  class DeleteRequest;

  Status OpenInternal();

  // This class informs its result sink of an error if a `GetAllOperation` is
  // deleted without being run. This functionality mimics that of
  // AbortOnDestruct callbacks. `GetAll()` cannot easily be shoe-horned into the
  // abort-on-destruct callback templating.
  class GetAllResultSinkWrapper {
   public:
    GetAllResultSinkWrapper(base::WeakPtr<Transaction> transaction,
                            blink::mojom::IDBDatabase::GetAllCallback callback);
    ~GetAllResultSinkWrapper();

    mojo::AssociatedRemote<blink::mojom::IDBDatabaseGetAllResultSink>& Get();

   private:
    base::WeakPtr<Transaction> transaction_;
    blink::mojom::IDBDatabase::GetAllCallback callback_;
    mojo::AssociatedRemote<blink::mojom::IDBDatabaseGetAllResultSink>
        result_sink_;
  };

  Status GetAllOperation(int64_t object_store_id,
                         int64_t index_id,
                         std::unique_ptr<blink::IndexedDBKeyRange> key_range,
                         indexed_db::CursorType cursor_type,
                         int64_t max_count,
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

  bool CanBeDestroyed();

  std::vector<PartitionedLockManager::PartitionedLockRequest>
  BuildLockRequestsFromTransaction(Transaction* transaction) const;

  // Find the transactions that block `current_transaction` from acquiring the
  // locks, and ensure that the clients with blocking transactions are active.
  void RequireBlockingTransactionClientsToBeActive(
      Transaction* current_transaction,
      std::vector<PartitionedLockManager::PartitionedLockRequest>&
          lock_requests);

  // `metadata_` may not be fully initialized, but its `name` will always be
  // valid.
  blink::IndexedDBDatabaseMetadata metadata_;

  const Identifier identifier_;

  // The object that owns `this`.
  raw_ref<BucketContext> bucket_context_;

  list_set<Connection*> connections_;

  bool force_closing_ = false;

  ConnectionCoordinator connection_coordinator_;

  // `weak_factory_` is used for all callback uses.
  base::WeakPtrFactory<Database> weak_factory_{this};
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_DATABASE_H_
