// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/calculator.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace responsiveness {

namespace {

// We divide the measurement interval into discretized time slices.
// Each slice is marked as janky if it contained a janky task. A janky task is
// one whose execution latency is greater than kJankThreshold.
constexpr auto kMeasurementInterval = base::Seconds(30);

// A task or event longer than kJankThreshold is considered janky.
constexpr auto kJankThreshold = base::Milliseconds(100);

// If there have been no events/tasks on the UI thread for a significant period
// of time, it's likely because Chrome was suspended.
// This value is copied from queueing_time_estimator.cc:kInvalidPeriodThreshold.
constexpr auto kSuspendInterval = base::Seconds(30);

constexpr char kLatencyEventCategory[] = "latency";

// The names emitted for JankyInterval measurement events.
constexpr char kJankyIntervalEvent[] = "JankyInterval";
constexpr char kJankyIntervalsPerThirtySeconds3Event[] =
    "JankyIntervalsPerThirtySeconds3";

// Given a |jank|, finds each janky slice between |start_time| and |end_time|,
// and adds it to |janky_slices|.
void AddJankySlices(std::set<int>* janky_slices,
                    const Calculator::Jank& jank,
                    base::TimeTicks start_time,
                    base::TimeTicks end_time) {
  // Ignore the first jank threshold, since that's the part of the task/event
  // that wasn't janky.
  base::TimeTicks jank_start = jank.start_time + kJankThreshold;

  // Bound by |start_time| and |end_time|.
  jank_start = std::max(jank_start, start_time);
  base::TimeTicks jank_end = std::min(jank.end_time, end_time);

  // Find each janky slice, and add it to |janky_slices|.
  while (jank_start < jank_end) {
    // Convert |jank_start| to a slice label.
    int64_t label = (jank_start - start_time).IntDiv(kJankThreshold);
    janky_slices->insert(label);

    jank_start += kJankThreshold;
  }
}

}  // namespace

Calculator::Jank::Jank(base::TimeTicks start_time, base::TimeTicks end_time)
    : start_time(start_time), end_time(end_time) {
  DCHECK_LE(start_time, end_time);
}

Calculator::Calculator()
    : last_calculation_time_(base::TimeTicks::Now()),
      most_recent_activity_time_(last_calculation_time_)
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

  if (execution_finish_time - queue_time >= kJankThreshold) {
    GetQueueAndExecutionJanksOnUIThread().emplace_back(queue_time,
                                                       execution_finish_time);
    if (execution_finish_time - execution_start_time >= kJankThreshold) {
      GetExecutionJanksOnUIThread().emplace_back(execution_start_time,
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

  if (execution_finish_time - queue_time >= kJankThreshold) {
    base::AutoLock lock(io_thread_lock_);
    queue_and_execution_janks_on_io_thread_.emplace_back(queue_time,
                                                         execution_finish_time);
    if (execution_finish_time - execution_start_time >= kJankThreshold) {
      execution_janks_on_io_thread_.emplace_back(execution_start_time,
                                                 execution_finish_time);
    }
  }
}

void Calculator::OnFirstIdle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!past_first_idle_);
  past_first_idle_ = true;
}

void Calculator::EmitResponsiveness(JankType jank_type,
                                    size_t janky_slices,
                                    StartupStage startup_stage) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  constexpr size_t kMaxJankySlices = 300;
  DCHECK_LE(janky_slices, kMaxJankySlices);
  switch (jank_type) {
    case JankType::kExecution: {
      UMA_HISTOGRAM_COUNTS_1000(
          "Browser.Responsiveness.JankyIntervalsPerThirtySeconds",
          janky_slices);
      // Only kFirstInterval and kPeriodic are reported with a suffix, stages
      // in between are only part of the unsuffixed histogram.
      if (startup_stage_ == StartupStage::kFirstInterval) {
        UMA_HISTOGRAM_COUNTS_1000(
            "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Initial",
            janky_slices);
      } else if (startup_stage_ == StartupStage::kPeriodic) {
        UMA_HISTOGRAM_COUNTS_1000(
            "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Periodic",
            janky_slices);
      }
      break;
    }
    case JankType::kQueueAndExecution: {
      // Queuing jank doesn't count before OnFirstIdle().
      if (startup_stage_ == StartupStage::kFirstInterval ||
          startup_stage_ == StartupStage::kFirstIntervalDoneWithoutFirstIdle) {
        break;
      }
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3",
          janky_slices, 1, kMaxJankySlices, 50);
      if (startup_stage_ == StartupStage::kFirstIntervalAfterFirstIdle) {
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Initial",
            janky_slices, 1, kMaxJankySlices, 50);
      } else if (startup_stage_ == StartupStage::kPeriodic) {
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Periodic",
            janky_slices, 1, kMaxJankySlices, 50);
      }
      break;
    }
  }
}

