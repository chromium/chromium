// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_TRACKER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_id.h"

namespace blink {
class ScriptState;
}  // namespace blink

namespace blink::scheduler {

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

  virtual ~TaskAttributionTracker() = default;

  // Create a new task scope.
  virtual std::unique_ptr<TaskScope> CreateTaskScope(
      ScriptState*,
      absl::optional<TaskId> parent_task_id) = 0;

  // Get the ID of the currently running task.
  virtual absl::optional<TaskId> RunningTaskId(ScriptState*) const = 0;
  // Check for ancestry of the currently running task against an input
  // |parentId|.
  virtual AncestorStatus IsAncestor(ScriptState*, TaskId parentId) = 0;
};

}  // namespace blink::scheduler

#endif
