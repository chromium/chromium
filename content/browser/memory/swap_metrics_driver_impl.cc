// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/swap_metrics_driver_impl.h"

#include <memory>

#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace content {

SwapMetricsDriverImpl::Delegate::Delegate() = default;

SwapMetricsDriverImpl::Delegate::~Delegate() = default;

SwapMetricsDriverImpl::SwapMetricsDriverImpl(
    std::unique_ptr<Delegate> delegate,
    const base::TimeDelta update_interval)
    : delegate_(std::move(delegate)),
      update_interval_(update_interval),
      is_initialized_(false) {
  DCHECK(delegate_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SwapMetricsDriverImpl::~SwapMetricsDriverImpl() = default;

SwapMetricsDriver::SwapMetricsUpdateResult
SwapMetricsDriverImpl::InitializeMetrics() {
  last_ticks_ = base::TimeTicks();
  auto result = UpdateMetricsImpl();
  if (result ==
      SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateSuccess)
    is_initialized_ = true;
  return result;
}

bool SwapMetricsDriverImpl::IsRunning() {
  return timer_.IsRunning();
}

void SwapMetricsDriverImpl::PeriodicUpdateMetrics() {
  SwapMetricsDriver::SwapMetricsUpdateResult result = UpdateMetricsImpl();
  if (result !=
      SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateSuccess)
    Stop();
}

SwapMetricsDriver::SwapMetricsUpdateResult SwapMetricsDriverImpl::Start() {
  DCHECK(update_interval_.InSeconds() > 0);

  SwapMetricsDriver::SwapMetricsUpdateResult result = InitializeMetrics();
  if (result !=
      SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateSuccess)
    return result;

  timer_.Start(FROM_HERE, update_interval_, this,
               &SwapMetricsDriverImpl::PeriodicUpdateMetrics);
  return SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateSuccess;
}

void SwapMetricsDriverImpl::Stop() {
  timer_.Stop();
}

SwapMetricsDriver::SwapMetricsUpdateResult
SwapMetricsDriverImpl::UpdateMetrics() {
  // Enforce initialization before updates.
  DCHECK(is_initialized_);
  return UpdateMetricsImpl();
}

SwapMetricsDriver::SwapMetricsUpdateResult
SwapMetricsDriverImpl::UpdateMetricsImpl() {
  // This covers all cases where metrics get updated, and is the critical secion
  // in the driver.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta interval =
      last_ticks_.is_null() ? base::TimeDelta() : now - last_ticks_;

  SwapMetricsDriver::SwapMetricsUpdateResult result =
      UpdateMetricsInternal(interval);
  if (result !=
      SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateSuccess) {
    delegate_->OnUpdateMetricsFailed();
    return result;
  }

  last_ticks_ = now;
  return SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateSuccess;
}

}  // namespace content
