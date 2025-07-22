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
class SchedulerTaskContext;
class SoftNavigationContext;
class TaskAttributionTaskState;
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

  // Creates a new `TaskScope` to propagate `task_state` to descendant tasks and
  // continuations.
  virtual TaskScope CreateTaskScope(TaskAttributionInfo* task_state,
                                    TaskScopeType type) = 0;

  // Create a new `TaskScope` to propagate the given `SoftNavigationContext`,
  // initiating propagation for the context.
  virtual TaskScope CreateTaskScope(SoftNavigationContext*) = 0;

  // Creates a new `TaskScope` with web scheduling context. `task_state` will be
  // propagated to descendant tasks and continuations; `continuation_context`
  // will only be propagated to continuations.
  virtual TaskScope CreateTaskScope(
      TaskAttributionInfo* task_state,
      TaskScopeType type,
      SchedulerTaskContext* continuation_context) = 0;

  // Conditionally create a `TaskScope` for a generic v8 callback. A `TaskScope`
  // is always created if `task_state` is non-null, and one is additionally
  // created if there isn't an active `TaskScope`.
  virtual std::optional<TaskScope> MaybeCreateTaskScopeForCallback(
      TaskAttributionInfo* task_state) = 0;

  // Get the `TaskAttributionInfo` for the currently running task.
  virtual TaskAttributionInfo* CurrentTaskState() const = 0;

  // Registers an observer to be notified when a `TaskScope` has been created.
  // Multiple `Observer`s can be registered, but only the innermost one will
  // receive callbacks.
  virtual ObserverScope RegisterObserver(Observer* observer) = 0;

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
  virtual void OnObserverScopeDestroyed(const ObserverScope&) = 0;
};

}  // namespace blink::scheduler

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_TRACKER_H_
