// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_

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
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_connection_coordinator.h"
#include "content/browser/indexed_db/indexed_db_factory_client.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/browser/indexed_db/indexed_db_value.h"
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

namespace content {
class IndexedDBBucketContext;
class IndexedDBConnection;
class IndexedDBDatabaseCallbacks;
class IndexedDBTransaction;
struct IndexedDBValue;

class CONTENT_EXPORT IndexedDBDatabase {
 public:
  // Identifier is pair of (bucket_locator, database name).
  using Identifier = std::pair<storage::BucketLocator, std::u16string>;
  // Used to report irrecoverable backend errors. The second argument can be
  // null.
  using ErrorCallback =
      base::RepeatingCallback<void(leveldb::Status, const char*)>;

  static const int64_t kInvalidId = 0;
  static const int64_t kMinimumIndexId = 30;

  IndexedDBDatabase(const std::u16string& name,
                    IndexedDBBucketContext& bucket_context,
                    const Identifier& unique_identifier);

  IndexedDBDatabase(const IndexedDBDatabase&) = delete;
  IndexedDBDatabase& operator=(const IndexedDBDatabase&) = delete;

  virtual ~IndexedDBDatabase();

  const Identifier& identifier() const { return identifier_; }
  IndexedDBBackingStore* backing_store();
  PartitionedLockManager* lock_manager();

  int64_t id() const { return metadata_.id; }
  const std::u16string& name() const { return metadata_.name; }
  const storage::BucketLocator& bucket_locator() const {
    return identifier_.first;
  }
  const blink::IndexedDBDatabaseMetadata& metadata() const { return metadata_; }

  const list_set<IndexedDBConnection*>& connections() const {
    return connections_;
  }

  // TODO(dmurph): Remove this method and have transactions be directly
  // scheduled using the lock manager.

  enum class RunTasksResult { kDone, kError, kCanBeDestroyed };
  std::tuple<RunTasksResult, leveldb::Status> RunTasks();
  void RegisterAndScheduleTransaction(IndexedDBTransaction* transaction);

  // The database object (this object) must be kept alive for the duration of
  // this call. This means the caller should own an
  // IndexedDBBucketContextHandle while calling this methods.
  leveldb::Status ForceCloseAndRunTasks();

  void TransactionCreated();
  void TransactionFinished(blink::mojom::IDBTransactionMode mode,
                           bool committed);

  void ScheduleOpenConnection(
      std::unique_ptr<IndexedDBPendingConnection> connection,
      scoped_refptr<IndexedDBClientStateCheckerWrapper> client_state_checker);

