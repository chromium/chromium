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

  // A class maintaining the scope of the current task. Keeping it alive ensures
  // that the current task is counted as a continuous one.
  class TaskScope {
   public:
    virtual ~TaskScope() = default;
    TaskScope(const TaskScope&) = delete;
    TaskScope& operator=(const TaskScope&) = delete;

   protected:
    TaskScope() = default;
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
  virtual std::unique_ptr<TaskScope> CreateTaskScope(
      ScriptState*,
      TaskAttributionInfo* parent_task,
      TaskScopeType type) = 0;
  // Create a new task scope with web scheduling context.
  virtual std::unique_ptr<TaskScope> CreateTaskScope(
      ScriptState*,
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
  virtual void OnObserverScopeDestroyed(const ObserverScope&) = 0;
};

}  // namespace blink::scheduler

#endif
