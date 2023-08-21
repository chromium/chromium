// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_ATTRIBUTION_TRACKER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_ATTRIBUTION_TRACKER_IMPL_H_

#include "base/containers/contains.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

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

  absl::optional<TaskAttributionId> RunningTaskAttributionId(
      ScriptState*) const override;

  AncestorStatus IsAncestor(ScriptState*, TaskAttributionId parent_id) override;
  AncestorStatus HasAncestorInSet(
      ScriptState*,
      const WTF::HashSet<scheduler::TaskAttributionIdType>&) override;

  std::unique_ptr<TaskScope> CreateTaskScope(
      ScriptState* script_state,
      absl::optional<TaskAttributionId> parent_task_id,
      TaskScopeType type) override;

  std::unique_ptr<TaskScope> CreateTaskScope(
      ScriptState* script_state,
      absl::optional<TaskAttributionId> parent_task_id,
      TaskScopeType type,
      AbortSignal* abort_source,
      DOMTaskSignal* priority_source) override;

  // The vector size limits the amount of tasks we keep track of. Setting this
  // value too small can result in calls to `IsAncestor` returning an `Unknown`
  // ancestor status. If this happens a lot in realistic scenarios, we'd need to
  // increase this value (at the expense of memory dedicated to task tracking).
  static constexpr size_t kVectorSize = 1024;

  void SetRunningTaskAttributionId(absl::optional<TaskAttributionId> id) {
    running_task_id_ = id;
  }

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
                  absl::optional<TaskAttributionId> running_task_id,
                  ScriptWrappableTaskState* continuation_task_state,
                  TaskScopeType,
                  absl::optional<TaskAttributionId> parent_task_id);
    ~TaskScopeImpl() override;
    TaskScopeImpl(const TaskScopeImpl&) = delete;
    TaskScopeImpl& operator=(const TaskScopeImpl&) = delete;

    TaskAttributionId GetTaskId() const { return scope_task_id_; }
    absl::optional<TaskAttributionId> RunningTaskIdToBeRestored() const {
      return running_task_id_to_be_restored_;
    }

    ScriptWrappableTaskState* ContinuationTaskStateToBeRestored() const {
      return continuation_state_to_be_restored_;
    }

    ScriptState* GetScriptState() const { return script_state_; }

   private:
    TaskAttributionTrackerImpl* task_tracker_;
    TaskAttributionId scope_task_id_;
    absl::optional<TaskAttributionId> running_task_id_to_be_restored_;
    Persistent<ScriptWrappableTaskState> continuation_state_to_be_restored_;
    Persistent<ScriptState> script_state_;
  };

  void TaskScopeCompleted(const TaskScopeImpl&);
  TaskAttributionIdPair& GetTaskAttributionIdPairFromTaskContainer(
      TaskAttributionId);
  void InsertTaskAttributionIdPair(
      TaskAttributionId task_id,
      absl::optional<TaskAttributionId> parent_task_id);

  TaskAttributionId next_task_id_;
  absl::optional<TaskAttributionId> running_task_id_;

  // The task container is a vector of optional TaskAttributionIdPairs where its
  // indexes are TaskAttributionId hashes, and its values are the TaskId of the
  // parent task for the TaskAttributionId that is resulted in the index. We're
  // using this vector as a circular array, where in order to find if task A is
  // an ancestor of task B, we look up the value at B's taskAttributionId hash
  // position, get its parent, and repeat that process until we either find A in
  // the ancestor chain, get no parent task (indicating that a task has no
  // parent, so wasn't initiated by another JS task), or reach a parent that
  // doesn't have the current ID its child though it should have, which
  // indicates that the parent was overwritten by a newer task, indicating that
  // we went "full circle".
  WTF::Vector<TaskAttributionIdPair> task_container_ =
      WTF::Vector<TaskAttributionIdPair>(kVectorSize);

  WTF::HashSet<WeakPersistent<TaskAttributionTracker::Observer>> observers_;
};

}  // namespace blink::scheduler

#endif
