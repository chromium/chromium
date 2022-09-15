// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_SWAP_METRICS_DRIVER_IMPL_LINUX_H_
#define CONTENT_BROWSER_MEMORY_SWAP_METRICS_DRIVER_IMPL_LINUX_H_

#include "base/time/time.h"
#include "content/browser/memory/swap_metrics_driver_impl.h"

#include <memory>

namespace content {

class SwapMetricsDriverImplLinux : public SwapMetricsDriverImpl {
 public:
  SwapMetricsDriverImplLinux(std::unique_ptr<Delegate> delegate,
                             const base::TimeDelta update_interval);
  ~SwapMetricsDriverImplLinux() override;

 protected:
  SwapMetricsDriver::SwapMetricsUpdateResult UpdateMetricsInternal(
      base::TimeDelta interval) override;

 private:
  uint64_t last_pswpin_ = 0;
  uint64_t last_pswpout_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_SWAP_METRICS_DRIVER_IMPL_LINUX_H_
