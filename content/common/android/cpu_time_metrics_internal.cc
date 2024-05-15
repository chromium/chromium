// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/cpu_time_metrics_internal.h"

#include <stdint.h>

#include <atomic>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/cpu.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/task/current_thread.h"
#include "base/task/task_observer.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/types/expected.h"
#include "content/common/process_visibility_tracker.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"

namespace content {
namespace internal {
namespace {

bool g_ignore_histogram_allocator_for_testing = false;

static_assert(static_cast<int>(ProcessTypeForUma::kMaxValue) ==
                  PROCESS_TYPE_PPAPI_BROKER,
              "ProcessTypeForUma and CurrentProcessType() require updating");

ProcessTypeForUma CurrentProcessType() {
  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  if (process_type.empty())
    return ProcessTypeForUma::kBrowser;
  if (process_type == switches::kRendererProcess)
    return ProcessTypeForUma::kRenderer;
  if (process_type == switches::kUtilityProcess)
    return ProcessTypeForUma::kUtility;
  if (process_type == switches::kSandboxIPCProcess)
    return ProcessTypeForUma::kSandboxHelper;
  if (process_type == switches::kGpuProcess)
    return ProcessTypeForUma::kGpu;
  if (process_type == switches::kPpapiPluginProcess)
    return ProcessTypeForUma::kPpapiPlugin;
  NOTREACHED_IN_MIGRATION() << "Unexpected process type: " << process_type;
  return ProcessTypeForUma::kUnknown;
}

const char* GetPerThreadHistogramNameForProcessType(ProcessTypeForUma type) {
  switch (type) {
    case ProcessTypeForUma::kBrowser:
      return "Power.CpuTimeSecondsPerThreadType.Browser";
    case ProcessTypeForUma::kRenderer:
      return "Power.CpuTimeSecondsPerThreadType.Renderer";
    case ProcessTypeForUma::kGpu:
      return "Power.CpuTimeSecondsPerThreadType.GPU";
    default:
      return "Power.CpuTimeSecondsPerThreadType.Other";
  }
}

const char* GetAvgCpuLoadHistogramNameForProcessType(ProcessTypeForUma type) {
  switch (type) {
    case ProcessTypeForUma::kBrowser:
      return "Power.AvgCpuLoad.Browser";
    case ProcessTypeForUma::kRenderer:
      return "Power.AvgCpuLoad.Renderer";
    case ProcessTypeForUma::kGpu:
      return "Power.AvgCpuLoad.GPU";
    default:
      return "Power.AvgCpuLoad.Other";
  }
}

// Keep in sync with CpuTimeMetricsThreadType in
// //tools/metrics/histograms/enums.xml.
enum class CpuTimeMetricsThreadType {
  kUnattributedThread = 0,
  kOtherThread,
  kMainThread,
  kIOThread,
  kThreadPoolBackgroundWorkerThread,
  kThreadPoolForegroundWorkerThread,
  kThreadPoolServiceThread,
  kCompositorThread,
  kCompositorTileWorkerThread,
  kVizCompositorThread,
  kRendererUnspecifiedWorkerThread,
  kRendererDedicatedWorkerThread,
  kRendererSharedWorkerThread,
  kRendererAnimationAndPaintWorkletThread,
  kRendererServiceWorkerThread,
  kRendererAudioWorkletThread,
  kRendererFileThread,
  kRendererDatabaseThread,
  kRendererOfflineAudioRenderThread,
  kRendererReverbConvolutionBackgroundThread,
  kRendererHRTFDatabaseLoaderThread,
  kRendererAudioEncoderThread,
  kRendererVideoEncoderThread,
  kMemoryInfraThread,
  kSamplingProfilerThread,
  kNetworkServiceThread,
  kAudioThread,
  kInProcessUtilityThread,
  kInProcessRendererThread,
  kInProcessGpuThread,
  kMaxValue = kInProcessGpuThread,
};

CpuTimeMetricsThreadType GetThreadTypeFromName(const char* const thread_name) {
  if (!thread_name)
    return CpuTimeMetricsThreadType::kOtherThread;

  if (base::MatchPattern(thread_name, "Cr*Main")) {
    return CpuTimeMetricsThreadType::kMainThread;
  } else if (base::MatchPattern(thread_name, "Chrome*IOThread")) {
    return CpuTimeMetricsThreadType::kIOThread;
  } else if (base::MatchPattern(thread_name, "ThreadPool*Foreground*")) {
    return CpuTimeMetricsThreadType::kThreadPoolForegroundWorkerThread;
  } else if (base::MatchPattern(thread_name, "ThreadPool*Background*")) {
    return CpuTimeMetricsThreadType::kThreadPoolBackgroundWorkerThread;
  } else if (base::MatchPattern(thread_name, "ThreadPoolService*")) {
    return CpuTimeMetricsThreadType::kThreadPoolServiceThread;
  } else if (base::MatchPattern(thread_name, "Compositor")) {
    return CpuTimeMetricsThreadType::kCompositorThread;
  } else if (base::MatchPattern(thread_name, "CompositorTileWorker*")) {
    return CpuTimeMetricsThreadType::kCompositorTileWorkerThread;
  } else if (base::MatchPattern(thread_name, "VizCompositor*")) {
    return CpuTimeMetricsThreadType::kVizCompositorThread;
  } else if (base::MatchPattern(thread_name, "unspecified worker*")) {
    return CpuTimeMetricsThreadType::kRendererUnspecifiedWorkerThread;
  } else if (base::MatchPattern(thread_name, "DedicatedWorker*")) {
    return CpuTimeMetricsThreadType::kRendererDedicatedWorkerThread;
  } else if (base::MatchPattern(thread_name, "SharedWorker*")) {
    return CpuTimeMetricsThreadType::kRendererSharedWorkerThread;
  } else if (base::MatchPattern(thread_name, "AnimationWorklet*")) {
    return CpuTimeMetricsThreadType::kRendererAnimationAndPaintWorkletThread;
  } else if (base::MatchPattern(thread_name, "ServiceWorker*")) {
    return CpuTimeMetricsThreadType::kRendererServiceWorkerThread;
  } else if (base::MatchPattern(thread_name, "AudioWorklet*")) {
    return CpuTimeMetricsThreadType::kRendererAudioWorkletThread;
  } else if (base::MatchPattern(thread_name, "File thread")) {
    return CpuTimeMetricsThreadType::kRendererFileThread;
  } else if (base::MatchPattern(thread_name, "Database thread")) {
    return CpuTimeMetricsThreadType::kRendererDatabaseThread;
  } else if (base::MatchPattern(thread_name, "OfflineAudioRender*")) {
    return CpuTimeMetricsThreadType::kRendererOfflineAudioRenderThread;
  } else if (base::MatchPattern(thread_name, "Reverb convolution*")) {
    return CpuTimeMetricsThreadType::kRendererReverbConvolutionBackgroundThread;
  } else if (base::MatchPattern(thread_name, "HRTF*")) {
    return CpuTimeMetricsThreadType::kRendererHRTFDatabaseLoaderThread;
  } else if (base::MatchPattern(thread_name, "Audio encoder*")) {
    return CpuTimeMetricsThreadType::kRendererAudioEncoderThread;
  } else if (base::MatchPattern(thread_name, "Video encoder*")) {
    return CpuTimeMetricsThreadType::kRendererVideoEncoderThread;
  } else if (base::MatchPattern(thread_name, "MemoryInfra")) {
    return CpuTimeMetricsThreadType::kMemoryInfraThread;
  } else if (base::MatchPattern(thread_name, "StackSamplingProfiler")) {
    return CpuTimeMetricsThreadType::kSamplingProfilerThread;
  } else if (base::MatchPattern(thread_name, "NetworkService")) {
    return CpuTimeMetricsThreadType::kNetworkServiceThread;
  } else if (base::MatchPattern(thread_name, "AudioThread")) {
    return CpuTimeMetricsThreadType::kAudioThread;
  } else if (base::MatchPattern(thread_name, "Chrome_InProcUtilityThread")) {
    return CpuTimeMetricsThreadType::kInProcessUtilityThread;
  } else if (base::MatchPattern(thread_name, "Chrome_InProcRendererThread")) {
    return CpuTimeMetricsThreadType::kInProcessRendererThread;
  } else if (base::MatchPattern(thread_name, "Chrome_InProcGpuThread")) {
    return CpuTimeMetricsThreadType::kInProcessGpuThread;
  }

  // TODO(eseckler): Also break out Android's RenderThread here somehow?

  return CpuTimeMetricsThreadType::kOtherThread;
}

}  // namespace

// Reports per-thread CPU time breakdowns.
class ProcessCpuTimeMetrics::DetailedCpuTimeMetrics {
 public:
  DetailedCpuTimeMetrics(base::ProcessMetrics* process_metrics,
                         ProcessTypeForUma process_type)
      : process_metrics_(process_metrics),
        process_type_(process_type),
        // DetailedCpuTimeMetrics is created on the main thread of the process
        // but lives on the thread pool sequence afterwards.
        main_thread_id_(base::PlatformThread::CurrentId()) {
    DETACH_FROM_SEQUENCE(thread_pool_);
  }

