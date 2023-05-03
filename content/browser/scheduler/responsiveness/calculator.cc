// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/calculator.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace responsiveness {

namespace {

// We divide the measurement interval into discretized time slices. Each slice
// is marked as congested if it contained a congested task. A congested task is
// one whose execution latency is greater than kCongestionThreshold.
constexpr auto kMeasurementPeriod = base::Seconds(30);

// A task or event longer than kCongestionThreshold is considered congested.
constexpr auto kCongestionThreshold = base::Milliseconds(100);

// If there have been no events/tasks on the UI thread for a significant period
// of time, it's likely because Chrome was suspended.
// This value is copied from queueing_time_estimator.cc:kInvalidPeriodThreshold.
constexpr auto kSuspendInterval = base::Seconds(30);

constexpr char kLatencyEventCategory[] = "latency";

// The names emitted for CongestedIntervals measurement events.
constexpr char kCongestedIntervalEvent[] = "CongestedInterval";
constexpr char kCongestedIntervalsMeasurementEvent[] =
    "CongestedIntervals measurement period";

// Given a |congestion|, finds each congested slice between |start_time| and
// |end_time|, and adds it to |congested_slices|.
void AddCongestedSlices(std::set<int>* congested_slices,
                        const Calculator::Congestion& congestion,
                        base::TimeTicks start_time,
                        base::TimeTicks end_time) {
  // Ignore the first congestion threshold, since that's the part of the
  // task/event that wasn't congested.
  base::TimeTicks congestion_start =
      congestion.start_time + kCongestionThreshold;

  // Bound by |start_time| and |end_time|.
  congestion_start = std::max(congestion_start, start_time);
  base::TimeTicks congestion_end = std::min(congestion.end_time, end_time);

  // Find each congested slice, and add it to |congested_slices|.
  while (congestion_start < congestion_end) {
    // Convert |congestion_start| to a slice label.
    int64_t label =
        (congestion_start - start_time).IntDiv(kCongestionThreshold);
    congested_slices->insert(label);

    congestion_start += kCongestionThreshold;
  }
}

}  // namespace

Calculator::Congestion::Congestion(base::TimeTicks start_time,
                                   base::TimeTicks end_time)
    : start_time(start_time), end_time(end_time) {
  DCHECK_LE(start_time, end_time);
}

Calculator::Calculator(
    std::unique_ptr<ResponsivenessCalculatorDelegate> delegate)
    : last_calculation_time_(base::TimeTicks::Now()),
      most_recent_activity_time_(last_calculation_time_),
      delegate_(std::move(delegate))
#if BUILDFLAG(IS_ANDROID)
      ,
      application_status_listener_(
          base::android::ApplicationStatusListener::New(
              base::BindRepeating(&Calculator::OnApplicationStateChanged,
                                  // Listener is destroyed at destructor, and
                                  // object will be alive for any callback.
                                  base::Unretained(this)))) {
  // This class assumes construction and access from the UI thread from all
  // methods that aren't explicitly flagged otherwise (i.e. *OnIOThread()).
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  OnApplicationStateChanged(
      base::android::ApplicationStatusListener::GetState());
}
#else
{
}
#endif

Calculator::~Calculator() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void Calculator::TaskOrEventFinishedOnUIThread(
    base::TimeTicks queue_time,
    base::TimeTicks execution_start_time,
    base::TimeTicks execution_finish_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GE(execution_start_time, queue_time);

  if (execution_finish_time - queue_time >= kCongestionThreshold) {
    GetCongestionOnUIThread().emplace_back(queue_time, execution_finish_time);
    if (execution_finish_time - execution_start_time >= kCongestionThreshold) {
      GetExecutionCongestionOnUIThread().emplace_back(execution_start_time,
                                                      execution_finish_time);
    }
  }

  // We rely on the assumption that |finish_time| is the current time.
  CalculateResponsivenessIfNecessary(/*current_time=*/execution_finish_time);
}

