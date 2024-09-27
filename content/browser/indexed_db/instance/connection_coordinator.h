// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_CONNECTION_COORDINATOR_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_CONNECTION_COORDINATOR_H_

#include <memory>
#include <tuple>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/status.h"
#include "content/common/content_export.h"

namespace content::indexed_db {
class FactoryClient;
class Connection;
class Database;
struct PendingConnection;

class CONTENT_EXPORT ConnectionCoordinator {
 public:
  static const int64_t kInvalidDatabaseId = 0;
  static const int64_t kMinimumIndexId = 30;

  ConnectionCoordinator(Database* db, BucketContext& bucket_context);

  ConnectionCoordinator(const ConnectionCoordinator&) = delete;
  ConnectionCoordinator& operator=(const ConnectionCoordinator&) = delete;

  ~ConnectionCoordinator();

  void ScheduleOpenConnection(std::unique_ptr<PendingConnection> connection);

  void ScheduleDeleteDatabase(std::unique_ptr<FactoryClient> factory_client,
                              base::OnceClosure on_deletion_complete);

  // Call this method to prune any tasks that don't want to be run during
  // force close. Returns any error caused by rolling back changes.
  Status PruneTasksForForceClose();

  void OnConnectionClosed(Connection* connection);

  void OnNoConnections();

  // Ack that one of the connections notified with a "versionchange" event did
  // not promptly close. Therefore a "blocked" event should be fired at the
  // pending connection.
  void OnVersionChangeIgnored();

  void BindVersionChangeTransactionReceiver();

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
  std::tuple<ExecuteTaskResult, Status> ExecuteTask(bool has_connections);

  bool HasTasks() const { return !request_queue_.empty(); }

  // Number of active open/delete calls (running or blocked on other
  // connections).
  size_t ActiveOpenDeleteCount() const;

  // Number of open/delete calls that are waiting their turn.
  size_t PendingOpenDeleteCount() const;

  base::WeakPtr<ConnectionCoordinator> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class Database;
  class ConnectionRequest;
  class OpenRequest;
  class DeleteRequest;

  raw_ptr<Database> db_;

  raw_ref<BucketContext> bucket_context_;

  base::queue<std::unique_ptr<ConnectionRequest>> request_queue_;

  // `weak_factory_` is used for all callback uses.
  base::WeakPtrFactory<ConnectionCoordinator> weak_factory_{this};
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_CONNECTION_COORDINATOR_H_
