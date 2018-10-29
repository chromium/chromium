// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/experimental/task.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_task.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/workers/experimental/task_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/experimental/thread_pool.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"

namespace blink {

ThreadPoolTask::ThreadPoolTask(ThreadPoolThreadProvider* thread_provider,
                               ScriptState* script_state,
                               const ScriptValue& function,
                               const Vector<ScriptValue>& arguments,
                               TaskType task_type)
    : ThreadPoolTask(thread_provider,
                     script_state,
                     function,
                     String(),
                     arguments,
                     task_type) {}

ThreadPoolTask::ThreadPoolTask(ThreadPoolThreadProvider* thread_provider,
                               ScriptState* script_state,
                               const String& function_name,
                               const Vector<ScriptValue>& arguments,
                               TaskType task_type)
    : ThreadPoolTask(thread_provider,
                     script_state,
                     ScriptValue(),
                     function_name,
                     arguments,
                     task_type) {}

ThreadPoolTask::ThreadPoolTask(ThreadPoolThreadProvider* thread_provider,
                               ScriptState* script_state,
                               const ScriptValue& function,
                               const String& function_name,
                               const Vector<ScriptValue>& arguments,
                               TaskType task_type)
    : task_type_(task_type),
      self_keep_alive_(base::AdoptRef(this)),
      resolver_(ScriptPromiseResolver::Create(script_state)),
      function_name_(function_name.IsolatedCopy()),
      arguments_(arguments.size()),
      weak_factory_(this) {
  DCHECK(IsMainThread());
  DCHECK_EQ(!function.IsEmpty(), function_name.IsNull());
  DCHECK(task_type_ == TaskType::kUserInteraction ||
         task_type_ == TaskType::kIdleTask);
  v8::Isolate* isolate = script_state->GetIsolate();

  // TODO(japhet): Handle serialization failures
  if (!function.IsEmpty()) {
    function_ = SerializedScriptValue::SerializeAndSwallowExceptions(
        isolate, function.V8Value()->ToString(isolate));
  }

  Vector<ThreadPoolTask*> prerequisites;
  Vector<size_t> prerequisites_indices;
  for (size_t i = 0; i < arguments_.size(); i++) {
    // Normal case: if the argument isn't a Task, just serialize it.
    if (!V8Task::hasInstance(arguments[i].V8Value(), isolate)) {
      arguments_[i].serialized_value =
          SerializedScriptValue::SerializeAndSwallowExceptions(
              isolate, arguments[i].V8Value());
      continue;
    }
    ThreadPoolTask* prerequisite =
        ToScriptWrappable(arguments[i].V8Value().As<v8::Object>())
            ->ToImpl<Task>()
            ->GetThreadPoolTask();
    prerequisites.push_back(prerequisite);
    prerequisites_indices.push_back(i);
  }

  worker_thread_ = SelectThread(prerequisites, thread_provider);
  worker_thread_->IncrementTasksInProgressCount();

  if (prerequisites.IsEmpty()) {
    MaybeStartTask();
    return;
  }

  // Prior to this point, other ThreadPoolTask instances don't have a reference
  // to |this| yet, so no need to lock mutex_. RegisterDependencies() populates
  // those references, so RegisterDependencies() and any logic thereafter must
  // consider the potential for data races.
  RegisterDependencies(prerequisites, prerequisites_indices);
}

// static
ThreadPoolThread* ThreadPoolTask::SelectThread(
    const Vector<ThreadPoolTask*>& prerequisites,
    ThreadPoolThreadProvider* thread_provider) {
  DCHECK(IsMainThread());
  HashCountedSet<ThreadPoolThread*> prerequisite_location_counts;
  size_t max_prerequisite_location_count = 0;
  ThreadPoolThread* max_prerequisite_thread = nullptr;
  for (ThreadPoolTask* prerequisite : prerequisites) {
    // For prerequisites that are not yet complete, track which thread the task
    // will run on. Put this task on the thread where the most prerequisites
    // reside. This is slightly imprecise, because a task may complete before
    // registering dependent tasks below.
    if (ThreadPoolThread* thread = prerequisite->GetScheduledThread()) {
      prerequisite_location_counts.insert(thread);
      unsigned thread_count = prerequisite_location_counts.count(thread);
      if (thread_count > max_prerequisite_location_count) {
        max_prerequisite_location_count = thread_count;
        max_prerequisite_thread = thread;
      }
    }
  }
  return max_prerequisite_thread ? max_prerequisite_thread
                                 : thread_provider->GetLeastBusyThread();
}

ThreadPoolThread* ThreadPoolTask::GetScheduledThread() {
  DCHECK(IsMainThread());
  MutexLocker lock(mutex_);
  return HasFinished() ? nullptr : worker_thread_;
}

// Should only be called from constructor. Split out in to a helper because
// clang appears to exempt constructors from thread safety analysis.
void ThreadPoolTask::RegisterDependencies(
    const Vector<ThreadPoolTask*>& prerequisites,
    const Vector<size_t>& prerequisites_indices) {
  DCHECK(IsMainThread());
  {
    MutexLocker lock(mutex_);
    prerequisites_remaining_ = prerequisites.size();
  }

  for (size_t i = 0; i < prerequisites.size(); i++) {
    ThreadPoolTask* prerequisite = prerequisites[i];
    size_t prerequisite_index = prerequisites_indices[i];
    scoped_refptr<SerializedScriptValue> result;
    State prerequisite_state = State::kPending;

    {
      MutexLocker lock(prerequisite->mutex_);
      prerequisite_state = prerequisite->state_;
      if (prerequisite->HasFinished()) {
        result = prerequisite->serialized_result_;
      } else {
        prerequisite->dependents_.insert(
            std::make_unique<Dependent>(this, prerequisite_index));
      }
    }

    // TODO(japhet): if a prerequisite failed, this task will be cancelled.
    // Should that throw an exception?
    if (prerequisite_state == State::kCompleted ||
        prerequisite_state == State::kFailed) {
      PrerequisiteFinished(prerequisite_index, v8::Local<v8::Value>(), result,
                           prerequisite_state);
    }
  }
}

ThreadPoolTask::~ThreadPoolTask() {
  DCHECK(IsMainThread());
  DCHECK(HasFinished());
  DCHECK(!function_);
  DCHECK(arguments_.IsEmpty());
  DCHECK(!prerequisites_remaining_);
  DCHECK(dependents_.IsEmpty());
}

void ThreadPoolTask::PrerequisiteFinished(
    size_t prerequisite_index,
    v8::Local<v8::Value> v8_result,
    scoped_refptr<SerializedScriptValue> result,
    State prerequisite_state) {
  MutexLocker lock(mutex_);
  DCHECK(state_ == State::kPending || state_ == State::kCancelPending);
  DCHECK(prerequisite_state == State::kCompleted ||
         prerequisite_state == State::kFailed);
  DCHECK_GT(prerequisites_remaining_, 0u);
  prerequisites_remaining_--;
  // If the result of the prerequisite doesn't need to move between threads,
  // save the deserialized v8::Value for later use.
  if (prerequisite_state == State::kFailed) {
    AdvanceState(State::kCancelPending);
  } else if (worker_thread_->IsCurrentThread() && !v8_result.IsEmpty()) {
    arguments_[prerequisite_index].v8_value =
        std::make_unique<ScopedPersistent<v8::Value>>(
            ToIsolate(worker_thread_->GlobalScope()), v8_result);
  } else {
    arguments_[prerequisite_index].serialized_value = result;
  }
  MaybeStartTask();
}

void ThreadPoolTask::MaybeStartTask() {
  if (prerequisites_remaining_)
    return;
  DCHECK(state_ == State::kPending || state_ == State::kCancelPending);
  PostCrossThreadTask(*worker_thread_->GetTaskRunner(task_type_), FROM_HERE,
                      CrossThreadBind(&ThreadPoolTask::StartTaskOnWorkerThread,
                                      CrossThreadUnretained(this)));
}

void ThreadPoolTask::StartTaskOnWorkerThread() {
  DCHECK(worker_thread_->IsCurrentThread());

  bool was_cancelled = false;
  {
    MutexLocker lock(mutex_);
    DCHECK(!prerequisites_remaining_);
    switch (state_) {
      case State::kPending:
        AdvanceState(State::kStarted);
        break;
      case State::kCancelPending:
        was_cancelled = true;
        break;
      case State::kStarted:
      case State::kCompleted:
      case State::kFailed:
        NOTREACHED();
        break;
    }
  }

  WorkerOrWorkletGlobalScope* global_scope = worker_thread_->GlobalScope();
  v8::Isolate* isolate = ToIsolate(global_scope);
  ScriptState::Scope scope(global_scope->ScriptController()->GetScriptState());

  v8::TryCatch block(isolate);
  v8::Local<v8::Value> return_value;
  if (was_cancelled) {
    return_value = V8String(isolate, "Task aborted");
  } else {
    return_value = RunTaskOnWorkerThread(isolate);
    if (return_value.IsEmpty()) {
      if (block.HasCaught())
        return_value = block.Exception()->ToString(isolate);
      else
        return_value = V8String(isolate, "Invalid task");
    }
  }

  scoped_refptr<SerializedScriptValue> local_result =
      SerializedScriptValue::SerializeAndSwallowExceptions(isolate,
                                                           return_value);
  State local_state =
      block.HasCaught() || was_cancelled ? State::kFailed : State::kCompleted;

  function_ = nullptr;
  arguments_.clear();

  HashSet<std::unique_ptr<Dependent>> dependents_to_notify;
  {
    MutexLocker lock(mutex_);
    serialized_result_ = local_result;
    AdvanceState(local_state);
    dependents_to_notify.swap(dependents_);
  }

  for (auto& dependent : dependents_to_notify) {
    dependent->task->PrerequisiteFinished(dependent->index, return_value,
                                          local_result, local_state);
  }

  PostCrossThreadTask(
      *worker_thread_->GetParentExecutionContextTaskRunners()->Get(
          TaskType::kInternalWorker),
      FROM_HERE,
      CrossThreadBind(&ThreadPoolTask::TaskCompleted,
                      CrossThreadUnretained(this)));
  // TaskCompleted may delete |this| at any time after this point.
}

v8::Local<v8::Value> ThreadPoolTask::RunTaskOnWorkerThread(
    v8::Isolate* isolate) {
  DCHECK(worker_thread_->IsCurrentThread());
  // No other thread should be touching function_ or arguments_ at this point,
  // so no mutex needed while actually running the task.
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Function> script_function;
  v8::Local<v8::Value> receiver;
  if (function_) {
    String core_script =
        "(" + ToCoreString(function_->Deserialize(isolate).As<v8::String>()) +
        ")";
    v8::MaybeLocal<v8::Script> script = v8::Script::Compile(
        isolate->GetCurrentContext(), V8String(isolate, core_script));
    script_function = script.ToLocalChecked()
                          ->Run(context)
                          .ToLocalChecked()
                          .As<v8::Function>();
    receiver = script_function;
  } else if (worker_thread_->IsWorklet()) {
    TaskWorkletGlobalScope* task_worklet_global_scope =
        static_cast<TaskWorkletGlobalScope*>(worker_thread_->GlobalScope());
    script_function = task_worklet_global_scope->GetProcessFunctionForName(
        function_name_, isolate);
    receiver =
        task_worklet_global_scope->GetInstanceForName(function_name_, isolate);
  }

  if (script_function.IsEmpty())
    return v8::Local<v8::Value>();

  Vector<v8::Local<v8::Value>> params(arguments_.size());
  for (size_t i = 0; i < arguments_.size(); i++) {
    DCHECK(!arguments_[i].serialized_value || !arguments_[i].v8_value);
    if (arguments_[i].serialized_value)
      params[i] = arguments_[i].serialized_value->Deserialize(isolate);
    else
      params[i] = arguments_[i].v8_value->NewLocal(isolate);
  }

  v8::MaybeLocal<v8::Value> ret =
      script_function->Call(context, receiver, params.size(), params.data());

  v8::Local<v8::Value> return_value;
  if (!ret.IsEmpty()) {
    return_value = ret.ToLocalChecked();
    if (return_value->IsPromise())
      return_value = return_value.As<v8::Promise>()->Result();
  }
  return return_value;
}

void ThreadPoolTask::TaskCompleted() {
  DCHECK(IsMainThread());
  bool rejected = false;
  {
    MutexLocker lock(mutex_);
    DCHECK(HasFinished());
    rejected = state_ == State::kFailed;
  }

  ScriptState* script_state = resolver_->GetScriptState();
  if (script_state->ContextIsValid()) {
    ScriptState::Scope scope(script_state);
    v8::Local<v8::Value> value;
    {
      MutexLocker lock(mutex_);
      value = serialized_result_->Deserialize(script_state->GetIsolate());
    }
    if (rejected)
      resolver_->Reject(v8::Exception::Error(value.As<v8::String>()));
    else
      resolver_->Resolve(value);
  }
  worker_thread_->DecrementTasksInProgressCount();
  self_keep_alive_.reset();
  // |this| may be deleted here.
}

ScriptPromise ThreadPoolTask::GetResult() {
  DCHECK(IsMainThread());
  return resolver_->Promise();
}

void ThreadPoolTask::Cancel() {
  DCHECK(IsMainThread());
  MutexLocker lock(mutex_);
  if (state_ == State::kPending)
    AdvanceState(State::kCancelPending);
}

void ThreadPoolTask::AdvanceState(State new_state) {
  switch (new_state) {
    case State::kPending:
      NOTREACHED() << "kPending should only be set via initialization";
      break;
    case State::kStarted:
      DCHECK_EQ(State::kPending, state_);
      break;
    case State::kCancelPending:
      DCHECK(state_ == State::kPending || state_ == State::kCancelPending);
      break;
    case State::kCompleted:
      DCHECK_EQ(State::kStarted, state_);
      break;
    case State::kFailed:
      DCHECK(state_ == State::kStarted || state_ == State::kCancelPending);
      break;
  }
  state_ = new_state;
}

}  // namespace blink