  void CollectOnThreadPool() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_);

    // This might overflow. We only care that it is different for each cycle.
    current_cycle_++;

    // Skip reporting any values into histograms until histogram persistence is
    // set up. Otherwise, we would create the histograms without persistence and
    // lose data at process termination (particularly in child processes).
    if (!base::GlobalHistogramAllocator::Get() &&
        !g_ignore_histogram_allocator_for_testing) {
      return;
    }

    const base::expected<base::TimeDelta, base::ProcessCPUUsageError>
        cumulative_cpu_time = process_metrics_->GetCumulativeCPUUsage();
    base::TimeDelta process_cpu_time_delta;
    if (cumulative_cpu_time.has_value()) {
      process_cpu_time_delta = cumulative_cpu_time.value() - reported_cpu_time_;
      reported_cpu_time_ = cumulative_cpu_time.value();
    }

    // Also report a breakdown by thread type.
    base::TimeDelta unattributed_delta = process_cpu_time_delta;
    if (process_metrics_->GetCumulativeCPUUsagePerThread(
            cumulative_thread_times_)) {
      for (const auto& entry : cumulative_thread_times_) {
        base::PlatformThreadId tid = entry.first;
        base::TimeDelta cumulative_time = entry.second;

        auto it_and_inserted = thread_details_.emplace(
            tid, ThreadDetails{base::TimeDelta(), current_cycle_});
        ThreadDetails* thread_details = &it_and_inserted.first->second;

        if (it_and_inserted.second) {
          // New thread.
          thread_details->type = GuessThreadType(tid);
        }

        thread_details->last_updated_cycle = current_cycle_;

        // Skip negative or null values, might be a transient collection error.
        if (cumulative_time <= base::TimeDelta())
          continue;

        if (cumulative_time < thread_details->reported_cpu_time) {
          // PlatformThreadId was likely reused, reset the details.
          thread_details->reported_cpu_time = base::TimeDelta();
          thread_details->type = GuessThreadType(tid);
        }

        base::TimeDelta thread_delta =
            cumulative_time - thread_details->reported_cpu_time;
        unattributed_delta -= thread_delta;

        ReportThreadCpuTimeDelta(thread_details->type, thread_delta);
        thread_details->reported_cpu_time = cumulative_time;
      }

      // Erase tracking for threads that have disappeared, as their
      // PlatformThreadId may be reused later.
      for (auto it = thread_details_.begin(); it != thread_details_.end();) {
        if (it->second.last_updated_cycle == current_cycle_) {
          it++;
        } else {
          it = thread_details_.erase(it);
        }
      }
    }

