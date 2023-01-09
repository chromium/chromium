// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

InspectorTaskRunner::InspectorTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> isolate_task_runner)
    : isolate_task_runner_(isolate_task_runner), task_queue_cv_(&lock_) {}

InspectorTaskRunner::~InspectorTaskRunner() = default;

void InspectorTaskRunner::InitIsolate(v8::Isolate* isolate) {
  base::AutoLock locker(lock_);
  isolate_ = isolate;
}

void InspectorTaskRunner::Dispose() {
  base::AutoLock locker(lock_);
  disposed_ = true;
  isolate_ = nullptr;
  isolate_task_runner_ = nullptr;
  task_queue_cv_.Broadcast();
}

bool InspectorTaskRunner::AppendTask(Task task) {
  base::AutoLock locker(lock_);
  if (disposed_)
    return false;
  interrupting_task_queue_.push_back(std::move(task));
  PostCrossThreadTask(
      *isolate_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &InspectorTaskRunner::PerformSingleInterruptingTaskDontWait,
          WrapRefCounted(this)));
  if (isolate_) {
    AddRef();
    isolate_->RequestInterrupt(&V8InterruptCallback, this);
  }
  task_queue_cv_.Signal();
  return true;
}

bool InspectorTaskRunner::AppendTaskDontInterrupt(Task task) {
  base::AutoLock locker(lock_);
  if (disposed_)
    return false;
  PostCrossThreadTask(*isolate_task_runner_, FROM_HERE, std::move(task));
  return true;
}

void InspectorTaskRunner::ProcessInterruptingTasks() {
  while (true) {
    InspectorTaskRunner::Task task = WaitForNextInterruptingTaskOrQuitRequest();
    if (!task) {
      break;
    }
    std::move(task).Run();
  }
}

void InspectorTaskRunner::RequestQuitProcessingInterruptingTasks() {
  base::AutoLock locker(lock_);
  quit_requested_ = true;
  task_queue_cv_.Broadcast();
}

InspectorTaskRunner::Task
InspectorTaskRunner::WaitForNextInterruptingTaskOrQuitRequest() {
  base::AutoLock locker(lock_);

  while (!quit_requested_ && !disposed_) {
    if (!interrupting_task_queue_.empty()) {
      return interrupting_task_queue_.TakeFirst();
    }
    task_queue_cv_.Wait();
  }
  quit_requested_ = false;
  return Task();
}

InspectorTaskRunner::Task InspectorTaskRunner::TakeNextInterruptingTask() {
  base::AutoLock locker(lock_);

  if (disposed_ || interrupting_task_queue_.empty())
    return Task();

  return interrupting_task_queue_.TakeFirst();
}

void InspectorTaskRunner::PerformSingleInterruptingTaskDontWait() {
  Task task = TakeNextInterruptingTask();
  if (task) {
    DCHECK(isolate_task_runner_->BelongsToCurrentThread());
    std::move(task).Run();
  }
}

void InspectorTaskRunner::V8InterruptCallback(v8::Isolate*, void* data) {
  InspectorTaskRunner* runner = static_cast<InspectorTaskRunner*>(data);
  Task task = runner->TakeNextInterruptingTask();
  runner->Release();
  if (!task) {
    return;
  }
  std::move(task).Run();
}

}  // namespace blink
