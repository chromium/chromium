// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/services/storage/indexed_db/scopes/scopes_lock_manager.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_connection_coordinator.h"
#include "content/browser/indexed_db/indexed_db_execution_context_connection_tracker.h"
#include "content/browser/indexed_db/indexed_db_observer.h"
#include "content/browser/indexed_db/indexed_db_origin_state_handle.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/list_set.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace blink {
class IndexedDBKeyPath;
class IndexedDBKeyRange;
struct IndexedDBDatabaseMetadata;
struct IndexedDBIndexMetadata;
struct IndexedDBObjectStoreMetadata;
}  // namespace blink

namespace url {
class Origin;
}

namespace content {
class IndexedDBClassFactory;
class IndexedDBConnection;
class IndexedDBDatabaseCallbacks;
class IndexedDBFactory;
class IndexedDBMetadataCoding;
class IndexedDBOriginStateHandle;
class IndexedDBTransaction;
struct IndexedDBValue;

class CONTENT_EXPORT IndexedDBDatabase {
 public:
  // Identifier is pair of (origin, database name).
  using Identifier = std::pair<url::Origin, base::string16>;
  // Used to report irrecoverable backend errors. The second argument can be
  // null.
  using ErrorCallback =
      base::RepeatingCallback<void(leveldb::Status, const char*)>;

  static const int64_t kInvalidId = 0;
  static const int64_t kMinimumIndexId = 30;

  virtual ~IndexedDBDatabase();

  const Identifier& identifier() const { return identifier_; }
  IndexedDBBackingStore* backing_store() { return backing_store_; }

  int64_t id() const { return metadata_.id; }
  const base::string16& name() const { return metadata_.name; }
  const url::Origin& origin() const { return identifier_.first; }
  const blink::IndexedDBDatabaseMetadata& metadata() const { return metadata_; }

  ScopesLockManager* transaction_lock_manager() { return lock_manager_; }
  const ScopesLockManager* transaction_lock_manager() const {
    return lock_manager_;
  }

  const list_set<IndexedDBConnection*>& connections() const {
    return connections_;
  }
  TasksAvailableCallback tasks_available_callback() {
    return tasks_available_callback_;
  }

  // TODO(dmurph): Remove this method and have transactions be directly
  // scheduled using the lock manager.

  enum class RunTasksResult { kDone, kError, kCanBeDestroyed };
  std::tuple<RunTasksResult, leveldb::Status> RunTasks();
  void RegisterAndScheduleTransaction(IndexedDBTransaction* transaction);

  // The database object (this object) must be kept alive for the duration of
  // this call. This means the caller should own an IndexedDBOriginStateHandle
  // while caling this methods.
  leveldb::Status ForceCloseAndRunTasks();

  void Commit(IndexedDBTransaction* transaction);

  void TransactionCreated();
  void TransactionFinished(blink::mojom::IDBTransactionMode mode,
                           bool committed);

  void AddPendingObserver(IndexedDBTransaction* transaction,
                          int32_t observer_id,
                          const IndexedDBObserver::Options& options);

  // |value| can be null for delete and clear operations.
  void FilterObservation(IndexedDBTransaction*,
                         int64_t object_store_id,
                         blink::mojom::IDBOperationType type,
                         const blink::IndexedDBKeyRange& key_range,
                         const IndexedDBValue* value);
  void SendObservations(
      std::map<int32_t, blink::mojom::IDBObserverChangesPtr> change_map);

  void ScheduleOpenConnection(
      IndexedDBOriginStateHandle origin_state_handle,
      std::unique_ptr<IndexedDBPendingConnection> connection);
  void ScheduleDeleteDatabase(IndexedDBOriginStateHandle origin_state_handle,
                              scoped_refptr<IndexedDBCallbacks> callbacks,
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
      const base::string16& name,
      const blink::IndexedDBKeyPath& key_path,
      bool auto_increment,
      IndexedDBTransaction* transaction);
  void CreateObjectStoreAbortOperation(int64_t object_store_id);

  leveldb::Status DeleteObjectStoreOperation(int64_t object_store_id,
                                             IndexedDBTransaction* transaction);
  void DeleteObjectStoreAbortOperation(
      blink::IndexedDBObjectStoreMetadata object_store_metadata);

  leveldb::Status RenameObjectStoreOperation(int64_t object_store_id,
                                             const base::string16& new_name,
                                             IndexedDBTransaction* transaction);
  void RenameObjectStoreAbortOperation(int64_t object_store_id,
                                       base::string16 old_name);

  leveldb::Status VersionChangeOperation(
      int64_t version,
      scoped_refptr<IndexedDBCallbacks> callbacks,
      IndexedDBTransaction* transaction);
  void VersionChangeAbortOperation(int64_t previous_version);

  leveldb::Status CreateIndexOperation(int64_t object_store_id,
                                       int64_t index_id,
                                       const base::string16& name,
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
                                       const base::string16& new_name,
                                       IndexedDBTransaction* transaction);
  void RenameIndexAbortOperation(int64_t object_store_id,
                                 int64_t index_id,
                                 base::string16 old_name);

