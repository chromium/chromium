// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"

#include <cstdint>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"

#include "base/record_replay.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

BudgetPool::BudgetPool(const char* name) : name_(name), is_enabled_(true) {
  // https://linear.app/replay/issue/RUN-966
  // https://linear.app/replay/issue/RUN-1045
  recordreplay::RegisterPointer("BudgetPool", this);
}

BudgetPool::~BudgetPool() {
  recordreplay::UnregisterPointer(this);

  for (auto* throttler : associated_throttlers_) {
    throttler->RemoveBudgetPool(this);
  }
}

const char* BudgetPool::Name() const {
  return name_;
}

void BudgetPool::AddThrottler(base::TimeTicks now,
                              TaskQueueThrottler* throttler) {
  throttler->AddBudgetPool(this);
  associated_throttlers_.insert(throttler);
  REPLAY_ASSERT("[TT-1465-1466] BudgetPool::AddThrottler %d",
                recordreplay::PointerId(throttler));

  if (!is_enabled_)
    return;

  throttler->UpdateQueueState(now);
}

void BudgetPool::UnregisterThrottler(TaskQueueThrottler* throttler) {
  associated_throttlers_.erase(throttler);
  REPLAY_ASSERT("[TT-1465-1466] BudgetPool::UnregisterThrottler %d",
                recordreplay::PointerId(throttler));
}

void BudgetPool::RemoveThrottler(base::TimeTicks now,
                                 TaskQueueThrottler* throttler) {
  throttler->RemoveBudgetPool(this);
  associated_throttlers_.erase(throttler);
  REPLAY_ASSERT("[TT-1465-1466] BudgetPool::RemoveThrottler %d",
                recordreplay::PointerId(throttler));

  if (!is_enabled_)
    return;

  throttler->UpdateQueueState(now);
}

void BudgetPool::EnableThrottling(base::LazyNow* lazy_now) {
  if (is_enabled_)
    return;
  is_enabled_ = true;

  TRACE_EVENT0("renderer.scheduler", "BudgetPool_EnableThrottling");

  UpdateStateForAllThrottlers(lazy_now->Now());
}

void BudgetPool::DisableThrottling(base::LazyNow* lazy_now) {
  if (!is_enabled_)
    return;
  is_enabled_ = false;

  TRACE_EVENT0("renderer.scheduler", "BudgetPool_DisableThrottling");

  UpdateStateForAllThrottlers(lazy_now->Now());

  // TODO(altimin): We need to disable TimeBudgetQueues here or they will
  // regenerate extra time budget when they are disabled.
}

bool BudgetPool::IsThrottlingEnabled() const {
  return is_enabled_;
}

void BudgetPool::Close() {
  DCHECK_EQ(0u, associated_throttlers_.size());
}

void BudgetPool::UpdateStateForAllThrottlers(base::TimeTicks now) {
  for (TaskQueueThrottler* throttler : associated_throttlers_) {
    REPLAY_ASSERT("[TT-1465-1466] BudgetPool::UpdateStateForAllThrottlers %d %d",
                  recordreplay::PointerId(this),
                  recordreplay::PointerId(throttler));
    throttler->UpdateQueueState(now);
  }
}

}  // namespace scheduler
}  // namespace blink
