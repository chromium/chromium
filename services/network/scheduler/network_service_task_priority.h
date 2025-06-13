// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_TASK_PRIORITY_H_
#define SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_TASK_PRIORITY_H_

#include "base/component_export.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"

namespace network::internal {

// Defines the set of task priorities for the Network Service. These priorities
// are used by the `SequenceManager` to schedule tasks.
enum class NetworkServiceTaskPriority : base::sequence_manager::TaskQueue::
    QueuePriority {
      // Priorities are in descending order.
      kHighPriority = 0,
      kNormalPriority = 1,
      kDefaultPriority = kNormalPriority,
      // Must be the last entry.
      kPriorityCount = 2,
    };

// Creates and returns the priority settings for the Network Service's
// `SequenceManager`.
COMPONENT_EXPORT(NETWORK_SERVICE)
base::sequence_manager::SequenceManager::PrioritySettings
CreateNetworkServiceTaskPrioritySettings();

}  // namespace network::internal
#endif  // SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_TASK_PRIORITY_H_
