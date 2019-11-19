// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_BUDGET_POOL_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_BUDGET_POOL_CONTROLLER_H_

#include "base/time/time.h"

namespace blink {
namespace scheduler {

class BudgetPool;

// Interface for BudgetPool to interact with TaskQueueThrottler.
class PLATFORM_EXPORT BudgetPoolController {
 public:
  virtual ~BudgetPoolController() = default;

  // To be used by BudgetPool only, use BudgetPool::{Add,Remove}Queue
  // methods instead.
  virtual void AddQueueToBudgetPool(base::sequence_manager::TaskQueue* queue,
                                    BudgetPool* budget_pool) = 0;
  virtual void RemoveQueueFromBudgetPool(
      base::sequence_manager::TaskQueue* queue,
      BudgetPool* budget_pool) = 0;

  // Deletes the budget pool.
  virtual void UnregisterBudgetPool(BudgetPool* budget_pool) = 0;

  // Ensure that an appropriate type of the fence is installed and schedule
  // a pump for this queue when needed.
  virtual void UpdateQueueSchedulingLifecycleState(
      base::TimeTicks now,
      base::sequence_manager::TaskQueue* queue) = 0;

  // Returns true if the |queue| is throttled (i.e. added to TaskQueueThrottler
  // and throttling is not disabled).
  virtual bool IsThrottled(base::sequence_manager::TaskQueue* queue) const = 0;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_BUDGET_POOL_CONTROLLER_H_"
