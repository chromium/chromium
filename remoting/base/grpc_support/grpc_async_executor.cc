// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/grpc_support/grpc_async_executor.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "remoting/base/grpc_support/grpc_async_request.h"
#include "third_party/grpc/src/include/grpcpp/completion_queue.h"

namespace remoting {

namespace {

using DequeueCallback = base::OnceCallback<void(bool operation_succeeded)>;

struct DispatchTask {
  scoped_refptr<base::SequencedTaskRunner> caller_sequence_task_runner;
  DequeueCallback callback;
};

// Helper class that is shared by all GrpcAsyncExecutors to run the completion
// queue and dispatch tasks back to the right executor.
// When enqueueing, caller should create a DispatchTask and enqueue it as the
// event_tag. The ownership of the object will be taken by the
// CompletionQueueDispatcher.
class CompletionQueueDispatcher {
 public:
  CompletionQueueDispatcher();
  ~CompletionQueueDispatcher();

  static CompletionQueueDispatcher* GetInstance();

  grpc::CompletionQueue* completion_queue() { return &completion_queue_; }

 private:
  void RunQueueOnDispatcherThread();

  // TODO(yuweih): Consider using task scheduler instead.
  // We need a dedicated thread because getting response from the completion
  // queue will block until any response is received. Note that the RPC call
  // itself is still async, meaning any new RPC call when the queue is blocked
  // can still be made, and can unblock the queue once the response is ready.
  base::Thread dispatcher_thread_{"grpc_completion_queue_dispatcher"};

  // Note that the gRPC library is thread-safe.
  grpc::CompletionQueue completion_queue_;

  DISALLOW_COPY_AND_ASSIGN(CompletionQueueDispatcher);
};

CompletionQueueDispatcher::CompletionQueueDispatcher() {
  dispatcher_thread_.Start();
  dispatcher_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CompletionQueueDispatcher::RunQueueOnDispatcherThread,
                     base::Unretained(this)));
}

CompletionQueueDispatcher::~CompletionQueueDispatcher() = default;

// static
CompletionQueueDispatcher* CompletionQueueDispatcher::GetInstance() {
  static base::NoDestructor<CompletionQueueDispatcher> dispatcher;
  return dispatcher.get();
}

void CompletionQueueDispatcher::RunQueueOnDispatcherThread() {
  void* event_tag;
  bool operation_succeeded = false;

  // completion_queue_.Next() blocks until a response is received.
  while (completion_queue_.Next(&event_tag, &operation_succeeded)) {
    DispatchTask* task = reinterpret_cast<DispatchTask*>(event_tag);
    task->caller_sequence_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(task->callback), operation_succeeded));
    delete task;
  }
}

}  // namespace

GrpcAsyncExecutor::GrpcAsyncExecutor() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GrpcAsyncExecutor::~GrpcAsyncExecutor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "# of pending RPCs at destruction: " << pending_requests_.size();
  CancelPendingRequests();
}

void GrpcAsyncExecutor::ExecuteRpc(std::unique_ptr<GrpcAsyncRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* unowned_request = request.get();
  DCHECK(FindRequest(unowned_request) == pending_requests_.end());
  auto task = std::make_unique<DispatchTask>();
  task->caller_sequence_task_runner = base::SequencedTaskRunnerHandle::Get();
  task->callback =
      base::BindOnce(&GrpcAsyncExecutor::OnDequeue, weak_factory_.GetWeakPtr(),
                     std::move(request));
  if (!unowned_request->CanStartRequest()) {
    VLOG(1) << "RPC is canceled before execution: " << unowned_request;
    return;
  }
  VLOG(1) << "Enqueuing RPC: " << unowned_request;

  // User can potentially delete the executor in the callback, so we should
  // delay it to prevent race condition. We also bind the closure with the
  // WeakPtr of this object to make sure it won't run after the executor is
  // deleted.
  auto run_task_cb = base::BindRepeating(
      &GrpcAsyncExecutor::PostTaskToRunClosure, weak_factory_.GetWeakPtr());

  unowned_request->Start(
      run_task_cb, CompletionQueueDispatcher::GetInstance()->completion_queue(),
      task.release());

  // You might think that we can invert the ownership and make GrpcAsyncExecutor
  // own the request instead, but this doesn't work because the gRPC completion
  // queue (which runs on a different thread) expects the client context to be
  // alive when you try to pop out a completed/dead event.
  pending_requests_.push_back(unowned_request->GetGrpcAsyncRequestWeakPtr());
}

void GrpcAsyncExecutor::CancelPendingRequests() {
  VLOG(1) << "Canceling # of pending requests: " << pending_requests_.size();
  // Drop pending response callbacks.
  weak_factory_.InvalidateWeakPtrs();
  for (auto& pending_request : pending_requests_) {
    // If the sequence itself is being destroyed, pending tasks will be dropped
    // in arbitrary order without checking the weak ptr. If the dequeue task is
    // destroyed earlier than the executor itself, then |pending_request| will
    // already be destroyed.
    if (pending_request) {
      pending_request->CancelRequest();
    }
  }
  pending_requests_.clear();
}

void GrpcAsyncExecutor::OnDequeue(std::unique_ptr<GrpcAsyncRequest> request,
                                  bool operation_succeeded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!request->OnDequeue(operation_succeeded)) {
    VLOG(1) << "Dequeuing RPC: " << request.get();
    auto iter = FindRequest(request.get());
    DCHECK(iter != pending_requests_.end());
    pending_requests_.erase(iter);
    return;
  }

  VLOG(1) << "Re-enqueuing RPC: " << request.get();
  DCHECK(FindRequest(request.get()) != pending_requests_.end());
  auto* unowned_request = request.get();
  auto task = std::make_unique<DispatchTask>();
  task->caller_sequence_task_runner = base::SequencedTaskRunnerHandle::Get();
  task->callback =
      base::BindOnce(&GrpcAsyncExecutor::OnDequeue, weak_factory_.GetWeakPtr(),
                     std::move(request));
  unowned_request->Reenqueue(task.release());
}

void GrpcAsyncExecutor::PostTaskToRunClosure(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&GrpcAsyncExecutor::RunClosure, weak_factory_.GetWeakPtr(),
                     std::move(closure)));
}

void GrpcAsyncExecutor::RunClosure(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(closure).Run();
}

GrpcAsyncExecutor::PendingRequestListIter GrpcAsyncExecutor::FindRequest(
    GrpcAsyncRequest* request) {
  return std::find_if(
      pending_requests_.begin(), pending_requests_.end(),
      [request](const base::WeakPtr<GrpcAsyncRequest>& current_request) {
        return current_request.get() == request;
      });
}

}  // namespace remoting
