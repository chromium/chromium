// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_SERVICE_TASK_PRIORITY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_SERVICE_TASK_PRIORITY_H_

#include "base/component_export.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "net/base/request_priority.h"

namespace network {

// Creates and returns the priority settings for the Network Service's
// `SequenceManager`.
COMPONENT_EXPORT(NETWORK_CPP)
base::sequence_manager::SequenceManager::PrioritySettings
CreateNetworkServiceTaskPrioritySettings();

namespace internal {

// Defines the set of task priorities for the Network Service. These priorities
// are used by the `SequenceManager` to schedule tasks.
// This reflects `net::RequestPriority`, but in a reverse order to meet a
// requirement of `base::sequence_manager::TaskQueue::QueuePriority`.
enum class NetworkServiceTaskPriority : base::sequence_manager::TaskQueue::
    QueuePriority {
      // Priorities are in descending order.
      kHighestPriority = 0,
      kMediumPriority = 1,
      kLowPriority = 2,
      kLowestPriority = 3,
      kIdlePriority = 4,
      kThrottledPriority = 5,
      kDefaultPriority = kLowestPriority,
      // Must be the last entry.
      kPriorityCount = 6,
    };

static_assert(net::RequestPriority::DEFAULT_PRIORITY ==
              net::RequestPriority::LOWEST);
static_assert(static_cast<size_t>(NetworkServiceTaskPriority::kPriorityCount) ==
              net::NUM_PRIORITIES);
static_assert(static_cast<size_t>(NetworkServiceTaskPriority::kPriorityCount) ==
              net::RequestPriority::kMaxValue + 1);

}  // namespace internal
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_SERVICE_TASK_PRIORITY_H_
