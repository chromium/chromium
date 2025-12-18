// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_TASK_ATTRIBUTION_TRACKER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_TASK_ATTRIBUTION_TRACKER_IMPL_H_

#include <optional>

#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
class SoftNavigationContext;
class WebSchedulingTaskState;
}  // namespace blink

namespace v8 {
class Isolate;
}  // namespace v8

namespace blink::scheduler {
class TaskAttributionInfo;

// This class is used to keep track of tasks posted on the main thread and their
// ancestry. It assigns an incerementing ID per task, and gets notified when a
// task is posted, started or ended, and using that, it keeps track of which
// task is the parent of the current task, and stores that info for later. It
// then enables callers to determine if a certain task ID is an ancestor of the
// current task.
class CORE_EXPORT TaskAttributionTrackerImpl
    : public TaskAttributionTracker,
      public blink::trace_event::TraceSessionObserver {
 public:
  static std::unique_ptr<TaskAttributionTracker> Create(v8::Isolate*);

  ~TaskAttributionTrackerImpl() override;

  // TaskAttributionTracker overrides:
  std::optional<TaskScope> SetCurrentTaskStateIfTopLevel(
      TaskAttributionInfo* task_state,
      TaskScopeType type) override;
  TaskScope SetCurrentTaskState(WebSchedulingTaskState* task_state,
                                TaskScopeType type) override;
  TaskScope SetTaskStateVariable(SoftNavigationContext*) override;
  TaskAttributionInfo* CurrentTaskState() const override;
  std::optional<TaskAttributionId> AsyncSameDocumentNavigationStarted()
      override;
  TaskAttributionInfo* CommitSameDocumentNavigation(TaskAttributionId) override;
  void ResetSameDocumentNavigationTasks() override;
  void BeginMicrotaskTrace() override;
  void EndMicrotaskTrace() override;

  // trace_event::TraceSessionObserver implementation.
  void OnStart(const perfetto::DataSourceBase::StartArgs&) override;
  void OnStop(const perfetto::DataSourceBase::StopArgs&) override;

 private:
  explicit TaskAttributionTrackerImpl(v8::Isolate*);

  TaskScope SetCurrentTaskStateImpl(TaskAttributionTaskState* task_state,
                                    TaskScopeType type);
  void OnTaskScopeDestroyed(const TaskScope&) override;

  TaskAttributionId next_task_id_{1};

  // A queue of TaskAttributionInfo objects representing tasks that initiated a
  // same-document navigation that was sent to the browser side. They are kept
  // here to ensure the relevant object remains alive (and hence properly
  // tracked through task attribution).
  Deque<Persistent<TaskAttributionInfo>> same_document_navigation_tasks_;

  // The lifetime of this class is tied to the `isolate_`.
  v8::Isolate* isolate_;

  base::WeakPtrFactory<TaskAttributionTrackerImpl> weak_factory_{this};
};

}  // namespace blink::scheduler

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_TASK_ATTRIBUTION_TRACKER_IMPL_H_
