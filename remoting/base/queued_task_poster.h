// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_QUEUED_TASK_POSTER_H_
#define REMOTING_BASE_QUEUED_TASK_POSTER_H_

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

// This is a helper class ensuring tasks posted to |target_task_runner| inside
// the same task slot will be queued up and executed together after current
// task is done on |source_task_runner|. This can prevent unrelated tasks to
// be scheduled on |target_task_runner| in between the task sequence.
// This class can be created on any thread but must be used and deleted on the
// thread of |source_task_runner|.
class QueuedTaskPoster {
 public:
  QueuedTaskPoster(
      scoped_refptr<base::SingleThreadTaskRunner> target_task_runner);

  QueuedTaskPoster(const QueuedTaskPoster&) = delete;
  QueuedTaskPoster& operator=(const QueuedTaskPoster&) = delete;

  ~QueuedTaskPoster();

  void AddTask(base::OnceClosure closure);

 private:
  void TransferTaskQueue();

  scoped_refptr<base::SingleThreadTaskRunner> source_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> target_task_runner_;

  base::queue<base::OnceClosure> task_queue_;

  bool transfer_task_scheduled_ = false;

  base::WeakPtrFactory<QueuedTaskPoster> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_BASE_QUEUED_TASK_POSTER_H_