void Calculator::TaskOrEventFinishedOnIOThread(
    base::TimeTicks queue_time,
    base::TimeTicks execution_start_time,
    base::TimeTicks execution_finish_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GE(execution_start_time, queue_time);

  if (execution_finish_time - queue_time >= kCongestionThreshold) {
    base::AutoLock lock(io_thread_lock_);
    congestion_on_io_thread_.emplace_back(queue_time, execution_finish_time);
    if (execution_finish_time - execution_start_time >= kCongestionThreshold) {
      execution_congestion_on_io_thread_.emplace_back(execution_start_time,
                                                      execution_finish_time);
    }
  }
}

void Calculator::OnFirstIdle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!past_first_idle_);
  past_first_idle_ = true;
}

void Calculator::EmitResponsiveness(CongestionType congestion_type,
                                    size_t num_congested_slices,
                                    StartupStage startup_stage) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  static constexpr size_t kMaxCongestedSlices =
      kMeasurementPeriod / kCongestionThreshold;
  static constexpr size_t kBucketCount = 50;
  DCHECK_LE(num_congested_slices, kMaxCongestedSlices);
  switch (congestion_type) {
    case CongestionType::kExecutionOnly: {
      UMA_HISTOGRAM_COUNTS_1000("Browser.MainThreadsCongestion.RunningOnly",
                                num_congested_slices);
      // Only kFirstInterval and kPeriodic are reported with a suffix, stages
      // in between are only part of the unsuffixed histogram.
      if (startup_stage_ == StartupStage::kFirstInterval) {
        UMA_HISTOGRAM_COUNTS_1000(
            "Browser.MainThreadsCongestion.RunningOnly.Initial",
            num_congested_slices);
      } else if (startup_stage_ == StartupStage::kPeriodic) {
        UMA_HISTOGRAM_COUNTS_1000(
            "Browser.MainThreadsCongestion.RunningOnly.Periodic",
            num_congested_slices);
      }
      break;
    }
    case CongestionType::kQueueAndExecution: {
      // Queuing congestion doesn't count before OnFirstIdle().
      if (startup_stage_ == StartupStage::kFirstInterval ||
          startup_stage_ == StartupStage::kFirstIntervalDoneWithoutFirstIdle) {
        break;
      }
      UMA_HISTOGRAM_CUSTOM_COUNTS("Browser.MainThreadsCongestion",
                                  num_congested_slices, 1, kMaxCongestedSlices,
                                  kBucketCount);
      if (delegate_) {
        delegate_->OnResponsivenessEmitted(num_congested_slices, 1,
                                           kMaxCongestedSlices, kBucketCount);
      }
      if (startup_stage_ == StartupStage::kFirstIntervalAfterFirstIdle) {
        UMA_HISTOGRAM_CUSTOM_COUNTS("Browser.MainThreadsCongestion.Initial",
                                    num_congested_slices, 1,
                                    kMaxCongestedSlices, kBucketCount);
      } else if (startup_stage_ == StartupStage::kPeriodic) {
        UMA_HISTOGRAM_CUSTOM_COUNTS("Browser.MainThreadsCongestion.Periodic",
                                    num_congested_slices, 1,
                                    kMaxCongestedSlices, kBucketCount);
      }
      break;
    }
  }
}

void Calculator::EmitResponsivenessTraceEvents(
    CongestionType congestion_type,
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    const std::set<int>& congested_slices) {
  // Only output kCongestedIntervalsMeasurementEvent event when there are
  // congested slices during the measurement.
  if (congested_slices.empty() ||
      congestion_type != CongestionType::kQueueAndExecution)
    return;

  // Emit a trace event to highlight the duration of congested intervals
  // measurement.
  EmitCongestedIntervalsMeasurementTraceEvent(start_time, end_time,
                                              congested_slices.size());

  // |congested_slices| contains the id of congested slices, e.g.
  // {3,6,7,8,41,42}. As such if the slice following slice x is x+1, we coalesce
  // it.
  std::set<int>::const_iterator congested_slice_it = congested_slices.begin();
  while (congested_slice_it != congested_slices.end()) {
    const int start_slice = *congested_slice_it;

    // Find the first slice that is not in the current sequence. After the loop,
    // |congested_slice_it| will point to the first congested slice in the next
    // sequence(or end() if at the end of the slices) while |current_slice|
    // will point to the first non-congested slice number which correspond to
    // the end of the current sequence.
    int current_slice = start_slice;
    do {
      ++congested_slice_it;
      ++current_slice;
    } while (congested_slice_it != congested_slices.end() &&
             *congested_slice_it == current_slice);

    // Output a trace event for the range [start_slice, current_slice[.
    EmitCongestedIntervalTraceEvent(
        start_time + start_slice * kCongestionThreshold,
        start_time + current_slice * kCongestionThreshold);
  }
}

