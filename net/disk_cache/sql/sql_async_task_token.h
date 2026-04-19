// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_ASYNC_TASK_TOKEN_H_
#define NET_DISK_CACHE_SQL_SQL_ASYNC_TASK_TOKEN_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/base/net_export.h"

namespace disk_cache {

class SqlAsyncTaskManager;

// A token representing a pending asynchronous task.
// This class is not thread-safe. It must be created and destroyed on the same
// sequence. Typically, it is created on the IO thread and bound to a reply
// callback (e.g., via PostTaskAndReply or SequenceBound::Then), ensuring that
// it is destroyed on the IO thread when the asynchronous operation completes.
//
// When this token is destroyed, it notifies the SqlAsyncTaskManager that
// the task has completed.
class NET_EXPORT_PRIVATE SqlAsyncTaskToken {
 public:
  explicit SqlAsyncTaskToken(base::WeakPtr<SqlAsyncTaskManager> manager);
  ~SqlAsyncTaskToken();

  SqlAsyncTaskToken(const SqlAsyncTaskToken&) = delete;
  SqlAsyncTaskToken& operator=(const SqlAsyncTaskToken&) = delete;

 private:
  base::WeakPtr<SqlAsyncTaskManager> manager_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_ASYNC_TASK_TOKEN_H_
