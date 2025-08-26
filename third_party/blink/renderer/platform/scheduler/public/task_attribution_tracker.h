// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_TRACKER_H_

#include <optional>
#include <utility>

#include "base/functional/function_ref.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
class SoftNavigationContext;
class TaskAttributionTaskState;
class WebSchedulingTaskState;
}  // namespace blink

namespace v8 {
class Isolate;
}  // namespace v8

namespace blink::scheduler {

class TaskAttributionInfo;
class TaskAttributionTrackerImpl;

// This public interface enables platform/ and core/ callers to create a task
// scope on the one hand, and check on the ID of the currently running task as
// well as its ancestry on the other.
class PLATFORM_EXPORT TaskAttributionTracker {
 public:
  enum class TaskScopeType {
    kCallback,
    kScheduledAction,
    kScriptExecution,
    kPostMessage,
    kPopState,
    kSchedulerPostTask,
    kRequestIdleCallback,
    kXMLHttpRequest,
    kSoftNavigation,
    kMiscEvent,
  };

  // `TaskScope` stores state for the current task, which is propagated to tasks
  // and promise reactions created with in the scope. `TaskScope`s are meant
  // to be only used for JavaScript execution, and "task" here approximately
  // means "the current JavaScript execution, excluding microtasks", which
  // roughly aligns with a top-level JS callback.
  class TaskScope final {
    STACK_ALLOCATED();

   public:
    ~TaskScope() {
      if (task_tracker_) {
        task_tracker_->OnTaskScopeDestroyed(*this);
      }
    }

    TaskScope(TaskScope&& other)
        : task_tracker_(std::exchange(other.task_tracker_, nullptr)),
          previous_task_state_(other.previous_task_state_) {}

    TaskScope& operator=(TaskScope&& other) {
      task_tracker_ = std::exchange(other.task_tracker_, nullptr);
      previous_task_state_ = other.previous_task_state_;
      return *this;
    }

   private:
    // Only `TaskAttributionTrackerImpl` can create `TaskScope`s.
    friend class TaskAttributionTrackerImpl;

    TaskScope(TaskAttributionTracker* tracker,
              TaskAttributionTaskState* previous_task_state)
        : task_tracker_(tracker),
          previous_task_state_(previous_task_state) {}

    // `task_tracker_` is tied to the lifetime of the isolate, which will
    // outlive the current task.
    TaskAttributionTracker* task_tracker_;

    // `previous_task_state_` is on the Oilpan heap, so this is stored as a raw
    // pointer since the class is stack allocated.
    TaskAttributionTaskState* previous_task_state_;
  };

  static TaskAttributionTracker* From(v8::Isolate* isolate) {
    return V8PerIsolateData::From(isolate)->GetTaskAttributionTracker();
  }

  virtual ~TaskAttributionTracker() = default;

  // Sets `task_state` as the current task state if `task_state` is non-null and
  // JavaScript is not currently executing. Returns a `TaskScope`  initiating
  // propagation for `task_state` if the current task state was updated, making
  // it the current task state as long as the `TaskScope` it returns is the
  // topmost `TaskScope` on the stack. Otherwise returns std::nullopt.
  //
  // This method is used to propagate existing (unchanged) state through async
  // APIs. This should be used in cases where the propagation might be overwrite
  // existing state, e.g. synchronous event dispatch or synchronous <script>
  // execution.
  //
  // Note: This returns std::nullopt if a v8::Context was entered before calling
  // this, so care must be taken about ordering.
  virtual std::optional<TaskScope> SetCurrentTaskStateIfTopLevel(
      TaskAttributionInfo* task_state,
      TaskScopeType type) = 0;

  // Initiates propagation of the given `WebSchedulingTaskState`, making it the
  // current task state as long as the `TaskScope` it returns is the topmost on
  // the stack.
  //
  // This should only be used for prioritized tasks associated with web
  // scheduling APIs (scheduler.postTask() and requestIdleCallback()), and this
  // is not allowed to be called with JavaScript on the stack.
  virtual TaskScope SetCurrentTaskState(WebSchedulingTaskState* task_state,
                                        TaskScopeType type) = 0;

  // Initiates propagation of the given `SoftNavigationContext`, which will be
  // propagated to (promise) continuations and through async APIs participating
  // in task attribution while the returned `TaskScope` is the topmost on the
  // stack.
  //
  // This is used to set an individual `TaskAttributionInfo` variable, forking
  // the existing `CurrentTaskState()` if necessary.
  virtual TaskScope SetTaskStateVariable(SoftNavigationContext*) = 0;

  // Get the `TaskAttributionInfo` for the currently running task.
  virtual TaskAttributionInfo* CurrentTaskState() const = 0;

  // Registers the current task state as being associated with a same-document
  // navigation, managing its lifetime until the navigation is committed
  // or aborted. Returns the `TaskAttributionId` associated with the current
  // task state, if any.
  virtual std::optional<scheduler::TaskAttributionId>
  AsyncSameDocumentNavigationStarted() = 0;

  // Returns the task state for the `TaskAttributionId`, which is associated
  // with a same-document navigation. Clears the tracked task state associated
  // with this and any previous pending same-document navigations.
  virtual TaskAttributionInfo* CommitSameDocumentNavigation(
      TaskAttributionId) = 0;

  // Clears all tracked task state associated with same-document navigations.
  virtual void ResetSameDocumentNavigationTasks() = 0;

 protected:
  virtual void OnTaskScopeDestroyed(const TaskScope&) = 0;
};

}  // namespace blink::scheduler

namespace blink {
using TaskScopeType = scheduler::TaskAttributionTracker::TaskScopeType;
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_TRACKER_H_
