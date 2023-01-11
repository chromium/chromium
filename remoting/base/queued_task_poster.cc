// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/queued_task_poster.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

QueuedTaskPoster::QueuedTaskPoster(
    scoped_refptr<base::SingleThreadTaskRunner> target_task_runner)
    : target_task_runner_(target_task_runner) {}

QueuedTaskPoster::~QueuedTaskPoster() {
  if (source_task_runner_) {
    DCHECK(source_task_runner_->BelongsToCurrentThread());
  }
}

void QueuedTaskPoster::AddTask(base::OnceClosure closure) {
  if (!source_task_runner_) {
    source_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  }
  DCHECK(source_task_runner_->BelongsToCurrentThread());
  task_queue_.emplace(std::move(closure));
  if (!transfer_task_scheduled_) {
    source_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QueuedTaskPoster::TransferTaskQueue,
                                  weak_factory_.GetWeakPtr()));
    transfer_task_scheduled_ = true;
  }
}

static void ConsumeTaskQueue(base::queue<base::OnceClosure>* queue) {
  while (!queue->empty()) {
    std::move(queue->front()).Run();
    queue->pop();
  }
}

void QueuedTaskPoster::TransferTaskQueue() {
  DCHECK(transfer_task_scheduled_);
  transfer_task_scheduled_ = false;
  base::queue<base::OnceClosure>* queue_to_transfer =
      new base::queue<base::OnceClosure>();
  queue_to_transfer->swap(task_queue_);
  target_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ConsumeTaskQueue, base::Owned(queue_to_transfer)));
}

}  // namespace remoting
