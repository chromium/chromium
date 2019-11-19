// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_WORKER_THREAD_REGISTRY_H_
#define CONTENT_RENDERER_WORKER_WORKER_THREAD_REGISTRY_H_

#include <map>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "content/common/content_export.h"
#include "content/public/renderer/worker_thread.h"

namespace base {
class TaskRunner;
}

namespace content {

class CONTENT_EXPORT WorkerThreadRegistry {
 public:
  WorkerThreadRegistry();

  int PostTaskToAllThreads(const base::RepeatingClosure& task);
  static WorkerThreadRegistry* Instance();

  void DidStartCurrentWorkerThread();
  void WillStopCurrentWorkerThread();

  // Always returns a non-null task runner regardless of whether the
  // corresponding worker thread is gone or not. If the thread is already gone
  // the tasks posted onto the task runner will be silently discarded.
  base::TaskRunner* GetTaskRunnerFor(int worker_id);

 private:
  friend class WorkerThread;
  friend class WorkerThreadRegistryTest;

  bool PostTask(int id, base::OnceClosure task);

  ~WorkerThreadRegistry();

  // It is possible for an IPC message to arrive for a worker thread that has
  // already gone away. In such cases, it is still necessary to provide a
  // task-runner for that particular thread, because otherwise the message will
  // end up being handled as per usual in the main-thread, causing incorrect
  // results. |task_runner_for_dead_worker_| is used to handle such messages,
  // which silently discards all the tasks it receives.
  scoped_refptr<base::TaskRunner> task_runner_for_dead_worker_;

  std::map<int /* worker_thread_id */, base::TaskRunner*> task_runner_map_;
  base::Lock task_runner_map_lock_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WORKER_WORKER_THREAD_REGISTRY_H_
