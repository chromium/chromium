// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/swap_metrics_driver_impl_linux.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/process/process_metrics.h"
#include "base/time/time.h"
#include "content/public/browser/swap_metrics_driver.h"

namespace content {

namespace {

bool HasSwap() {
  base::SystemMemoryInfoKB memory_info;
  if (!base::GetSystemMemoryInfo(&memory_info))
    return false;
  return memory_info.swap_total > 0;
}

}  // namespace

// static
std::unique_ptr<SwapMetricsDriver> SwapMetricsDriver::Create(
    std::unique_ptr<Delegate> delegate,
    const base::TimeDelta update_interval) {
  return HasSwap() ? base::WrapUnique<SwapMetricsDriver>(
                         new SwapMetricsDriverImplLinux(std::move(delegate),
                                                        update_interval))
                   : std::unique_ptr<SwapMetricsDriver>();
}

SwapMetricsDriverImplLinux::SwapMetricsDriverImplLinux(
    std::unique_ptr<Delegate> delegate,
    const base::TimeDelta update_interval)
    : SwapMetricsDriverImpl(std::move(delegate), update_interval) {}

SwapMetricsDriverImplLinux::~SwapMetricsDriverImplLinux() = default;

SwapMetricsDriver::SwapMetricsUpdateResult
SwapMetricsDriverImplLinux::UpdateMetricsInternal(base::TimeDelta interval) {
  base::VmStatInfo vmstat;
  if (!base::GetVmStatInfo(&vmstat)) {
    return SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateFailed;
  }

  uint64_t in_counts = vmstat.pswpin - last_pswpin_;
  uint64_t out_counts = vmstat.pswpout - last_pswpout_;
  last_pswpin_ = vmstat.pswpin;
  last_pswpout_ = vmstat.pswpout;

  if (interval.is_zero())
    return SwapMetricsDriver::SwapMetricsUpdateResult::
        kSwapMetricsUpdateSuccess;

  delegate_->OnSwapInCount(in_counts, interval);
  delegate_->OnSwapOutCount(out_counts, interval);

  return SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateSuccess;
}

}  // namespace content