    // Report the difference of the process's total CPU time and all thread's
    // CPU time as unattributed time (e.g. time consumed by threads that died).
    // `unattributed_delta` can be negative if GetCumulativeCPUUsagePerThread()
    // reported more time than GetCumulativeCPUUsage() did, or if
    // GetCumulativeCPUUsage() failed so `unattributed_delta` started at 0.
    if (unattributed_delta.is_positive()) {
      ReportThreadCpuTimeDelta(CpuTimeMetricsThreadType::kUnattributedThread,
                               unattributed_delta);
    }
  }

 private:
  struct ThreadDetails {
    base::TimeDelta reported_cpu_time;
    uint32_t last_updated_cycle = 0;
    CpuTimeMetricsThreadType type = CpuTimeMetricsThreadType::kOtherThread;
  };

  void ReportThreadCpuTimeDelta(CpuTimeMetricsThreadType type,
                                base::TimeDelta cpu_time_delta) {
    // Histogram name cannot change after being used once. That's ok since this
    // only depends on the process type, which also doesn't change.
    static const char* histogram_name =
        GetPerThreadHistogramNameForProcessType(process_type_);
    // Histograms use int internally. Make sure it doesn't overflow.
    int capped_value = std::min<int64_t>(cpu_time_delta.InMicroseconds(),
                                         std::numeric_limits<int>::max());
    UMA_HISTOGRAM_SCALED_ENUMERATION(histogram_name, type, capped_value,
                                     base::Time::kMicrosecondsPerSecond);
  }

