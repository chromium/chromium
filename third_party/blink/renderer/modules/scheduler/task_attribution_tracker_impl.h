// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_ATTRIBUTION_TRACKER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_ATTRIBUTION_TRACKER_IMPL_H_

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
class AbortSignal;
class DOMTaskSignal;
class ScriptWrappableTaskState;
}  // namespace blink

namespace blink::scheduler {

class TaskAttributionTrackerTest;

// This class is used to keep track of tasks posted on the main thread and their
// ancestry. It assigns an incerementing ID per task, and gets notified when a
// task is posted, started or ended, and using that, it keeps track of which
// task is the parent of the current task, and stores that info for later. It
// then enables callers to determine if a certain task ID is an ancestor of the
// current task.
class MODULES_EXPORT TaskAttributionTrackerImpl
    : public TaskAttributionTracker {
  friend class TaskAttributionTrackerTest;

 public:
  TaskAttributionTrackerImpl();

  TaskAttributionInfo* RunningTask(ScriptState*) const override;

  AncestorStatus IsAncestor(ScriptState*, TaskAttributionId parent_id) override;
  AncestorStatus HasAncestorInSet(
      ScriptState*,
      const WTF::HashSet<scheduler::TaskAttributionIdType>&) override;

  std::unique_ptr<TaskScope> CreateTaskScope(ScriptState* script_state,
                                             TaskAttributionInfo* parent_task,
                                             TaskScopeType type) override;

  std::unique_ptr<TaskScope> CreateTaskScope(
      ScriptState* script_state,
      TaskAttributionInfo* parent_task,
      TaskScopeType type,
      AbortSignal* abort_source,
      DOMTaskSignal* priority_source) override;

  void TaskScopeCompleted(ScriptState*, TaskAttributionId);

  void RegisterObserver(TaskAttributionTracker::Observer* observer) override {
    if (!base::Contains(observers_, observer)) {
      observers_.insert(observer);
    }
  }

  void UnregisterObserver(TaskAttributionTracker::Observer* observer) override {
    auto it = observers_.find(observer);
    // It's possible for the observer to not be registered if it already
    // unregistered itself in the past.
    if (it != observers_.end()) {
      observers_.erase(it);
    }
  }

  void AddSameDocumentNavigationTask(TaskAttributionInfo* task) override;
  void ResetSameDocumentNavigationTasks() override;
  TaskAttributionInfo* CommitSameDocumentNavigation(TaskAttributionId) override;

 protected:
  // Saves the given `ScriptWrappableTaskState` as the current continuation
  // preserved embedder data. Virtual for testing.
  virtual void SetCurrentTaskContinuationData(ScriptState*,
                                              ScriptWrappableTaskState*);

  // Gets the current `ScriptWrappableTaskState` from the current continuation
  // preserved embedder data. Virtual for testing.
  virtual ScriptWrappableTaskState* GetCurrentTaskContinuationData(
      ScriptState*) const;

 private:
  struct TaskAttributionIdPair {
    TaskAttributionIdPair() = default;
    TaskAttributionIdPair(absl::optional<TaskAttributionId> parent_id,
                          absl::optional<TaskAttributionId> current_id)
        : parent(parent_id), current(current_id) {}

    explicit operator bool() const { return parent.has_value(); }
    absl::optional<TaskAttributionId> parent;
    absl::optional<TaskAttributionId> current;
  };

  template <typename F>
  AncestorStatus IsAncestorInternal(ScriptState*, F callback);

  // The TaskScope class maintains information about a task. The task's lifetime
  // match those of TaskScope, and the task is considered terminated when
  // TaskScope is destructed. TaskScope takes in the Task's ID, ScriptState, the
  // ID of the running task (to restore as the running task once this task is
  // done), and a continuation task ID (to restore in V8 once the current task
  // is done).
  class TaskScopeImpl : public TaskScope {
   public:
    TaskScopeImpl(ScriptState*,
                  TaskAttributionTrackerImpl*,
                  TaskAttributionId scope_task_id,
                  TaskAttributionInfo* running_task,
                  ScriptWrappableTaskState* continuation_task_state,
                  TaskScopeType,
                  absl::optional<TaskAttributionId> parent_task_id);
    ~TaskScopeImpl() override;
    TaskScopeImpl(const TaskScopeImpl&) = delete;
    TaskScopeImpl& operator=(const TaskScopeImpl&) = delete;

    TaskAttributionId GetTaskId() const { return scope_task_id_; }
    TaskAttributionInfo* RunningTaskToBeRestored() const {
      return running_task_to_be_restored_.Get();
    }

    ScriptWrappableTaskState* ContinuationTaskStateToBeRestored() const {
      return continuation_state_to_be_restored_;
    }

    ScriptState* GetScriptState() const { return script_state_; }

   private:
    raw_ptr<TaskAttributionTrackerImpl, ExperimentalRenderer> task_tracker_;
    TaskAttributionId scope_task_id_;
    Persistent<TaskAttributionInfo> running_task_to_be_restored_;
    Persistent<ScriptWrappableTaskState> continuation_state_to_be_restored_;
    Persistent<ScriptState> script_state_;
  };

  void TaskScopeCompleted(const TaskScopeImpl&);

  TaskAttributionId next_task_id_;
  Persistent<TaskAttributionInfo> running_task_ = nullptr;

  WTF::HashSet<WeakPersistent<TaskAttributionTracker::Observer>> observers_;

  // A queue of TaskAttributionInfo objects representing tasks that initiated a
  // same-document navigation that was sent to the browser side. They are kept
  // here to ensure the relevant object remains alive (and hence properly
  // tracked through task attribution).
  WTF::Deque<Persistent<TaskAttributionInfo>> same_document_navigation_tasks_;
};

}  // namespace blink::scheduler

#endif
