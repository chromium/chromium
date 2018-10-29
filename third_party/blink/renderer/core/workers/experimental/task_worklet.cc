// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/experimental/task_worklet.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/experimental/thread_pool_thread.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_object_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"

namespace blink {

class TaskWorkletMessagingProxy final : public ThreadedWorkletMessagingProxy {
 public:
  TaskWorkletMessagingProxy(ExecutionContext* context)
      : ThreadedWorkletMessagingProxy(context) {}
  ~TaskWorkletMessagingProxy() override = default;

  std::unique_ptr<WorkerThread> CreateWorkerThread() override {
    return std::make_unique<ThreadPoolThread>(GetExecutionContext(),
                                              WorkletObjectProxy(),
                                              ThreadPoolThread::kWorklet);
  }

  ThreadPoolThread* GetWorkerThread() const {
    return static_cast<ThreadPoolThread*>(
        ThreadedMessagingProxyBase::GetWorkerThread());
  }
};

const char TaskWorklet::kSupplementName[] = "TaskWorklet";

TaskWorklet* TaskWorklet::From(LocalDOMWindow& window) {
  TaskWorklet* task_worklet =
      Supplement<LocalDOMWindow>::From<TaskWorklet>(window);
  if (!task_worklet) {
    task_worklet = new TaskWorklet(window.document());
    Supplement<LocalDOMWindow>::ProvideTo(window, task_worklet);
  }
  return task_worklet;
}

static const size_t kMaxTaskWorkletThreads = 4;

TaskWorklet::TaskWorklet(Document* document) : Worklet(document) {}

Task* TaskWorklet::postTask(ScriptState* script_state,
                            const ScriptValue& function,
                            const Vector<ScriptValue>& arguments) {
  DCHECK(function.IsFunction());
  // TODO(japhet): Here and below: it's unclear what task type should be used,
  // and whether the API should allow it to be configured. Using kIdleTask as a
  // placeholder for now.
  ThreadPoolTask* thread_pool_task = new ThreadPoolTask(
      this, script_state, function, arguments, TaskType::kIdleTask);
  return new Task(thread_pool_task);
}

Task* TaskWorklet::postTask(ScriptState* script_state,
                            const String& function_name,
                            const Vector<ScriptValue>& arguments) {
  ThreadPoolTask* thread_pool_task = new ThreadPoolTask(
      this, script_state, function_name, arguments, TaskType::kIdleTask);
  return new Task(thread_pool_task);
}

ThreadPoolThread* TaskWorklet::GetLeastBusyThread() {
  DCHECK(IsMainThread());
  ThreadPoolThread* least_busy_thread = nullptr;
  size_t lowest_task_count = std::numeric_limits<std::size_t>::max();
  for (auto& proxy : proxies_) {
    ThreadPoolThread* current_thread =
        static_cast<TaskWorkletMessagingProxy*>(proxy.Get())->GetWorkerThread();
    size_t current_task_count = current_thread->GetTasksInProgressCount();
    // If there's an idle thread, use it.
    if (current_task_count == 0)
      return current_thread;
    if (current_task_count < lowest_task_count) {
      least_busy_thread = current_thread;
      lowest_task_count = current_task_count;
    }
  }
  if (proxies_.size() == kMaxTaskWorkletThreads)
    return least_busy_thread;
  auto* proxy = static_cast<TaskWorkletMessagingProxy*>(CreateGlobalScope());
  proxies_.push_back(proxy);
  return proxy->GetWorkerThread();
}

// TODO(japhet): This causes all of the backing threads to be created when
// addModule() is first called. Sort out lazy global scope creation.
// Note that if the function variant of postTask() is called first, global
// scopes will be created lazily; it's only module loading that needs upfront
// global scope creation, presumably because we don't have a way to replay
// module loads from WorkletModuleResponsesMap yet.
bool TaskWorklet::NeedsToCreateGlobalScope() {
  return GetNumberOfGlobalScopes() < kMaxTaskWorkletThreads;
}

WorkletGlobalScopeProxy* TaskWorklet::CreateGlobalScope() {
  DCHECK_LT(GetNumberOfGlobalScopes(), kMaxTaskWorkletThreads);
  TaskWorkletMessagingProxy* proxy =
      new TaskWorkletMessagingProxy(GetExecutionContext());
  proxy->Initialize(WorkerClients::Create(), ModuleResponsesMap(),
                    WorkerBackingThreadStartupData::CreateDefault());
  return proxy;
}

// We select a global scope without this getting called.
wtf_size_t TaskWorklet::SelectGlobalScope() {
  NOTREACHED();
  return 0u;
}

void TaskWorklet::Trace(blink::Visitor* visitor) {
  Worklet::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
