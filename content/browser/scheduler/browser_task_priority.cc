// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_priority.h"

#include "base/notreached.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"

namespace content::internal {

namespace {

using ProtoPriority = perfetto::protos::pbzero::SequenceManagerTask::Priority;

ProtoPriority ToProtoPriority(BrowserTaskPriority priority) {
  switch (priority) {
    case BrowserTaskPriority::kControlPriority:
      return ProtoPriority::CONTROL_PRIORITY;
    case BrowserTaskPriority::kHighestPriority:
      return ProtoPriority::HIGHEST_PRIORITY;
    case BrowserTaskPriority::kHighPriority:
      return ProtoPriority::HIGH_PRIORITY;
    case BrowserTaskPriority::kNormalPriority:
      return ProtoPriority::NORMAL_PRIORITY;
    case BrowserTaskPriority::kLowPriority:
      return ProtoPriority::LOW_PRIORITY;
    case BrowserTaskPriority::kBestEffortPriority:
      return ProtoPriority::BEST_EFFORT_PRIORITY;
    case BrowserTaskPriority::kPriorityCount:
      NOTREACHED_IN_MIGRATION();
      return ProtoPriority::UNKNOWN;
  }
}

ProtoPriority TaskPriorityToProto(
    base::sequence_manager::TaskQueue::QueuePriority priority) {
  DCHECK_LT(static_cast<size_t>(priority),
            static_cast<size_t>(BrowserTaskPriority::kPriorityCount));
  return ToProtoPriority(static_cast<BrowserTaskPriority>(priority));
}

}  // namespace

base::sequence_manager::SequenceManager::PrioritySettings
CreateBrowserTaskPrioritySettings() {
  using base::sequence_manager::TaskQueue;
  base::sequence_manager::SequenceManager::PrioritySettings settings(
      BrowserTaskPriority::kPriorityCount,
      BrowserTaskPriority::kDefaultPriority);
  settings.SetProtoPriorityConverter(&TaskPriorityToProto);
  return settings;
}

}  // namespace content::internal
