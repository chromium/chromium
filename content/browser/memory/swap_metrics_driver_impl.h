// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_SWAP_METRICS_DRIVER_IMPL_H_
#define CONTENT_BROWSER_MEMORY_SWAP_METRICS_DRIVER_IMPL_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/swap_metrics_driver.h"

namespace content {

// SwapMetricsDriverImpl is the platform independent portion of the
// SwapMetricsDriver implementation.
class CONTENT_EXPORT SwapMetricsDriverImpl : public SwapMetricsDriver {
 public:
  ~SwapMetricsDriverImpl() override;

  // SwapMetricsDriver
  SwapMetricsDriver::SwapMetricsUpdateResult InitializeMetrics() override;
  bool IsRunning() override;
  SwapMetricsDriver::SwapMetricsUpdateResult Start() override;
  void Stop() override;
  SwapMetricsDriver::SwapMetricsUpdateResult UpdateMetrics() override;

 protected:
  SwapMetricsDriverImpl(std::unique_ptr<Delegate> delegate,
                        const base::TimeDelta update_interval);

  // Periodically called to update swap metrics.
  void PeriodicUpdateMetrics();

  // Common swap metrics update method for both periodic and manual updates.
  SwapMetricsDriver::SwapMetricsUpdateResult UpdateMetricsImpl();

  // Platform-dependent parts of UpdateMetricsImpl(). |interval| is the elapsed
  // time since the last UpdateMetricsImpl() call. |interval| will be zero when
  // this function is called for the first time.
  virtual SwapMetricsDriver::SwapMetricsUpdateResult UpdateMetricsInternal(
      base::TimeDelta interval) = 0;

  // The Delegate observing the metrics updates.
  std::unique_ptr<Delegate> delegate_;

 private:
  FRIEND_TEST_ALL_PREFIXES(TestSwapMetricsDriver, ExpectedMetricCounts);
  FRIEND_TEST_ALL_PREFIXES(TestSwapMetricsDriver, UpdateMetricsFail);

  // The interval between metrics updates.
  base::TimeDelta update_interval_;

  // A periodic timer to update swap metrics.
  base::RepeatingTimer timer_;

  // Holds the last TimeTicks when swap metrics are updated.
  base::TimeTicks last_ticks_;

  // True if and only if InitalizeMetrics() was called, and used to enforce
  // InitializeMetrics() is called before UpdateMetrics(). This helps
  // code readability.
  bool is_initialized_;

  // Updating metrics is not thread safe, and this checks that
  // UpdateMetricsImpl() is always called on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SwapMetricsDriverImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_SWAP_METRICS_DRIVER_IMPL_H_
