// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_QUEUED_TASK_POSTER_H_
#define REMOTING_CLIENT_QUEUED_TASK_POSTER_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"

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
  ~QueuedTaskPoster();

  void AddTask(const base::Closure& closure);

 private:
  void TransferTaskQueue();

  scoped_refptr<base::SingleThreadTaskRunner> source_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> target_task_runner_;

  base::queue<base::Closure> task_queue_;

  bool transfer_task_scheduled_ = false;

  base::WeakPtrFactory<QueuedTaskPoster> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QueuedTaskPoster);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_QUEUED_TASK_POSTER_H_
