// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"

#include <memory>
#include <utility>

#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// Owns a task posted by `PostCrossThreadCleanupTask()`. If the task queue it
// was posted to is shut down before it runs, the wrapper is destroyed without
// running and reposts the task to `fallback_task_runner_` instead. This is the
// same approach as `DeleteHelper` in blink_scheduler_single_thread_task_runner.
class CleanupTaskWithFallback final {
  USING_FAST_MALLOC(CleanupTaskWithFallback);

 public:
  CleanupTaskWithFallback(
      const base::Location& location,
      CrossThreadOnceClosure task,
      scoped_refptr<base::SingleThreadTaskRunner> fallback_task_runner)
      : location_(location),
        task_(std::move(task)),
        fallback_task_runner_(std::move(fallback_task_runner)) {}

  CleanupTaskWithFallback(const CleanupTaskWithFallback&) = delete;
  CleanupTaskWithFallback& operator=(const CleanupTaskWithFallback&) = delete;

  ~CleanupTaskWithFallback() {
    if (task_) {
      PostCrossThreadTask(*fallback_task_runner_, location_, std::move(task_));
    }
  }

  void Run() { std::move(task_).Run(); }

 private:
  const base::Location location_;
  CrossThreadOnceClosure task_;
  const scoped_refptr<base::SingleThreadTaskRunner> fallback_task_runner_;
};

}  // namespace

ParentExecutionContextTaskRunners* ParentExecutionContextTaskRunners::Create(
    ExecutionContext& context) {
  return MakeGarbageCollected<ParentExecutionContextTaskRunners>(context);
}

ParentExecutionContextTaskRunners::ParentExecutionContextTaskRunners(
    ExecutionContext& context)
    : ExecutionContextLifecycleObserver(&context),
      cleanup_task_runner_(ThreadScheduler::Current()->CleanupTaskRunner()) {
  DCHECK(context.IsContextThread());
  // For now we only support very limited task types. Sort in the TaskType enum
  // value order.
  for (auto type : {TaskType::kNetworking, TaskType::kPostedMessage,
                    TaskType::kWorkerAnimation, TaskType::kInternalDefault,
                    TaskType::kInternalLoading, TaskType::kInternalTest,
                    TaskType::kInternalMedia, TaskType::kInternalInspector}) {
    task_runners_.insert(type, context.GetTaskRunner(type));
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
ParentExecutionContextTaskRunners::Get(TaskType type) {
  base::AutoLock locker(lock_);
  return task_runners_.at(type);
}

void ParentExecutionContextTaskRunners::PostCrossThreadCleanupTask(
    TaskType type,
    const base::Location& location,
    CrossThreadOnceClosure task) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner = Get(type);
  // If task_runner has been set to cleanup_task_runner_, that means
  // ContextDestroyed() has run, so the task does not need a fallback wrapper.
  if (task_runner == cleanup_task_runner_) {
    PostCrossThreadTask(*task_runner, location, std::move(task));
    return;
  }
  PostCrossThreadTask(
      *task_runner, location,
      CrossThreadBindOnce(
          &CleanupTaskWithFallback::Run,
          std::make_unique<CleanupTaskWithFallback>(location, std::move(task),
                                                    cleanup_task_runner_)));
}

void ParentExecutionContextTaskRunners::Trace(Visitor* visitor) const {
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void ParentExecutionContextTaskRunners::ContextDestroyed() {
  base::AutoLock locker(lock_);
  for (auto& entry : task_runners_) {
    entry.value = cleanup_task_runner_;
  }
}

}  // namespace blink