  CpuTimeMetricsThreadType GuessThreadType(base::PlatformThreadId tid) {
    // Match the main thread by TID, so that this also works for WebView, where
    // the main thread can have an arbitrary name.
    if (tid == main_thread_id_)
      return CpuTimeMetricsThreadType::kMainThread;
    const char* name = base::ThreadIdNameManager::GetInstance()->GetName(tid);
    return GetThreadTypeFromName(name);
  }

  // Accessed on |task_runner_|.
  SEQUENCE_CHECKER(thread_pool_);
  raw_ptr<base::ProcessMetrics> process_metrics_;
  ProcessTypeForUma process_type_;
  uint32_t current_cycle_ = 0;
  base::PlatformThreadId main_thread_id_;
  base::TimeDelta reported_cpu_time_;
  base::flat_map<base::PlatformThreadId, ThreadDetails> thread_details_;
  // Stored as instance variable to avoid allocation churn.
  base::ProcessMetrics::CPUUsagePerThread cumulative_thread_times_;
};

// static
ProcessCpuTimeMetrics* ProcessCpuTimeMetrics::GetInstance() {
  static base::NoDestructor<ProcessCpuTimeMetrics> instance;
  return instance.get();
}

ProcessCpuTimeMetrics::ProcessCpuTimeMetrics()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT,
           // TODO(eseckler): Consider hooking into process shutdown on
           // desktop to reduce metric data loss.
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      process_metrics_(base::ProcessMetrics::CreateCurrentProcessMetrics()),
      process_type_(CurrentProcessType()),
      detailed_metrics_(
          std::make_unique<DetailedCpuTimeMetrics>(process_metrics_.get(),
                                                   process_type_)) {
  DETACH_FROM_SEQUENCE(thread_pool_);

  // Browser and GPU processes have a longer lifetime (don't disappear between
  // navigations), and typically execute a large number of small main-thread
  // tasks. For these processes, choose a higher reporting interval.
  if (process_type_ == ProcessTypeForUma::kBrowser ||
      process_type_ == ProcessTypeForUma::kGpu) {
    reporting_interval_ = kReportAfterEveryNTasksPersistentProcess;
  } else {
    reporting_interval_ = kReportAfterEveryNTasksOtherProcess;
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ProcessCpuTimeMetrics::InitializeOnThreadPool,
                                base::Unretained(this)));

  base::CurrentThread::Get()->AddTaskObserver(this);
}

ProcessCpuTimeMetrics::~ProcessCpuTimeMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);

  // Note that this object can only be destroyed in unit tests. We clean up
  // the members and observer registrations but assume that the test takes
  // care of any threading issues.
  base::CurrentThread::Get()->RemoveTaskObserver(this);
  ProcessVisibilityTracker::GetInstance()->RemoveObserver(this);
}

void ProcessCpuTimeMetrics::InitializeOnThreadPool() {
  ProcessVisibilityTracker::GetInstance()->AddObserver(this);
  PerformFullCollectionOnThreadPool();
}

// base::TaskObserver implementation:
void ProcessCpuTimeMetrics::WillProcessTask(
    const base::PendingTask& pending_task,
    bool was_blocked_or_low_priority) {}

void ProcessCpuTimeMetrics::DidProcessTask(
    const base::PendingTask& pending_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);
  // Periodically perform a full collection that includes |detailed_metrics_| in
  // addition to high-level metrics.
  task_counter_++;
  if (task_counter_ == reporting_interval_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ProcessCpuTimeMetrics::PerformFullCollectionOnThreadPool,
            base::Unretained(this)));
    task_counter_ = 0;
  }
}

// ProcessVisibilityTracker::ProcessVisibilityObserver implementation:
void ProcessCpuTimeMetrics::OnVisibilityChanged(bool visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_);
  // Collect high-level metrics that include a visibility breakdown and
  // attribute them to the old value of |is_visible_| before updating it.
  CollectHighLevelMetricsOnThreadPool();
  is_visible_ = visible;
}

void ProcessCpuTimeMetrics::PerformFullCollectionOnThreadPool() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_);
  CollectHighLevelMetricsOnThreadPool();
  detailed_metrics_->CollectOnThreadPool();
}

