// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_async_task_token.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "net/disk_cache/sql/sql_async_task_manager.h"

namespace disk_cache {

SqlAsyncTaskToken::SqlAsyncTaskToken(
    base::WeakPtr<SqlAsyncTaskManager> manager,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> shutdown_flag)
    : manager_(std::move(manager)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      shutdown_flag_(std::move(shutdown_flag)) {
  CHECK(shutdown_flag_);
}

SqlAsyncTaskToken::~SqlAsyncTaskToken() {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    if (manager_) {
      manager_->OnTaskComplete(base::PassKey<SqlAsyncTaskToken>());
    }
  } else {
    // Destroyed on a different sequence. This is only allowed during shutdown
    // (when posting back to the original sequence fails).
    CHECK(shutdown_flag_->data.load());
  }
}

}  // namespace disk_cache
