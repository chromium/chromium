// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_HIGHEST_PMF_REPORTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_HIGHEST_PMF_REPORTER_H_

#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/controller/memory_usage_monitor.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace base {
class SingleThreadTaskRunner;
class TickClock;
}

namespace blink {

// Reports the highest private memory footprint in [X mins after the first page
// navigation, Y mins after the first page navigation].
class CONTROLLER_EXPORT HighestPmfReporter
    : public MemoryUsageMonitor::Observer {
  USING_FAST_MALLOC(HighestPmfReporter);

 public:
  // Returns the shared instance.
  static HighestPmfReporter& Instance();

 private:
  HighestPmfReporter();

  // The constructor for testing.
  HighestPmfReporter(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing,
      const base::TickClock* clock);

  friend class MockHighestPmfReporter;

  // MemoryUsageMonitor::Observer:
  void OnMemoryPing(MemoryUsage) override;
  void OnReportMetrics();

  // Make the following methods virtual for testing.
  virtual bool FirstNavigationStarted();
  virtual void ReportMetrics();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const base::TickClock* clock_;

  bool first_navigation_detected_ = false;
  double current_highest_pmf_ = 0.0;
  double peak_resident_bytes_at_current_highest_pmf_ = 0.0;
  unsigned webpage_counts_at_current_highest_pmf_ = 0;
  unsigned report_count_ = 0;

  static const char* highest_pmf_metric_names[];
  static const char* peak_resident_bytes_metric_names[];
  static const char* webpage_counts_metric_names[];
  static const base::TimeDelta time_to_report[];
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_HIGHEST_PMF_REPORTER_H_
