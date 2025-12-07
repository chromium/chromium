// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/resource_task.h"

#include "services/webnn/queueable_resource_state_base.h"

namespace webnn {

ResourceTask::ResourceTask(
    std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources,
    std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources,
    base::OnceCallback<void(base::OnceClosure)> task)
    : shared_resources_(std::move(shared_resources)),
      exclusive_resources_(std::move(exclusive_resources)),
      task_(std::move(task)) {}

void ResourceTask::Enqueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (CanExecute()) {
    Execute(/*dequeue=*/false);
    return;
  }

  for (const auto& resource : shared_resources_) {
    resource->EnqueueTask(this);
  }
  for (const auto& resource : exclusive_resources_) {
    resource->EnqueueTask(this);
  }
}

ResourceTask::~ResourceTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(task_.is_null());
}

bool ResourceTask::CanExecute() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& resource : shared_resources_) {
    if (!resource->CanLock(/*exclusive=*/false)) {
      return false;
    }
    ResourceTask* task = resource->PeekTask();
    if (task && task != this) {
      return false;
    }
  }
  for (const auto& resource : exclusive_resources_) {
    if (!resource->CanLock(/*exclusive=*/true)) {
      return false;
    }
    ResourceTask* task = resource->PeekTask();
    if (task && task != this) {
      return false;
    }
  }

  return true;
}

void ResourceTask::Execute(bool dequeue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Popping this task from the resource queues might release the last reference
  // to this object. Make sure to save one of these references on the stack.
  scoped_refptr<ResourceTask> self;

  for (const auto& resource : shared_resources_) {
    if (dequeue) {
      self = resource->PopTask();
      CHECK_EQ(this, self.get());
    }
    resource->Lock(/*exclusive=*/false);
  }
  for (const auto& resource : exclusive_resources_) {
    if (dequeue) {
      self = resource->PopTask();
      CHECK_EQ(this, self.get());
    }
    resource->Lock(/*exclusive=*/true);
  }

  // `task_` may invoke the completion callback synchronously.
  std::move(task_).Run(base::BindOnce(&ResourceTask::Complete, this));
}

void ResourceTask::Complete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& resource : shared_resources_) {
    resource->Unlock();
  }
  for (const auto& resource : exclusive_resources_) {
    resource->Unlock();
  }

  for (const auto& resource : shared_resources_) {
    // A task that is waiting for a resource with a shared lock must want an
    // exclusive lock and only one such task can run at once so we can stop
    // after finding the first task.
    ResourceTask* task = resource->PeekTask();
    if (task && task->CanExecute()) {
      task->Execute(/*dequeue=*/true);
    }
  }
  for (const auto& resource : exclusive_resources_) {
    // Multiple tasks requiring a shared lock could be waiting for this resource
    // to be unlocked, so try to run as many executable tasks as possible.
    while (ResourceTask* task = resource->PeekTask()) {
      if (task->CanExecute()) {
        task->Execute(/*dequeue=*/true);
      } else {
        break;
      }
    }
  }
}

}  // namespace webnn
