// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"

#include <cstdint>

#include "base/optional.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool_controller.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

BudgetPool::BudgetPool(const char* name,
                       BudgetPoolController* budget_pool_controller)
    : name_(name),
      budget_pool_controller_(budget_pool_controller),
      is_enabled_(true) {}

BudgetPool::~BudgetPool() = default;

const char* BudgetPool::Name() const {
  return name_;
}

void BudgetPool::AddQueue(base::TimeTicks now, TaskQueue* queue) {
  budget_pool_controller_->AddQueueToBudgetPool(queue, this);
  associated_task_queues_.insert(queue);

  if (!is_enabled_)
    return;
  budget_pool_controller_->UpdateQueueSchedulingLifecycleState(now, queue);
}

void BudgetPool::UnregisterQueue(TaskQueue* queue) {
  DissociateQueue(queue);
}

void BudgetPool::RemoveQueue(base::TimeTicks now, TaskQueue* queue) {
  DissociateQueue(queue);

  if (!is_enabled_)
    return;

  budget_pool_controller_->UpdateQueueSchedulingLifecycleState(now, queue);
}

void BudgetPool::DissociateQueue(TaskQueue* queue) {
  budget_pool_controller_->RemoveQueueFromBudgetPool(queue, this);
  associated_task_queues_.erase(queue);
}

void BudgetPool::EnableThrottling(base::sequence_manager::LazyNow* lazy_now) {
  if (is_enabled_)
    return;
  is_enabled_ = true;

  TRACE_EVENT0("renderer.scheduler", "BudgetPool_EnableThrottling");

  BlockThrottledQueues(lazy_now->Now());
}

void BudgetPool::DisableThrottling(base::sequence_manager::LazyNow* lazy_now) {
  if (!is_enabled_)
    return;
  is_enabled_ = false;

  TRACE_EVENT0("renderer.scheduler", "BudgetPool_DisableThrottling");

  for (TaskQueue* queue : associated_task_queues_) {
    budget_pool_controller_->UpdateQueueSchedulingLifecycleState(
        lazy_now->Now(), queue);
  }

  // TODO(altimin): We need to disable TimeBudgetQueues here or they will
  // regenerate extra time budget when they are disabled.
}

bool BudgetPool::IsThrottlingEnabled() const {
  return is_enabled_;
}

void BudgetPool::Close() {
  DCHECK_EQ(0u, associated_task_queues_.size());

  budget_pool_controller_->UnregisterBudgetPool(this);
}

void BudgetPool::BlockThrottledQueues(base::TimeTicks now) {
  for (TaskQueue* queue : associated_task_queues_)
    budget_pool_controller_->UpdateQueueSchedulingLifecycleState(now, queue);
}

}  // namespace scheduler
}  // namespace blink