void Calculator::EmitResponsivenessTraceEvents(
    JankType jank_type,
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    const std::set<int>& janky_slices) {
  // Only output JankyIntervalsPerThirtySeconds3 event when there are janky
  // slices during the measurement.
  if (janky_slices.empty() || jank_type != JankType::kQueueAndExecution)
    return;

  // Emit a trace event to highlight the duration of janky intervals
  // measurement.
  EmitJankyIntervalsMeasurementTraceEvent(start_time, end_time,
                                          janky_slices.size());

  // |janky_slices| contains the id of janky slices, e.g. {3,6,7,8,41,42}.
  // As such if the slice following slice x is x+1, we coalesce it.
  std::set<int>::const_iterator jank_slice_it = janky_slices.begin();
  while (jank_slice_it != janky_slices.end()) {
    const int start_slice = *jank_slice_it;

    // Find the first slice that is not in the current sequence. After the loop,
    // |jank_slice| will point to the first janky slice in the next sequence
    // (or end() if at the end of the slices) while |current_slice| will
    // point to the first non-janky slice number which correspond to the end of
    // the current sequence.
    int current_slice = start_slice;
    do {
      ++jank_slice_it;
      ++current_slice;
    } while (jank_slice_it != janky_slices.end() &&
             *jank_slice_it == current_slice);

    // Output a trace event for the range [start_slice, current_slice[.
    EmitJankyIntervalsJankTraceEvent(
        start_time + start_slice * kJankThreshold,
        start_time + current_slice * kJankThreshold);
  }
}

void Calculator::EmitJankyIntervalsMeasurementTraceEvent(
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    size_t amount_of_slices) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      kLatencyEventCategory, kJankyIntervalsPerThirtySeconds3Event,
      TRACE_ID_LOCAL(this), start_time, "amount_of_slices", amount_of_slices);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      kLatencyEventCategory, kJankyIntervalsPerThirtySeconds3Event,
      TRACE_ID_LOCAL(this), end_time);
}