  void ScheduleDeleteDatabase(
      std::unique_ptr<IndexedDBFactoryClient> factory_client,
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

  leveldb::Status CreateObjectStoreOperation(
      int64_t object_store_id,
      const std::u16string& name,
      const blink::IndexedDBKeyPath& key_path,
      bool auto_increment,
      IndexedDBTransaction* transaction);
  void CreateObjectStoreAbortOperation(int64_t object_store_id);

  leveldb::Status DeleteObjectStoreOperation(int64_t object_store_id,
                                             IndexedDBTransaction* transaction);
  void DeleteObjectStoreAbortOperation(
      blink::IndexedDBObjectStoreMetadata object_store_metadata);

  leveldb::Status RenameObjectStoreOperation(int64_t object_store_id,
                                             const std::u16string& new_name,
                                             IndexedDBTransaction* transaction);
  void RenameObjectStoreAbortOperation(int64_t object_store_id,
                                       std::u16string old_name);

  leveldb::Status VersionChangeOperation(int64_t version,
                                         IndexedDBTransaction* transaction);
  void VersionChangeAbortOperation(int64_t previous_version);

  leveldb::Status CreateIndexOperation(int64_t object_store_id,
                                       int64_t index_id,
                                       const std::u16string& name,
                                       const blink::IndexedDBKeyPath& key_path,
                                       bool unique,
                                       bool multi_entry,
                                       IndexedDBTransaction* transaction);
  void CreateIndexAbortOperation(int64_t object_store_id, int64_t index_id);

  leveldb::Status DeleteIndexOperation(int64_t object_store_id,
                                       int64_t index_id,
                                       IndexedDBTransaction* transaction);
  void DeleteIndexAbortOperation(int64_t object_store_id,
                                 blink::IndexedDBIndexMetadata index_metadata);

  leveldb::Status RenameIndexOperation(int64_t object_store_id,
                                       int64_t index_id,
                                       const std::u16string& new_name,
                                       IndexedDBTransaction* transaction);
  void RenameIndexAbortOperation(int64_t object_store_id,
                                 int64_t index_id,
                                 std::u16string old_name);

  leveldb::Status GetOperation(
      int64_t object_store_id,
      int64_t index_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      indexed_db::CursorType cursor_type,
      blink::mojom::IDBDatabase::GetCallback callback,
      IndexedDBTransaction* transaction);

  leveldb::Status GetAllOperation(
      int64_t object_store_id,
      int64_t index_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      indexed_db::CursorType cursor_type,
      int64_t max_count,
      blink::mojom::IDBDatabase::GetAllCallback callback,
      IndexedDBTransaction* transaction);

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
  leveldb::Status PutOperation(std::unique_ptr<PutOperationParams> params,
                               IndexedDBTransaction* transaction);

  leveldb::Status SetIndexKeysOperation(
      int64_t object_store_id,
      std::unique_ptr<blink::IndexedDBKey> primary_key,
      const std::vector<blink::IndexedDBIndexKeys>& index_keys,
      IndexedDBTransaction* transaction);

  leveldb::Status SetIndexesReadyOperation(size_t index_count,
                                           IndexedDBTransaction* transaction);

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
  leveldb::Status OpenCursorOperation(
      std::unique_ptr<OpenCursorOperationParams> params,
      const storage::BucketLocator& bucket_locator,
      IndexedDBTransaction* transaction);

  leveldb::Status CountOperation(
      int64_t object_store_id,
      int64_t index_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      blink::mojom::IDBDatabase::CountCallback callback,
      IndexedDBTransaction* transaction);

  leveldb::Status DeleteRangeOperation(
      int64_t object_store_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      blink::mojom::IDBDatabase::DeleteRangeCallback success_callback,
      IndexedDBTransaction* transaction);

  leveldb::Status GetKeyGeneratorCurrentNumberOperation(
      int64_t object_store_id,
      blink::mojom::IDBDatabase::GetKeyGeneratorCurrentNumberCallback callback,
      IndexedDBTransaction* transaction);

  leveldb::Status ClearOperation(
      int64_t object_store_id,
      blink::mojom::IDBDatabase::ClearCallback callback,
      IndexedDBTransaction* transaction);

  bool IsObjectStoreIdInMetadata(int64_t object_store_id) const;
  bool IsObjectStoreIdAndIndexIdInMetadata(int64_t object_store_id,
                                           int64_t index_id) const;
  bool IsObjectStoreIdAndMaybeIndexIdInMetadata(int64_t object_store_id,
                                                int64_t index_id) const;
  bool IsObjectStoreIdInMetadataAndIndexNotInMetadata(int64_t object_store_id,
                                                      int64_t index_id) const;

  bool IsTransactionBlockingOthers(IndexedDBTransaction* transaction) const;

  base::WeakPtr<IndexedDBDatabase> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void AddConnectionForTesting(IndexedDBConnection* connection) {
    connections_.insert(connection);
  }

 protected:
  friend class IndexedDBTransaction;
  friend class IndexedDBConnectionCoordinator;
  friend class IndexedDBConnectionCoordinator::ConnectionRequest;
  friend class IndexedDBConnectionCoordinator::OpenRequest;
  friend class IndexedDBConnectionCoordinator::DeleteRequest;

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDatabaseTest, OpenDeleteClear);

  void CallUpgradeTransactionStartedForTesting(int64_t old_version);

  class ConnectionRequest;
  class OpenRequest;
  class DeleteRequest;

  leveldb::Status OpenInternal();

  // If there is no active request, grab a new one from the pending queue and
  // start it. Afterwards, possibly release the database by calling
  // MaybeReleaseDatabase().
  void ProcessRequestQueueAndMaybeRelease();

  // If there are no connections, pending requests, or an active request, then
  // this function will call `destroy_me_`, which can destruct this object.
  void MaybeReleaseDatabase();

  std::unique_ptr<IndexedDBConnection> CreateConnection(
      std::unique_ptr<IndexedDBDatabaseCallbacks> database_callbacks,
      scoped_refptr<IndexedDBClientStateCheckerWrapper> client_state_checker);

  // Ack that one of the connections notified with a "versionchange" event did
  // not promptly close. Therefore a "blocked" event should be fired at the
  // pending connection.
  void VersionChangeIgnored();

  bool HasNoConnections() const;

  void SendVersionChangeToAllConnections(int64_t old_version,
                                         int64_t new_version);

  // This can only be called when the given connection is closed and no longer
  // has any transaction objects.
  void ConnectionClosed(IndexedDBConnection* connection);

  bool CanBeDestroyed();

  std::vector<PartitionedLockManager::PartitionedLockRequest>
  BuildLockRequestsFromTransaction(IndexedDBTransaction* transaction) const;

  // Find the transactions that block `current_transaction` from acquiring the
  // locks, and ensure that the clients with blocking transactions are active.
  void RequireBlockingTransactionClientsToBeActive(
      IndexedDBTransaction* current_transaction,
      std::vector<PartitionedLockManager::PartitionedLockRequest>&
          lock_requests);

  // `metadata_` may not be fully initialized, but its `name` will always be
  // valid.
  blink::IndexedDBDatabaseMetadata metadata_;

  const Identifier identifier_;

  // The object that owns `this`.
  raw_ref<IndexedDBBucketContext> bucket_context_;

  int64_t transaction_count_ = 0;

  list_set<IndexedDBConnection*> connections_;

  bool force_closing_ = false;

  IndexedDBConnectionCoordinator connection_coordinator_;

  // `weak_factory_` is used for all callback uses.
  base::WeakPtrFactory<IndexedDBDatabase> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
