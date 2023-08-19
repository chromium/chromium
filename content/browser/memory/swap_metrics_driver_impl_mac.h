// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_SWAP_METRICS_DRIVER_IMPL_MAC_H_
#define CONTENT_BROWSER_MEMORY_SWAP_METRICS_DRIVER_IMPL_MAC_H_

#include "content/browser/memory/swap_metrics_driver_impl.h"

#include <memory>

#include "base/apple/scoped_mach_port.h"
#include "base/time/time.h"

namespace content {

class SwapMetricsDriverImplMac : public SwapMetricsDriverImpl {
 public:
  SwapMetricsDriverImplMac(std::unique_ptr<Delegate> delegate,
                           const base::TimeDelta update_interval);
  ~SwapMetricsDriverImplMac() override;

 protected:
  SwapMetricsDriver::SwapMetricsUpdateResult UpdateMetricsInternal(
      base::TimeDelta interval) override;

 private:
  base::apple::ScopedMachSendRight host_;

  uint64_t last_swapins_ = 0;
  uint64_t last_swapouts_ = 0;
  uint64_t last_decompressions_ = 0;
  uint64_t last_compressions_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_SWAP_METRICS_DRIVER_IMPL_MAC_H_
