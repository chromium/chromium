// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_TRACKER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
class AbortSignal;
class DOMTaskSignal;
class ExecutionContext;
class ScriptState;
}  // namespace blink

namespace blink::scheduler {

class TaskAttributionInfo;

// This public interface enables platform/ and core/ callers to create a task
// scope on the one hand, and check on the ID of the currently running task as
// well as its ancestry on the other.
class PLATFORM_EXPORT TaskAttributionTracker {
 public:
  // Return value for encestry related queries.
  enum class AncestorStatus {
    kAncestor,
    kNotAncestor,
    kUnknown,
  };

  enum class TaskScopeType {
    kCallback,
    kScheduledAction,
    kScriptExecution,
    kPostMessage,
    kPopState,
    kSchedulerPostTask,
    kRequestIdleCallback,
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
    virtual void OnCreateTaskScope(TaskAttributionInfo&, ScriptState*) = 0;
    virtual void OnTaskDisposal(const TaskAttributionInfo&) = 0;
    virtual ExecutionContext* GetExecutionContext() = 0;
  };

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

  // Get the ID of the currently running task.
  virtual TaskAttributionInfo* RunningTask(ScriptState*) const = 0;

  // Check for ancestry of the currently running task against an input
  // |parentId|.
  virtual AncestorStatus IsAncestor(ScriptState*,
                                    TaskAttributionId parentId) = 0;

  // Check for ancestry in a set of potential parent tasks. ScriptState is
  // required as well as the |set|. |task| is the task on which ancestry is
  // tested. The return value is an optional task ID, representing the ancestor
  // from the set, in case it was found, and nullopt otherwise.
  virtual absl::optional<TaskAttributionId> GetAncestorFromSet(
      ScriptState*,
      const WTF::HashSet<scheduler::TaskAttributionIdType>& set,
      const TaskAttributionInfo& task) = 0;

  // Register an observer if one isn't yet registered, to be notified when a
  // task is started. Return false if no observer was registered.
  virtual bool RegisterObserverIfNeeded(Observer* observer) = 0;
  // Unregister the observer.
  virtual void UnregisterObserver(Observer* observer) = 0;

  // Setter and getter for a pointer to a pending same-document navigation task,
  // to ensure the task's lifetime.
  virtual void AddSameDocumentNavigationTask(TaskAttributionInfo* task) = 0;
  virtual void ResetSameDocumentNavigationTasks() = 0;
  virtual TaskAttributionInfo* CommitSameDocumentNavigation(
      TaskAttributionId) = 0;
  virtual Observer* GetObserverForTaskDisposal(TaskAttributionId) = 0;
  virtual void SetObserverForTaskDisposal(TaskAttributionId, Observer*) = 0;
};

}  // namespace blink::scheduler

#endif
