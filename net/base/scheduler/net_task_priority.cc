// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/scheduler/net_task_priority.h"

#include "base/notreached.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"

namespace net {

using internal::NetTaskPriority;

namespace {

using ProtoPriority = perfetto::protos::pbzero::SequenceManagerTask::Priority;

ProtoPriority ToProtoPriority(NetTaskPriority priority) {
  switch (priority) {
    case NetTaskPriority::kHighestPriority:
      return ProtoPriority::HIGHEST_PRIORITY;
    case NetTaskPriority::kMediumPriority:
      return ProtoPriority::MEDIUM_PRIORITY;
    case NetTaskPriority::kLowPriority:
      return ProtoPriority::LOW_PRIORITY;
    case NetTaskPriority::kLowestPriority:
      return ProtoPriority::LOWEST_PRIORITY;
    case NetTaskPriority::kIdlePriority:
      return ProtoPriority::IDLE_PRIORITY;
    case NetTaskPriority::kThrottledPriority:
      return ProtoPriority::THROTTLED_PRIORITY;
    case NetTaskPriority::kPriorityCount:
      NOTREACHED();
  }
  NOTREACHED();
}

ProtoPriority TaskPriorityToProto(
    base::sequence_manager::TaskQueue::QueuePriority priority) {
  CHECK_LT(static_cast<size_t>(priority),
           static_cast<size_t>(NetTaskPriority::kPriorityCount));
  return ToProtoPriority(static_cast<NetTaskPriority>(priority));
}

base::ThreadType ToThreadType(NetTaskPriority priority) {
  switch (priority) {
    case NetTaskPriority::kHighestPriority:
      return base::ThreadType::kPresentation;
    case NetTaskPriority::kMediumPriority:
      return base::ThreadType::kDefault;
    case NetTaskPriority::kLowPriority:
      return base::ThreadType::kUtility;
    case NetTaskPriority::kLowestPriority:
    case NetTaskPriority::kIdlePriority:
    case NetTaskPriority::kThrottledPriority:
      return base::ThreadType::kBackground;
    case NetTaskPriority::kPriorityCount:
      NOTREACHED();
  }
}

base::ThreadType TaskPriorityToThreadType(
    base::sequence_manager::TaskQueue::QueuePriority priority) {
  CHECK_LT(static_cast<size_t>(priority),
           static_cast<size_t>(NetTaskPriority::kPriorityCount));
  return ToThreadType(static_cast<NetTaskPriority>(priority));
}

}  // namespace

base::sequence_manager::SequenceManager::PrioritySettings
CreateNetTaskPrioritySettings() {
  base::sequence_manager::SequenceManager::PrioritySettings settings(
      NetTaskPriority::kPriorityCount, NetTaskPriority::kDefaultPriority);
  settings.SetProtoPriorityConverter(&TaskPriorityToProto);
  settings.SetThreadTypeMapping(&TaskPriorityToThreadType);
  return settings;
}

}  // namespace net
