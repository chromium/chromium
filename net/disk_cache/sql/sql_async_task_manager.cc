// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_async_task_manager.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "net/disk_cache/sql/sql_async_task_token.h"

namespace disk_cache {

SqlAsyncTaskManager::SqlAsyncTaskManager() = default;

SqlAsyncTaskManager::~SqlAsyncTaskManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<SqlAsyncTaskToken> SqlAsyncTaskManager::StartTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++pending_task_count_;
  return std::make_unique<SqlAsyncTaskToken>(weak_factory_.GetWeakPtr());
}

void SqlAsyncTaskManager::RunOnAllTasksCompleteForTest(  // IN-TEST
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!on_all_tasks_complete_callback_)
      << "Only one Wait callback can be registered at a time.";

  on_all_tasks_complete_callback_ = std::move(callback);
  if (pending_task_count_ == 0) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SqlAsyncTaskManager::CheckAndRunCallback,
                                  weak_factory_.GetWeakPtr()));
  }
}

void SqlAsyncTaskManager::RunUntilAllTasksCompleteForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::RunLoop run_loop;
  RunOnAllTasksCompleteForTest(run_loop.QuitClosure());  // IN-TEST
  run_loop.Run();
}

void SqlAsyncTaskManager::OnTaskComplete(base::PassKey<SqlAsyncTaskToken>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(pending_task_count_, 0);
  --pending_task_count_;

  if (pending_task_count_ == 0 && on_all_tasks_complete_callback_) {
    // We post a task to verify the count is still zero rather than finishing
    // immediately. This handles cases where the completion of the last task
    // immediately schedules a new tracked operation (e.g., in a multi-step
    // process). Posting ensures we only finish when the sequence has settled.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SqlAsyncTaskManager::CheckAndRunCallback,
                                  weak_factory_.GetWeakPtr()));
  }
}

void SqlAsyncTaskManager::CheckAndRunCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only finish if the count is still zero. If a new task was started between
  // the PostTask and this check, we must continue waiting.
  if (pending_task_count_ == 0 && on_all_tasks_complete_callback_) {
    std::move(on_all_tasks_complete_callback_).Run();
  }
}

}  // namespace disk_cache
