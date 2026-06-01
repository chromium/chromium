// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_ASYNC_TASK_MANAGER_H_
#define NET_DISK_CACHE_SQL_SQL_ASYNC_TASK_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "net/base/net_export.h"

namespace disk_cache {

class SqlAsyncTaskToken;

// Manages the tracking of asynchronous tasks in the SQL backend.
// It issues SqlAsyncTaskToken objects and keeps track of how many are currently
// alive. This allows tests to wait for all asynchronous operations to finish.
//
// This class is not thread-safe and must be used on a single sequence.
class NET_EXPORT_PRIVATE SqlAsyncTaskManager {
 public:
  SqlAsyncTaskManager();
  ~SqlAsyncTaskManager();

  SqlAsyncTaskManager(const SqlAsyncTaskManager&) = delete;
  SqlAsyncTaskManager& operator=(const SqlAsyncTaskManager&) = delete;

  // Starts a new tracked task and returns a token. The task is considered
  // pending until the returned token is destroyed.
  std::unique_ptr<SqlAsyncTaskToken> StartTask();

  // Runs the message loop until all tracked tasks are complete.
  void RunUntilAllTasksCompleteForTest();

  // Called by ~SqlAsyncTaskToken when a task completes.
  void OnTaskComplete(base::PassKey<SqlAsyncTaskToken>);

 private:
  friend class SqlAsyncTaskManagerTest;

  // Registers a callback to be run when all outstanding tasks have completed.
  // If there are currently no pending tasks, the callback is run immediately.
  void RunOnAllTasksCompleteForTest(base::OnceClosure callback);

  void CheckAndRunCallback();

  scoped_refptr<base::RefCountedData<std::atomic_bool>> shutdown_flag_;
  int pending_task_count_ = 0;
  base::OnceClosure on_all_tasks_complete_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SqlAsyncTaskManager> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_ASYNC_TASK_MANAGER_H_
