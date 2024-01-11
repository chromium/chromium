// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_CPU_TIME_METRICS_INTERNAL_H_
#define CONTENT_COMMON_ANDROID_CPU_TIME_METRICS_INTERNAL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/process/process_metrics.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_observer.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/process_visibility_tracker.h"

namespace content {
namespace internal {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Histogram macros expect an enum class with kMaxValue. Because
// content::ProcessType cannot be migrated to this style at the moment, we
// specify a separate version here. Keep in sync with content::ProcessType.
// TODO(eseckler): Replace with content::ProcessType after its migration.
enum class ProcessTypeForUma {
  kUnknown = 1,
  kBrowser,
  kRenderer,
  kPluginDeprecated,
  kWorkerDeprecated,
  kUtility,
  kZygote,
  kSandboxHelper,
  kGpu,
  kPpapiPlugin,
  kPpapiBroker,
  kMaxValue = kPpapiBroker,
};

// Samples the process's CPU time after a specific number of task were executed
// on the current thread (process main). The number of tasks is a crude proxy
// for CPU activity within this process. We sample more frequently when the
// process is more active, thus ensuring we lose little CPU time attribution
// when the process is terminated, even after it was very active.
//
// Also samples some of the breakdowns when the process's visibility changes.
class CONTENT_EXPORT ProcessCpuTimeMetrics
    : public base::TaskObserver,
      public ProcessVisibilityTracker::ProcessVisibilityObserver {
 public:
  static ProcessCpuTimeMetrics* GetInstance();

  ~ProcessCpuTimeMetrics() override;

  // base::TaskObserver implementation:
  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override;

  void DidProcessTask(const base::PendingTask& pending_task) override;

  // ProcessVisibilityTracker::ProcessVisibilityObserver implementation:
  void OnVisibilityChanged(bool visible) override;

  void PerformFullCollectionForTesting();
  void WaitForCollectionForTesting() const;

  static std::unique_ptr<ProcessCpuTimeMetrics> CreateForTesting();
  static void SetIgnoreHistogramAllocatorForTesting(bool ignore);

 private:
  friend class base::NoDestructor<ProcessCpuTimeMetrics>;

  class DetailedCpuTimeMetrics;

  explicit ProcessCpuTimeMetrics();

  void InitializeOnThreadPool();
  void PerformFullCollectionOnThreadPool();
  void CollectHighLevelMetricsOnThreadPool();
  void ReportAverageCpuLoad(base::TimeDelta cumulative_cpu_time);

  // Sample CPU time after a certain number of main-thread task to balance
  // overhead of sampling and loss at process termination.
  static constexpr int kReportAfterEveryNTasksPersistentProcess = 2500;
  static constexpr int kReportAfterEveryNTasksOtherProcess = 1000;
  static constexpr base::TimeDelta kAvgCpuLoadReportInterval =
      base::Seconds(30);
  static constexpr base::TimeDelta kIdleCpuLoadReportInterval =
      base::Seconds(5);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Accessed on main thread.
  SEQUENCE_CHECKER(main_thread_);
  int task_counter_ = 0;
  int reporting_interval_ = 0;  // set in constructor.

  // Accessed on |task_runner_|.
  SEQUENCE_CHECKER(thread_pool_);
  std::unique_ptr<base::ProcessMetrics> process_metrics_;
  std::optional<bool> is_visible_;
  ProcessTypeForUma process_type_;
  base::TimeDelta reported_cpu_time_;
  base::TimeDelta cpu_time_on_last_load_report_;
  base::TimeTicks cpu_load_report_time_;

  // Lives on |task_runner_| after construction.
  std::unique_ptr<DetailedCpuTimeMetrics> detailed_metrics_;
};

}  // namespace internal
}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_CPU_TIME_METRICS_INTERNAL_H_
