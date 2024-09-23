// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_QUEUEABLE_RESOURCE_STATE_H_
#define SERVICES_WEBNN_QUEUEABLE_RESOURCE_STATE_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "services/webnn/queueable_resource_state_base.h"

namespace webnn {

// Manages the state of a resource to be executed in a `ResourceTask`.
// This class enforces that the underlying resource may only be accessed while
// executing in a `ResourceTask`.
//
// Example use:
//
//   auto task = base::MakeRefCounted<ResourceTask>(
//       /*shared_resources=*/{my_resource_state},
//       /*exclusive_resources=*/{},
//       base::BindOnce(
//           [](scoped_refptr<QueueableResourceState<MyResourceType>> state,
//              base::OnceClosure completion_closure) {
//             // We have a shared lock on the resource while until
//             // `completion_closure` is run. We can safely get a reference to
//             // the underlying resource.
//             const MyResourceType& resource = state.GetSharedLockedResource();
//             // Do something with `resource`...
//           },
//           my_resource_state));
//   task->Enqueue();
//
// This class is reference counted so that operations that are in progress will
// keep the resources they are using alive until they complete. This class may
// not be passed between threads.
template <typename ResourceType>
class QueueableResourceState : public QueueableResourceStateBase {
 public:
  explicit QueueableResourceState(std::unique_ptr<ResourceType> resource)
      : resource_content_(std::move(resource)) {}

  // Get const access to the underlying resource. This method may only be
  // called within a `ResourceTask` which took a shared lock on this state.
  const ResourceType& GetSharedLockedResource() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(IsLockedShared());
    return *resource_content_.get();
  }

  // Get mutable access to the underlying resource. This method may only be
  // called within a `ResourceTask` which took an exclusive lock on this state.
  ResourceType* GetExclusivelyLockedResource() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(IsLockedExclusive());
    return resource_content_.get();
  }

 private:
  ~QueueableResourceState() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  const std::unique_ptr<ResourceType> resource_content_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_QUEUEABLE_RESOURCE_STATE_H_