void Calculator::EmitCongestedIntervalsMeasurementTraceEvent(
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    size_t amount_of_slices) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      kLatencyEventCategory, kCongestedIntervalsMeasurementEvent,
      TRACE_ID_LOCAL(this), start_time, "amount_of_slices", amount_of_slices);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      kLatencyEventCategory, kCongestedIntervalsMeasurementEvent,
      TRACE_ID_LOCAL(this), end_time);
}

void Calculator::EmitCongestedIntervalTraceEvent(base::TimeTicks start_time,
                                                 base::TimeTicks end_time) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      kLatencyEventCategory, kCongestedIntervalEvent, TRACE_ID_LOCAL(this),
      start_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      kLatencyEventCategory, kCongestedIntervalEvent, TRACE_ID_LOCAL(this),
      end_time);
}

base::TimeTicks Calculator::GetLastCalculationTime() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return last_calculation_time_;
}

void Calculator::CalculateResponsivenessIfNecessary(
    base::TimeTicks current_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::TimeTicks last_activity_time = most_recent_activity_time_;
  most_recent_activity_time_ = current_time;

  // We intentionally dump all data if it appears that Chrome was suspended.
  // [e.g. machine is asleep, process is backgrounded on Android]. We don't have
  // an explicit signal for this. Instead, we rely on the assumption that when
  // Chrome is not suspended, there is a steady stream of tasks and events on
  // the UI thread. If there's been a significant amount of time since the last
  // calculation, then it's likely because Chrome was suspended.
  bool is_suspended = current_time - last_activity_time > kSuspendInterval;
#if BUILDFLAG(IS_ANDROID)
  is_suspended |= !is_application_visible_;
#endif
  if (is_suspended) {
    // Notify the delegate that the interval ended so that it can reset its
    // accumulated data for the current interval.
    if (delegate_) {
      delegate_->OnMeasurementIntervalEnded();
    }
    last_calculation_time_ = current_time;
    GetExecutionCongestionOnUIThread().clear();
    GetCongestionOnUIThread().clear();
    {
      base::AutoLock lock(io_thread_lock_);
      execution_congestion_on_io_thread_.clear();
      congestion_on_io_thread_.clear();
    }
    return;
  }

  base::TimeDelta time_since_last_calculation =
      current_time - last_calculation_time_;
  if (time_since_last_calculation <= kMeasurementPeriod)
    return;

  // At least |kMeasurementPeriod| time has passed, so we want to move forward
  // |last_calculation_time_| and make measurements based on congestions in that
  // interval.
  const base::TimeTicks new_calculation_time =
      current_time - (time_since_last_calculation % kMeasurementPeriod);

  // Acquire the congestions in the measurement interval from the UI and IO
  // threads.
  std::vector<CongestionList> execution_congestion_from_multiple_threads;
  std::vector<CongestionList> congestion_from_multiple_threads;
  execution_congestion_from_multiple_threads.push_back(
      TakeCongestionsOlderThanTime(&GetExecutionCongestionOnUIThread(),
                                   new_calculation_time));
  congestion_from_multiple_threads.push_back(TakeCongestionsOlderThanTime(
      &GetCongestionOnUIThread(), new_calculation_time));
  {
    base::AutoLock lock(io_thread_lock_);
    execution_congestion_from_multiple_threads.push_back(
        TakeCongestionsOlderThanTime(&execution_congestion_on_io_thread_,
                                     new_calculation_time));
    congestion_from_multiple_threads.push_back(TakeCongestionsOlderThanTime(
        &congestion_on_io_thread_, new_calculation_time));
  }

  if (delegate_) {
    delegate_->OnMeasurementIntervalEnded();
  }

  CalculateResponsiveness(CongestionType::kExecutionOnly,
                          std::move(execution_congestion_from_multiple_threads),
                          last_calculation_time_, new_calculation_time);
  CalculateResponsiveness(CongestionType::kQueueAndExecution,
                          std::move(congestion_from_multiple_threads),
                          last_calculation_time_, new_calculation_time);

  if (startup_stage_ == StartupStage::kFirstInterval)
    startup_stage_ = StartupStage::kFirstIntervalDoneWithoutFirstIdle;

  if (startup_stage_ == StartupStage::kFirstIntervalDoneWithoutFirstIdle &&
      past_first_idle_) {
    startup_stage_ = StartupStage::kFirstIntervalAfterFirstIdle;
  } else if (startup_stage_ == StartupStage::kFirstIntervalAfterFirstIdle) {
    startup_stage_ = StartupStage::kPeriodic;
  }

  last_calculation_time_ = new_calculation_time;
}

