// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_TASK_PRIORITY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_TASK_PRIORITY_H_

#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink::scheduler {

enum class TaskPriority : base::sequence_manager::TaskQueue::QueuePriority {
  // Priorities are in descending order.
  kControlPriority = 0,
  kHighestPriority = 1,
  kVeryHighPriority = 2,
  kHighPriorityContinuation = 3,
  kHighPriority = 4,
  kNormalPriorityContinuation = 5,
  kNormalPriority = 6,
  kDefaultPriority = kNormalPriority,
  kLowPriorityContinuation = 7,
  kLowPriority = 8,
  kBestEffortPriority = 9,

  // Must be the last entry.
  kPriorityCount = 10,
};

base::sequence_manager::SequenceManager::PrioritySettings PLATFORM_EXPORT
CreatePrioritySettings();

const char* TaskPriorityToString(TaskPriority);

}  // namespace blink::scheduler

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_TASK_PRIORITY_H_
