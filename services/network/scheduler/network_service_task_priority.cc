// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/scheduler/network_service_task_priority.h"

#include "base/notreached.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"

namespace network::internal {

namespace {

using ProtoPriority = perfetto::protos::pbzero::SequenceManagerTask::Priority;

ProtoPriority ToProtoPriority(NetworkServiceTaskPriority priority) {
  switch (priority) {
    case NetworkServiceTaskPriority::kHighPriority:
      return ProtoPriority::HIGHEST_PRIORITY;
    case NetworkServiceTaskPriority::kNormalPriority:
      return ProtoPriority::NORMAL_PRIORITY;
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
  using base::sequence_manager::TaskQueue;
  base::sequence_manager::SequenceManager::PrioritySettings settings(
      NetworkServiceTaskPriority::kPriorityCount,
      NetworkServiceTaskPriority::kNormalPriority);
  settings.SetProtoPriorityConverter(&TaskPriorityToProto);
  return settings;
}

}  // namespace network::internal