void Calculator::EmitJankyIntervalsJankTraceEvent(base::TimeTicks start_time,
                                                  base::TimeTicks end_time) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      kLatencyEventCategory, kJankyIntervalEvent, TRACE_ID_LOCAL(this),
      start_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      kLatencyEventCategory, kJankyIntervalEvent, TRACE_ID_LOCAL(this),
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
    last_calculation_time_ = current_time;
    GetExecutionJanksOnUIThread().clear();
    GetQueueAndExecutionJanksOnUIThread().clear();
    {
      base::AutoLock lock(io_thread_lock_);
      execution_janks_on_io_thread_.clear();
      queue_and_execution_janks_on_io_thread_.clear();
    }
    return;
  }

  base::TimeDelta time_since_last_calculation =
      current_time - last_calculation_time_;
  if (time_since_last_calculation <= kMeasurementInterval)
    return;

  // At least |kMeasurementInterval| time has passed, so we want to move forward
  // |last_calculation_time_| and make measurements based on janks in that
  // interval.
  const base::TimeTicks new_calculation_time =
      current_time - (time_since_last_calculation % kMeasurementInterval);

  // Acquire the janks in the measurement interval from the UI and IO threads.
  std::vector<JankList> execution_janks_from_multiple_threads;
  std::vector<JankList> queue_and_execution_janks_from_multiple_threads;
  execution_janks_from_multiple_threads.push_back(TakeJanksOlderThanTime(
      &GetExecutionJanksOnUIThread(), new_calculation_time));
  queue_and_execution_janks_from_multiple_threads.push_back(
      TakeJanksOlderThanTime(&GetQueueAndExecutionJanksOnUIThread(),
                             new_calculation_time));
  {
    base::AutoLock lock(io_thread_lock_);
    execution_janks_from_multiple_threads.push_back(TakeJanksOlderThanTime(
        &execution_janks_on_io_thread_, new_calculation_time));
    queue_and_execution_janks_from_multiple_threads.push_back(
        TakeJanksOlderThanTime(&queue_and_execution_janks_on_io_thread_,
                               new_calculation_time));
  }

  CalculateResponsiveness(JankType::kExecution,
                          std::move(execution_janks_from_multiple_threads),
                          last_calculation_time_, new_calculation_time);
  CalculateResponsiveness(
      JankType::kQueueAndExecution,
      std::move(queue_and_execution_janks_from_multiple_threads),
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
    JankType jank_type,
    std::vector<JankList> janks_from_multiple_threads,
    base::TimeTicks start_time,
    base::TimeTicks end_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  while (start_time < end_time) {
    const base::TimeTicks current_interval_end_time =
        start_time + kMeasurementInterval;

    // We divide the current measurement interval into slices. Each slice is
    // given a monotonically increasing label, from 0 to |kNumberOfSlices - 1|.
    // Example [all times in milliseconds since UNIX epoch]:
    //   The measurement interval is [50135, 80135].
    //   The slice [50135, 50235] is labeled 0.
    //   The slice [50235, 50335] is labeled 1.
    //   ...
    //   The slice [80035, 80135] is labeled 299.
    std::set<int> janky_slices;

    for (const JankList& janks : janks_from_multiple_threads) {
      for (const Jank& jank : janks) {
        AddJankySlices(&janky_slices, jank, start_time,
                       current_interval_end_time);
      }
    }

    EmitResponsiveness(jank_type, janky_slices.size(), startup_stage_);

    // If the 'latency' tracing category is enabled and we are ready to emit
    // JankyIntervalsPerThirtySeconds3, emit trace events for the measurement
    // duration and the janky slices.
    bool latency_category_enabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(kLatencyEventCategory,
                                       &latency_category_enabled);
    if (latency_category_enabled &&
        (startup_stage_ == StartupStage::kFirstIntervalAfterFirstIdle ||
         startup_stage_ == StartupStage::kPeriodic)) {
      EmitResponsivenessTraceEvents(jank_type, start_time,
                                    current_interval_end_time, janky_slices);
    }

    start_time = current_interval_end_time;
  }
}

Calculator::JankList& Calculator::GetExecutionJanksOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return execution_janks_on_ui_thread_;
}

Calculator::JankList& Calculator::GetQueueAndExecutionJanksOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return queue_and_execution_janks_on_ui_thread_;
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
Calculator::JankList Calculator::TakeJanksOlderThanTime(
    JankList* janks,
    base::TimeTicks end_time) {
  // Find all janks with Jank.start_time < |end_time|.
  auto it = std::partition(
      janks->begin(), janks->end(),
      [&end_time](const Jank& jank) { return jank.start_time < end_time; });

  // Early exit. We don't need to remove any Janks either, since Jank.end_time
  // >= Jank.start_time.
  if (it == janks->begin())
    return JankList();

  JankList janks_to_return(janks->begin(), it);

  // Remove all janks with Jank.end_time < |end_time|.
  auto first_jank_to_keep = std::partition(
      janks->begin(), janks->end(),
      [&end_time](const Jank& jank) { return jank.end_time < end_time; });
  janks->erase(janks->begin(), first_jank_to_keep);
  return janks_to_return;
}

}  // namespace responsiveness
}  // namespace content
