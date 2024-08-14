// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_RESOURCE_TASK_H_
#define SERVICES_WEBNN_RESOURCE_TASK_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"

namespace webnn {

class QueueableResourceStateBase;

// Represents a task performed against one or more `QueueableResourceState`
// instances. See `QueueableResourceState` for more details.
class ResourceTask : public base::RefCounted<ResourceTask> {
 public:
  // Create a new `task` which will run when the requested locks may be acquired
  // to each of the given resources.
  //
  // The underlying resources are kept alive by this `ResourceTask` until the
  // `task`'s completion closure is run.
  ResourceTask(
      std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources,
      std::vector<scoped_refptr<QueueableResourceStateBase>>
          exclusive_resources,
      base::OnceCallback<void(base::OnceClosure)> task);

  // Checks if the require resources can be locked. If so they are and
  // `task` is run immediately, otherwise this task is added to the
  // queues for each of the resources and `task` will be run when they
  // can be locked.
  void Enqueue();

 private:
  friend class base::RefCounted<ResourceTask>;

  ~ResourceTask();

  bool CanExecute();
  void Execute(bool dequeue);
  void Complete();

  const std::vector<scoped_refptr<QueueableResourceStateBase>>
      shared_resources_;
  const std::vector<scoped_refptr<QueueableResourceStateBase>>
      exclusive_resources_;
  base::OnceCallback<void(base::OnceClosure)> task_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_RESOURCE_TASK_H_
