// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/queued_task_poster.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace remoting {

QueuedTaskPoster::QueuedTaskPoster(
    scoped_refptr<base::SingleThreadTaskRunner> target_task_runner)
    : target_task_runner_(target_task_runner) {}

QueuedTaskPoster::~QueuedTaskPoster() {
  if (source_task_runner_) {
    DCHECK(source_task_runner_->BelongsToCurrentThread());
  }
}

void QueuedTaskPoster::AddTask(const base::Closure& closure) {
  if (!source_task_runner_) {
    source_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }
  DCHECK(source_task_runner_->BelongsToCurrentThread());
  task_queue_.push(closure);
  if (!transfer_task_scheduled_) {
    source_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QueuedTaskPoster::TransferTaskQueue,
                                  weak_factory_.GetWeakPtr()));
    transfer_task_scheduled_ = true;
  }
}

static void ConsumeTaskQueue(base::queue<base::Closure>* queue) {
  while (!queue->empty()) {
    queue->front().Run();
    queue->pop();
  }
}

void QueuedTaskPoster::TransferTaskQueue() {
  DCHECK(transfer_task_scheduled_);
  transfer_task_scheduled_ = false;
  base::queue<base::Closure>* queue_to_transfer =
      new base::queue<base::Closure>();
  queue_to_transfer->swap(task_queue_);
  target_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ConsumeTaskQueue, base::Owned(queue_to_transfer)));
}

}  // namespace remoting
