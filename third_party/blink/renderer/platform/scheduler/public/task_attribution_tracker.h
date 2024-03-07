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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
class AbortSignal;
class DOMTaskSignal;
class ScriptState;
class ScriptWrappableTaskState;
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
          script_state_(other.script_state_),
          previous_running_task_(other.previous_running_task_),
          previous_continuation_task_state_(
              other.previous_continuation_task_state_) {}

    TaskScope& operator=(TaskScope&& other) {
      task_tracker_ = std::exchange(other.task_tracker_, nullptr);
      script_state_ = other.script_state_;
      previous_running_task_ = other.previous_running_task_;
      previous_continuation_task_state_ =
          other.previous_continuation_task_state_;
      return *this;
    }

   private:
    // Only `TaskAttributionTrackerImpl` can create `TaskScope`s.
    friend class TaskAttributionTrackerImpl;

    TaskScope(TaskAttributionTracker* tracker,
              ScriptState* script_state,
              TaskAttributionInfo* previous_running_task,
              ScriptWrappableTaskState* previous_continuation_task_state)
        : task_tracker_(tracker),
          script_state_(script_state),
          previous_running_task_(previous_running_task),
          previous_continuation_task_state_(previous_continuation_task_state) {}

    // `task_tracker_` is tied to the lifetime of the isolate, which will
    // outlive the current task.
    TaskAttributionTracker* task_tracker_;

    // The rest are on the Oilpan heap, so these are stored as raw pointers
    // since the class is stack allocated.
    ScriptState* script_state_;
    TaskAttributionInfo* previous_running_task_;
    ScriptWrappableTaskState* previous_continuation_task_state_;
  };

  class Observer : public GarbageCollectedMixin {
   public:
    virtual void OnCreateTaskScope(TaskAttributionInfo&) = 0;
  };

  class PLATFORM_EXPORT ObserverScope {
    STACK_ALLOCATED();

   public:
    ObserverScope(ObserverScope&& other)
        : task_tracker_(std::exchange(other.task_tracker_, nullptr)),
          previous_observer_(std::exchange(other.previous_observer_, nullptr)) {
    }

    ObserverScope& operator=(ObserverScope&& other) {
      task_tracker_ = std::exchange(other.task_tracker_, nullptr);
      previous_observer_ = std::exchange(other.previous_observer_, nullptr);
      return *this;
    }

    ~ObserverScope() {
      if (task_tracker_) {
        task_tracker_->OnObserverScopeDestroyed(*this);
      }
    }

   private:
    friend class TaskAttributionTrackerImpl;

    ObserverScope(TaskAttributionTracker* tracker,
                  Observer* observer,
                  Observer* previous_observer)
        : task_tracker_(tracker), previous_observer_(previous_observer) {}

    Observer* PreviousObserver() const { return previous_observer_; }

    TaskAttributionTracker* task_tracker_;
    Observer* previous_observer_;
  };

  static TaskAttributionTracker* From(v8::Isolate* isolate) {
    return V8PerIsolateData::From(isolate)->GetTaskAttributionTracker();
  }

  virtual ~TaskAttributionTracker() = default;

  // Create a new task scope.
  virtual TaskScope CreateTaskScope(ScriptState*,
                                    TaskAttributionInfo* parent_task,
                                    TaskScopeType type) = 0;
  // Create a new task scope with web scheduling context.
  virtual TaskScope CreateTaskScope(ScriptState*,
                                    TaskAttributionInfo* parent_task,
                                    TaskScopeType type,
                                    AbortSignal* abort_source,
                                    DOMTaskSignal* priority_source) = 0;

  // Get the `TaskAttributionInfo` for the currently running task.
  virtual TaskAttributionInfo* RunningTask() const = 0;

  // Returns true iff `task` has an ancestor task with `ancestor_id`.
  virtual bool IsAncestor(const TaskAttributionInfo& task,
                          TaskAttributionId anscestor_id) = 0;

  // Runs `visitor` for each ancestor `TaskAttributionInfo` of `task`. `visitor`
  // controls iteration with its return value.
  enum class IterationStatus { kContinue, kStop };
  virtual void ForEachAncestor(
      const TaskAttributionInfo& task,
      base::FunctionRef<IterationStatus(const TaskAttributionInfo& task)>
          visitor) = 0;

  // Registers an observer to be notified when a `TaskScope` has been created.
  // Multiple `Observer`s can be registered, but only the innermost one will
  // receive callbacks.
  virtual ObserverScope RegisterObserver(Observer* observer) = 0;

  // Setter and getter for a pointer to a pending same-document navigation task,
  // to ensure the task's lifetime.
  virtual void AddSameDocumentNavigationTask(TaskAttributionInfo* task) = 0;
  virtual void ResetSameDocumentNavigationTasks() = 0;
  virtual TaskAttributionInfo* CommitSameDocumentNavigation(
      TaskAttributionId) = 0;

 protected:
  virtual void OnTaskScopeDestroyed(const TaskScope&) = 0;
  virtual void OnObserverScopeDestroyed(const ObserverScope&) = 0;
};

}  // namespace blink::scheduler

#endif
