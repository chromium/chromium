// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_REQUEST_H_
#define REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_REQUEST_H_

#include <memory>
#include <utility>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/grpc/src/include/grpcpp/client_context.h"
#include "third_party/grpc/src/include/grpcpp/support/status.h"

namespace grpc_impl {
class CompletionQueue;
}  // namespace grpc_impl

namespace remoting {

// The GrpcAsyncRequest base class that holds logic invariant to the response
// type.
//
// The lifetime of GrpcAsyncRequest is bound to the completion queue. A
// subclass may enqueue itself multiple times into the completion queue, and
// GrpcAsyncExecutor will dequeue it and call OnDequeue() when the event is
// handled. If the subclass won't re-enqueue itself, OnDequeue() should return
// false, which will delete the request object.
//
// All methods are called from the same sequence, but the destructor could end
// up being called on an arbitrary sequence if the caller's sequence itself has
// been destroyed.
class GrpcAsyncRequest {
 public:
  using RunTaskCallback = base::RepeatingCallback<void(base::OnceClosure)>;

  GrpcAsyncRequest();
  virtual ~GrpcAsyncRequest();

  grpc::ClientContext* context() { return &context_; }

  // Methods below are considered internal to grpc_support.

  // Force dequeues any pending request.
  void CancelRequest();

  // Starts the request. Subclass can store |cq| but should only use |event_tag|
  // before the method returns.
  // Subclass shall only run callbacks using |run_task_cb|. Directly running
  // task in OnDequeue() might result in concurrency issue.
  virtual void Start(const RunTaskCallback& run_task_cb,
                     grpc_impl::CompletionQueue* cq,
                     void* event_tag) = 0;

  // Called when the request has been dequeued from the completion queue.
  // Returns true iff the task is not finished, and Reenqueue() should be
  // called.
  virtual bool OnDequeue(bool operation_succeeded) = 0;

  // Re-enqueues the request. This is only called if the previous call to
  // OnDequeue() returns true. Subclass shall not store |event_tag|.
  virtual void Reenqueue(void* event_tag) = 0;

  // Called before calling Start(). If this returns false then Start() won't be
  // called and request will be silently dropped.
  virtual bool CanStartRequest() const = 0;

  base::WeakPtr<GrpcAsyncRequest> GetGrpcAsyncRequestWeakPtr();

 protected:
  // Called after CancelRequest() is called.
  virtual void OnRequestCanceled() = 0;

  grpc::Status status_{grpc::StatusCode::UNKNOWN, "Uninitialized"};

 private:
  grpc_impl::ClientContext context_;

  base::WeakPtrFactory<GrpcAsyncRequest> grpc_async_request_weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncRequest);
};

}  // namespace remoting

#endif  // REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_REQUEST_H_
