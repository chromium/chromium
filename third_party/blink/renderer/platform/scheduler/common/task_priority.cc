// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"

#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"

namespace blink::scheduler {

namespace {

using ProtoPriority = perfetto::protos::pbzero::SequenceManagerTask::Priority;

ProtoPriority ToProtoPriority(TaskPriority priority) {
  switch (priority) {
    case TaskPriority::kControlPriority:
      return ProtoPriority::CONTROL_PRIORITY;
    case TaskPriority::kHighestPriority:
      return ProtoPriority::HIGHEST_PRIORITY;
    case TaskPriority::kVeryHighPriority:
      return ProtoPriority::VERY_HIGH_PRIORITY;
    case TaskPriority::kHighPriority:
      return ProtoPriority::HIGH_PRIORITY;
    case TaskPriority::kNormalPriority:
      return ProtoPriority::NORMAL_PRIORITY;
    case TaskPriority::kLowPriority:
      return ProtoPriority::LOW_PRIORITY;
    case TaskPriority::kBestEffortPriority:
      return ProtoPriority::BEST_EFFORT_PRIORITY;
    case TaskPriority::kPriorityCount:
      return ProtoPriority::UNKNOWN;
  }
}

ProtoPriority TaskPriorityToProto(
    base::sequence_manager::TaskQueue::QueuePriority priority) {
  DCHECK_LT(static_cast<size_t>(priority),
            static_cast<size_t>(TaskPriority::kPriorityCount));
  return ToProtoPriority(static_cast<TaskPriority>(priority));
}

}  // namespace

base::sequence_manager::SequenceManager::PrioritySettings
CreatePrioritySettings() {
  using base::sequence_manager::TaskQueue;
  base::sequence_manager::SequenceManager::PrioritySettings settings(
      TaskPriority::kPriorityCount, TaskPriority::kDefaultPriority);
#if BUILDFLAG(ENABLE_BASE_TRACING)
  settings.SetProtoPriorityConverter(&TaskPriorityToProto);
#endif
  return settings;
}

const char* TaskPriorityToString(TaskPriority priority) {
  switch (priority) {
    case TaskPriority::kControlPriority:
      return "control";
    case TaskPriority::kHighestPriority:
      return "highest";
    case TaskPriority::kVeryHighPriority:
      return "very_high";
    case TaskPriority::kHighPriority:
      return "high";
    case TaskPriority::kNormalPriority:
      return "normal";
    case TaskPriority::kLowPriority:
      return "low";
    case TaskPriority::kBestEffortPriority:
      return "best_effort";
    case TaskPriority::kPriorityCount:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink::scheduler
