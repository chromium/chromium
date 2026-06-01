// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_ASYNC_TASK_TOKEN_H_
#define NET_DISK_CACHE_SQL_SQL_ASYNC_TASK_TOKEN_H_

#include <atomic>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/net_export.h"

namespace disk_cache {

class SqlAsyncTaskManager;

// This class is generally not thread-safe. It should be created and destroyed
// on the same sequence. Typically, it is created on the IO thread and bound to
// a reply callback (e.g., via PostTaskAndReply or SequenceBound::Then),
// ensuring that it is destroyed on the IO thread when the asynchronous
// operation completes.
//
// However, it tolerates being destroyed on a different sequence during
// shutdown (when posting back to the original sequence fails), in which case it
// silently drops the task completion notification.
class NET_EXPORT_PRIVATE SqlAsyncTaskToken {
 public:
  explicit SqlAsyncTaskToken(
      base::WeakPtr<SqlAsyncTaskManager> manager,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> shutdown_flag);
  ~SqlAsyncTaskToken();

  SqlAsyncTaskToken(const SqlAsyncTaskToken&) = delete;
  SqlAsyncTaskToken& operator=(const SqlAsyncTaskToken&) = delete;

 private:
  base::WeakPtr<SqlAsyncTaskManager> manager_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::RefCountedData<std::atomic_bool>> shutdown_flag_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_ASYNC_TASK_TOKEN_H_
