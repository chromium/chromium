// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SCHEDULER_NET_TASK_PRIORITY_H_
#define NET_BASE_SCHEDULER_NET_TASK_PRIORITY_H_

#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"

namespace net {

// Creates and returns the priority settings for the network thread's
// `SequenceManager`.
NET_EXPORT base::sequence_manager::SequenceManager::PrioritySettings
CreateNetTaskPrioritySettings();

namespace internal {

// Defines the set of task priorities for the //net stack. These priorities
// are used by the `SequenceManager` to schedule tasks.
// This reflects `net::RequestPriority`, but in a reverse order to meet a
// requirement of `base::sequence_manager::TaskQueue::QueuePriority`.
enum class NetTaskPriority : base::sequence_manager::TaskQueue::QueuePriority {
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

static_assert(DEFAULT_PRIORITY == LOWEST);
static_assert(static_cast<size_t>(NetTaskPriority::kPriorityCount) ==
              NUM_PRIORITIES);
static_assert(static_cast<size_t>(NetTaskPriority::kPriorityCount) ==
              kMaxValue + 1);

}  // namespace internal
}  // namespace net

#endif  // NET_BASE_SCHEDULER_NET_TASK_PRIORITY_H_