  leveldb::Status GetOperation(
      base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
      int64_t object_store_id,
      int64_t index_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      indexed_db::CursorType cursor_type,
      blink::mojom::IDBDatabase::GetCallback callback,
      IndexedDBTransaction* transaction);

  leveldb::Status GetAllOperation(
      base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
      int64_t object_store_id,
      int64_t index_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      indexed_db::CursorType cursor_type,
      int64_t max_count,
      blink::mojom::IDBDatabase::GetAllCallback callback,
      IndexedDBTransaction* transaction);

  struct CONTENT_EXPORT PutOperationParams {
    PutOperationParams();
    ~PutOperationParams();
    int64_t object_store_id;
    IndexedDBValue value;
    std::unique_ptr<blink::IndexedDBKey> key;
    blink::mojom::IDBPutMode put_mode;
    blink::mojom::IDBTransaction::PutCallback callback;
    std::vector<blink::IndexedDBIndexKeys> index_keys;

   private:
    DISALLOW_COPY_AND_ASSIGN(PutOperationParams);
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
    ~OpenCursorOperationParams();
    int64_t object_store_id;
    int64_t index_id;
    std::unique_ptr<blink::IndexedDBKeyRange> key_range;
    blink::mojom::IDBCursorDirection direction;
    indexed_db::CursorType cursor_type;
    blink::mojom::IDBTaskType task_type;
    blink::mojom::IDBDatabase::OpenCursorCallback callback;

   private:
    DISALLOW_COPY_AND_ASSIGN(OpenCursorOperationParams);
  };
  leveldb::Status OpenCursorOperation(
      std::unique_ptr<OpenCursorOperationParams> params,
      const url::Origin& origin,
      base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
      IndexedDBTransaction* transaction);

  leveldb::Status CountOperation(
      int64_t object_store_id,
      int64_t index_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      scoped_refptr<IndexedDBCallbacks> callbacks,
      IndexedDBTransaction* transaction);

  leveldb::Status DeleteRangeOperation(
      int64_t object_store_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      scoped_refptr<IndexedDBCallbacks> callbacks,
      IndexedDBTransaction* transaction);

  leveldb::Status GetKeyGeneratorCurrentNumberOperation(
      int64_t object_store_id,
      scoped_refptr<IndexedDBCallbacks> callbacks,
      IndexedDBTransaction* transaction);

  leveldb::Status ClearOperation(int64_t object_store_id,
                                 scoped_refptr<IndexedDBCallbacks> callbacks,
                                 IndexedDBTransaction* transaction);

  bool IsObjectStoreIdInMetadata(int64_t object_store_id) const;
  bool IsObjectStoreIdAndIndexIdInMetadata(int64_t object_store_id,
                                           int64_t index_id) const;
  bool IsObjectStoreIdAndMaybeIndexIdInMetadata(int64_t object_store_id,
                                                int64_t index_id) const;
  bool IsObjectStoreIdInMetadataAndIndexNotInMetadata(int64_t object_store_id,
                                                      int64_t index_id) const;

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

  IndexedDBDatabase(const base::string16& name,
                    IndexedDBBackingStore* backing_store,
                    IndexedDBFactory* factory,
                    IndexedDBClassFactory* class_factory,
                    TasksAvailableCallback tasks_available_callback,
                    std::unique_ptr<IndexedDBMetadataCoding> metadata_coding,
                    const Identifier& unique_identifier,
                    ScopesLockManager* transaction_lock_manager);

  // May be overridden in tests.
  virtual size_t GetUsableMessageSizeInBytes() const;

 private:
  friend class MockBrowserTestIndexedDBClassFactory;
  friend class IndexedDBClassFactory;

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
  // this function will call |destroy_me_|, which can destruct this object.
  void MaybeReleaseDatabase();

  std::unique_ptr<IndexedDBConnection> CreateConnection(
      IndexedDBOriginStateHandle origin_state_handle,
      scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
      IndexedDBExecutionContextConnectionTracker::Handle
          execution_context_connection_handle);

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

  // Safe because the IndexedDBBackingStore is owned by the same object which
  // owns us, the IndexedDBPerOriginFactory.
  IndexedDBBackingStore* backing_store_;
  blink::IndexedDBDatabaseMetadata metadata_;

  const Identifier identifier_;
  // TODO(dmurph): Remove the need for this to be here (and then remove it).
  IndexedDBFactory* factory_;
  IndexedDBClassFactory* const class_factory_;
  std::unique_ptr<IndexedDBMetadataCoding> metadata_coding_;

  ScopesLockManager* lock_manager_;
  int64_t transaction_count_ = 0;

  list_set<IndexedDBConnection*> connections_;

  TasksAvailableCallback tasks_available_callback_;

  bool force_closing_ = false;

  IndexedDBConnectionCoordinator connection_coordinator_;

  // |weak_factory_| is used for all callback uses.
  base::WeakPtrFactory<IndexedDBDatabase> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
