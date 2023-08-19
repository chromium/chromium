// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/swap_metrics_driver_impl_mac.h"

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <memory>

#include "base/apple/mach_logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"

namespace content {

// static
std::unique_ptr<SwapMetricsDriver> SwapMetricsDriver::Create(
    std::unique_ptr<Delegate> delegate,
    const base::TimeDelta update_interval) {
  return base::WrapUnique<SwapMetricsDriver>(
      new SwapMetricsDriverImplMac(std::move(delegate), update_interval));
}

SwapMetricsDriverImplMac::SwapMetricsDriverImplMac(
    std::unique_ptr<Delegate> delegate,
    const base::TimeDelta update_interval)
    : SwapMetricsDriverImpl(std::move(delegate), update_interval),
      host_(mach_host_self()) {}

SwapMetricsDriverImplMac::~SwapMetricsDriverImplMac() = default;

SwapMetricsDriver::SwapMetricsUpdateResult
SwapMetricsDriverImplMac::UpdateMetricsInternal(base::TimeDelta interval) {
  vm_statistics64_data_t statistics;
  mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
  kern_return_t result =
      host_statistics64(host_.get(), HOST_VM_INFO64,
                        reinterpret_cast<host_info64_t>(&statistics), &count);
  if (result != KERN_SUCCESS) {
    MACH_DLOG(WARNING, result) << "host_statistics64";
    return SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateFailed;
  }
  DCHECK_EQ(HOST_VM_INFO64_COUNT, count);

  uint64_t swapins = statistics.swapins - last_swapins_;
  uint64_t swapouts = statistics.swapouts - last_swapouts_;
  uint64_t decompressions = statistics.decompressions - last_decompressions_;
  uint64_t compressions = statistics.compressions - last_compressions_;
  last_swapins_ = statistics.swapins;
  last_swapouts_ = statistics.swapouts;
  last_decompressions_ = statistics.decompressions;
  last_compressions_ = statistics.compressions;

  if (interval.is_zero())
    return SwapMetricsDriver::SwapMetricsUpdateResult::
        kSwapMetricsUpdateSuccess;

  delegate_->OnSwapInCount(swapins, interval);
  delegate_->OnSwapOutCount(swapouts, interval);
  delegate_->OnDecompressedPageCount(decompressions, interval);
  delegate_->OnCompressedPageCount(compressions, interval);

  return SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateSuccess;
}

}  // namespace content
