// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_COORDINATOR_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_COORDINATOR_H_

#include <memory>
#include <tuple>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db_origin_state_handle.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/browser/indexed_db/list_set.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {
class IndexedDBCallbacks;
class IndexedDBConnection;
class IndexedDBDatabase;
struct IndexedDBPendingConnection;

class CONTENT_EXPORT IndexedDBConnectionCoordinator {
 public:
  static const int64_t kInvalidDatabaseId = 0;
  static const int64_t kMinimumIndexId = 30;

  IndexedDBConnectionCoordinator(
      IndexedDBDatabase* db,
      TasksAvailableCallback tasks_available_callback);
  ~IndexedDBConnectionCoordinator();

  void ScheduleOpenConnection(
      IndexedDBOriginStateHandle origin_state_handle,
      std::unique_ptr<IndexedDBPendingConnection> connection);

  void ScheduleDeleteDatabase(IndexedDBOriginStateHandle origin_state_handle,
                              scoped_refptr<IndexedDBCallbacks> callbacks,
                              base::OnceClosure on_deletion_complete);

  // Call this method to prune any tasks that don't want to be run during
  // force close. Returns any error caused by rolling back changes.
  leveldb::Status PruneTasksForForceClose();

  void OnConnectionClosed(IndexedDBConnection* connection);

  void OnNoConnections();

  // Ack that one of the connections notified with a "versionchange" event did
  // not promptly close. Therefore a "blocked" event should be fired at the
  // pending connection.
  void OnVersionChangeIgnored();

  void CreateAndBindUpgradeTransaction();

  void OnUpgradeTransactionStarted(int64_t old_version);

  void OnUpgradeTransactionFinished(bool committed);

  enum class ExecuteTaskResult {
    // There are more tasks to run, so ExecuteTask() should be called again.
    kMoreTasks,
    // There are tasks but they are waiting on async work to complete. No more
    // calls to ExecuteTask() are necessary.
    kPendingAsyncWork,
    // There was an error executing a task - see the status. The offending task
    // was removed, and the caller can choose to continue executing tasks if
    // they want.
    kError,
    // There are no more tasks to run.
    kDone,
  };
  std::tuple<ExecuteTaskResult, leveldb::Status> ExecuteTask(
      bool has_connections);

  bool HasTasks() const { return !request_queue_.empty(); }

  // Number of active open/delete calls (running or blocked on other
  // connections).
  size_t ActiveOpenDeleteCount() const;

  // Number of open/delete calls that are waiting their turn.
  size_t PendingOpenDeleteCount() const;

  base::WeakPtr<IndexedDBConnectionCoordinator> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class IndexedDBDatabase;
  class ConnectionRequest;
  class OpenRequest;
  class DeleteRequest;

  IndexedDBDatabase* db_;

  TasksAvailableCallback tasks_available_callback_;

  base::queue<std::unique_ptr<ConnectionRequest>> request_queue_;

  // |weak_factory_| is used for all callback uses.
  base::WeakPtrFactory<IndexedDBConnectionCoordinator> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IndexedDBConnectionCoordinator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_COORDINATOR_H_
