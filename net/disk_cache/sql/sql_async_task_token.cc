// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_async_task_token.h"

#include "base/types/pass_key.h"
#include "net/disk_cache/sql/sql_async_task_manager.h"

namespace disk_cache {

SqlAsyncTaskToken::SqlAsyncTaskToken(base::WeakPtr<SqlAsyncTaskManager> manager)
    : manager_(std::move(manager)) {}

SqlAsyncTaskToken::~SqlAsyncTaskToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (manager_) {
    manager_->OnTaskComplete(base::PassKey<SqlAsyncTaskToken>());
  }
}

}  // namespace disk_cache
