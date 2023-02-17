// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_PRIORITY_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_PRIORITY_H_

#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "content/common/content_export.h"

namespace content::internal {

// clang-format off
enum class BrowserTaskPriority
    : base::sequence_manager::TaskQueue::QueuePriority {
  // Priorities are in descending order.
  kControlPriority = 0,
  kHighestPriority = 1,
  kHighPriority = 2,
  kNormalPriority = 3,
  kDefaultPriority = kNormalPriority,
  kLowPriority = 4,
  kBestEffortPriority = 5,

  // Must be the last entry.
  kPriorityCount = 6,
};
// clang-format on

base::sequence_manager::SequenceManager::PrioritySettings CONTENT_EXPORT
CreateBrowserTaskPrioritySettings();

}  // namespace content::internal

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_PRIORITY_H_