void ProcessCpuTimeMetrics::CollectHighLevelMetricsOnThreadPool() {
  // Skip reporting any values into histograms until histogram persistence is
  // set up. Otherwise, we would create the histograms without persistence and
  // lose data at process termination (particularly in child processes).
  if (!base::GlobalHistogramAllocator::Get() &&
      !g_ignore_histogram_allocator_for_testing) {
    return;
  }

  const base::expected<base::TimeDelta, base::ProcessCPUUsageError>
      cumulative_cpu_usage = process_metrics_->GetCumulativeCPUUsage();
  base::TimeDelta process_cpu_time_delta;
  if (cumulative_cpu_usage.has_value()) {
    process_cpu_time_delta = cumulative_cpu_usage.value() - reported_cpu_time_;
  }
  // Don't report anything if GetCumulativeCPUUsage() failed or the delta is 0.
  if (process_cpu_time_delta.is_positive()) {
    const base::TimeDelta cumulative_cpu_time = cumulative_cpu_usage.value();

    UMA_HISTOGRAM_SCALED_ENUMERATION("Power.CpuTimeSecondsPerProcessType",
                                     process_type_,
                                     process_cpu_time_delta.InMicroseconds(),
                                     base::Time::kMicrosecondsPerSecond);
    if (is_visible_.has_value()) {
      if (*is_visible_) {
        UMA_HISTOGRAM_SCALED_ENUMERATION(
            "Power.CpuTimeSecondsPerProcessType.Foreground", process_type_,
            process_cpu_time_delta.InMicroseconds(),
            base::Time::kMicrosecondsPerSecond);
      } else {
        UMA_HISTOGRAM_SCALED_ENUMERATION(
            "Power.CpuTimeSecondsPerProcessType.Background", process_type_,
            process_cpu_time_delta.InMicroseconds(),
            base::Time::kMicrosecondsPerSecond);
      }
    } else {
      UMA_HISTOGRAM_SCALED_ENUMERATION(
          "Power.CpuTimeSecondsPerProcessType.Unattributed", process_type_,
          process_cpu_time_delta.InMicroseconds(),
          base::Time::kMicrosecondsPerSecond);
    }

    reported_cpu_time_ = cumulative_cpu_time;

    ReportAverageCpuLoad(cumulative_cpu_time);
  }
}

void ProcessCpuTimeMetrics::ReportAverageCpuLoad(
    base::TimeDelta cumulative_cpu_time) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (cpu_load_report_time_ == base::TimeTicks()) {
    cpu_load_report_time_ = now;
    cpu_time_on_last_load_report_ = cumulative_cpu_time;
  }

  base::TimeDelta time_since_report = now - cpu_load_report_time_;
  if (time_since_report >= kAvgCpuLoadReportInterval) {
    base::TimeDelta cpu_time_since_report =
        cumulative_cpu_time - cpu_time_on_last_load_report_;
    int load = 100LL * cpu_time_since_report.InMilliseconds() /
               time_since_report.InMilliseconds();
    static const char* histogram_name =
        GetAvgCpuLoadHistogramNameForProcessType(process_type_);
    // CPU load can be greater than 100% because of multiple cores.
    // That's why we use UmaHistogramCounts, not UmaHistogramPercentage.
    base::UmaHistogramCounts1000(histogram_name, load);

    cpu_load_report_time_ = now;
    cpu_time_on_last_load_report_ = cumulative_cpu_time;
  }
}

void ProcessCpuTimeMetrics::PerformFullCollectionForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ProcessCpuTimeMetrics::PerformFullCollectionOnThreadPool,
                     base::Unretained(this)));
}

void ProcessCpuTimeMetrics::WaitForCollectionForTesting() const {
  base::RunLoop run_loop;
  // Post the QuitClosure to execute after any pending collection.
  task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// static
std::unique_ptr<ProcessCpuTimeMetrics>
ProcessCpuTimeMetrics::CreateForTesting() {
  std::unique_ptr<ProcessCpuTimeMetrics> ptr;
  // Can't use std::make_unique due to private constructor.
  ptr.reset(new ProcessCpuTimeMetrics());
  return ptr;
}

// static
void ProcessCpuTimeMetrics::SetIgnoreHistogramAllocatorForTesting(bool ignore) {
  g_ignore_histogram_allocator_for_testing = ignore;
}

}  // namespace internal
}  // namespace content