void Calculator::CalculateResponsiveness(
    CongestionType congestion_type,
    std::vector<CongestionList> congestions_from_multiple_threads,
    base::TimeTicks start_time,
    base::TimeTicks end_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  while (start_time < end_time) {
    const base::TimeTicks current_interval_end_time =
        start_time + kMeasurementPeriod;

    // We divide the current measurement interval into slices. Each slice is
    // given a monotonically increasing label, from 0 to |kNumberOfSlices - 1|.
    // Example [all times in milliseconds since UNIX epoch]:
    //   The measurement interval is [50135, 80135].
    //   The slice [50135, 50235] is labeled 0.
    //   The slice [50235, 50335] is labeled 1.
    //   ...
    //   The slice [80035, 80135] is labeled 299.
    std::set<int> congested_slices;

    for (const CongestionList& congestions :
         congestions_from_multiple_threads) {
      for (const Congestion& congestion : congestions) {
        AddCongestedSlices(&congested_slices, congestion, start_time,
                           current_interval_end_time);
      }
    }

    EmitResponsiveness(congestion_type, congested_slices.size(),
                       startup_stage_);

    // If the 'latency' tracing category is enabled and we are ready to observe
    // queuing times (past first idle), emit trace events for the measurement
    // duration and the congested slices.
    bool latency_category_enabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(kLatencyEventCategory,
                                       &latency_category_enabled);
    if (latency_category_enabled &&
        (startup_stage_ == StartupStage::kFirstIntervalAfterFirstIdle ||
         startup_stage_ == StartupStage::kPeriodic)) {
      EmitResponsivenessTraceEvents(congestion_type, start_time,
                                    current_interval_end_time,
                                    congested_slices);
    }

    start_time = current_interval_end_time;
  }
}

Calculator::CongestionList& Calculator::GetExecutionCongestionOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return execution_congestion_on_ui_thread_;
}

Calculator::CongestionList& Calculator::GetCongestionOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return congestion_on_ui_thread_;
}

#if BUILDFLAG(IS_ANDROID)
void Calculator::OnApplicationStateChanged(
    base::android::ApplicationState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  switch (state) {
    case base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
      // The application is still visible and partially hidden in paused state.
      is_application_visible_ = true;
      break;
    case base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
      is_application_visible_ = false;
      break;
    case base::android::APPLICATION_STATE_UNKNOWN:
      break;  // Keep in previous state.
  }
}
#endif

// static
Calculator::CongestionList Calculator::TakeCongestionsOlderThanTime(
    CongestionList* congestions,
    base::TimeTicks end_time) {
  // Find all congestions with Congestion.start_time < |end_time|.
  auto it = std::partition(congestions->begin(), congestions->end(),
                           [&end_time](const Congestion& congestion) {
                             return congestion.start_time < end_time;
                           });

  // Early exit. We don't need to remove any Congestions either, since
  // Congestion.end_time
  // >= Congestion.start_time.
  if (it == congestions->begin())
    return CongestionList();

  CongestionList congestions_to_return(congestions->begin(), it);

  // Remove all congestions with Congestion.end_time < |end_time|.
  auto first_congestion_to_keep =
      std::partition(congestions->begin(), congestions->end(),
                     [&end_time](const Congestion& congestion) {
                       return congestion.end_time < end_time;
                     });
  congestions->erase(congestions->begin(), first_congestion_to_keep);
  return congestions_to_return;
}

}  // namespace responsiveness
}  // namespace content
