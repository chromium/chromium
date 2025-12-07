// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_service_task_priority.h"

#include "base/notreached.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"

namespace network {

using internal::NetworkServiceTaskPriority;

namespace {

using ProtoPriority = perfetto::protos::pbzero::SequenceManagerTask::Priority;

ProtoPriority ToProtoPriority(NetworkServiceTaskPriority priority) {
  switch (priority) {
    case NetworkServiceTaskPriority::kHighestPriority:
      return ProtoPriority::HIGHEST_PRIORITY;
    case NetworkServiceTaskPriority::kMediumPriority:
      return ProtoPriority::MEDIUM_PRIORITY;
    case NetworkServiceTaskPriority::kLowPriority:
      return ProtoPriority::LOW_PRIORITY;
    case NetworkServiceTaskPriority::kLowestPriority:
      return ProtoPriority::LOWEST_PRIORITY;
    case NetworkServiceTaskPriority::kIdlePriority:
      return ProtoPriority::IDLE_PRIORITY;
    case NetworkServiceTaskPriority::kThrottledPriority:
      return ProtoPriority::THROTTLED_PRIORITY;
    case NetworkServiceTaskPriority::kPriorityCount:
      NOTREACHED();
  }
  NOTREACHED();
}

ProtoPriority TaskPriorityToProto(
    base::sequence_manager::TaskQueue::QueuePriority priority) {
  CHECK_LT(static_cast<size_t>(priority),
           static_cast<size_t>(NetworkServiceTaskPriority::kPriorityCount));
  return ToProtoPriority(static_cast<NetworkServiceTaskPriority>(priority));
}

}  // namespace

base::sequence_manager::SequenceManager::PrioritySettings
CreateNetworkServiceTaskPrioritySettings() {
  base::sequence_manager::SequenceManager::PrioritySettings settings(
      NetworkServiceTaskPriority::kPriorityCount,
      NetworkServiceTaskPriority::kDefaultPriority);
  settings.SetProtoPriorityConverter(&TaskPriorityToProto);
  return settings;
}

}  // namespace network
