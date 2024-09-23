// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/loader/interactive_detector.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/profiler/sample_metadata.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

namespace {

// Used to generate a unique id when emitting the "Long Input Delay" trace
// event and metadata.
int g_num_long_input_events = 0;

// The threshold to emit the "Long Input Delay" trace event is the 99th
// percentile of the histogram on Windows Stable as of Feb 25, 2020.
constexpr base::TimeDelta kInputDelayTraceEventThreshold =
    base::Milliseconds(250);

// The threshold to emit the "Long First Input Delay" trace event is the 99th
// percentile of the histogram on Windows Stable as of Feb 27, 2020.
constexpr base::TimeDelta kFirstInputDelayTraceEventThreshold =
    base::Milliseconds(575);

}  // namespace

// Required length of main thread and network quiet window for determining
// Time to Interactive.
constexpr auto kTimeToInteractiveWindow = base::Seconds(5);
// Network is considered "quiet" if there are no more than 2 active network
// requests for this duration of time.
constexpr int kNetworkQuietMaximumConnections = 2;

const char kHistogramInputDelay[] = "PageLoad.InteractiveTiming.InputDelay3";
const char kHistogramInputTimestamp[] =
    "PageLoad.InteractiveTiming.InputTimestamp3";
const char kHistogramProcessingTime[] =
    "PageLoad.InteractiveTiming.ProcessingTime";
const char kHistogramTimeToNextPaint[] =
    "PageLoad.InteractiveTiming.TimeToNextPaint";

// static
const char InteractiveDetector::kSupplementName[] = "InteractiveDetector";

InteractiveDetector* InteractiveDetector::From(Document& document) {
  InteractiveDetector* detector =
      Supplement<Document>::From<InteractiveDetector>(document);
  if (!detector) {
    detector = MakeGarbageCollected<InteractiveDetector>(
        document, std::make_unique<NetworkActivityChecker>(&document));
    Supplement<Document>::ProvideTo(document, detector);
  }
  return detector;
}

const char* InteractiveDetector::SupplementName() {
  return "InteractiveDetector";
}

InteractiveDetector::InteractiveDetector(
    Document& document,
    std::unique_ptr<NetworkActivityChecker> network_activity_checker)
    : Supplement<Document>(document),
      ExecutionContextLifecycleObserver(document.GetExecutionContext()),
      clock_(base::DefaultTickClock::GetInstance()),
      network_activity_checker_(std::move(network_activity_checker)),
      time_to_interactive_timer_(
          document.GetTaskRunner(TaskType::kInternalDefault),
          this,
          &InteractiveDetector::TimeToInteractiveTimerFired),
      initially_hidden_(document.hidden()) {}

void InteractiveDetector::SetNavigationStartTime(
    base::TimeTicks navigation_start_time) {
  // Should not set nav start twice.
  DCHECK(page_event_times_.nav_start.is_null());

  // Don't record TTI for OOPIFs (yet).
  // TODO(crbug.com/808086): enable this case.
  if (!GetSupplementable()->IsInMainFrame())
    return;

  LongTaskDetector::Instance().RegisterObserver(this);
  page_event_times_.nav_start = navigation_start_time;
  base::TimeTicks initial_timer_fire_time =
      navigation_start_time + kTimeToInteractiveWindow;

  active_network_quiet_window_start_ = navigation_start_time;
  StartOrPostponeCITimer(initial_timer_fire_time);
}

int InteractiveDetector::NetworkActivityChecker::GetActiveConnections() {
  DCHECK(document_);
  ResourceFetcher* fetcher = document_->Fetcher();
  return fetcher->BlockingRequestCount() + fetcher->NonblockingRequestCount();
}

int InteractiveDetector::ActiveConnections() {
  return network_activity_checker_->GetActiveConnections();
}

void InteractiveDetector::StartOrPostponeCITimer(
    base::TimeTicks timer_fire_time) {
  // This function should never be called after Time To Interactive is
  // reached.
  DCHECK(interactive_time_.is_null());

  // We give 1ms extra padding to the timer fire time to prevent floating point
  // arithmetic pitfalls when comparing window sizes.
  timer_fire_time += base::Milliseconds(1);

  // Return if there is an active timer scheduled to fire later than
  // |timer_fire_time|.
  if (timer_fire_time < time_to_interactive_timer_fire_time_)
    return;

  base::TimeDelta delay = timer_fire_time - clock_->NowTicks();
  time_to_interactive_timer_fire_time_ = timer_fire_time;

  if (delay <= base::TimeDelta()) {
    // This argument of this function is never used and only there to fulfill
    // the API contract. nullptr should work fine.
    TimeToInteractiveTimerFired(nullptr);
  } else {
    time_to_interactive_timer_.StartOneShot(delay, FROM_HERE);
  }
}

std::optional<base::TimeDelta> InteractiveDetector::GetFirstInputDelay() const {
  return page_event_times_.first_input_delay;
}

WTF::Vector<std::optional<base::TimeDelta>>
InteractiveDetector::GetFirstInputDelaysAfterBackForwardCacheRestore() const {
  return page_event_times_.first_input_delays_after_back_forward_cache_restore;
}

std::optional<base::TimeTicks> InteractiveDetector::GetFirstInputTimestamp()
    const {
  return page_event_times_.first_input_timestamp;
}

std::optional<base::TimeTicks> InteractiveDetector::GetFirstScrollTimestamp()
    const {
  return page_event_times_.first_scroll_timestamp;
}

std::optional<base::TimeDelta> InteractiveDetector::GetFirstScrollDelay()
    const {
  return page_event_times_.frist_scroll_delay;
}

bool InteractiveDetector::PageWasBackgroundedSinceEvent(
    base::TimeTicks event_time) {
  DCHECK(GetSupplementable());
  if (GetSupplementable()->hidden()) {
    return true;
  }

  bool curr_hidden = initially_hidden_;
  base::TimeTicks visibility_start = page_event_times_.nav_start;
  for (auto change_event : visibility_change_events_) {
    base::TimeTicks visibility_end = change_event.timestamp;
    if (curr_hidden && event_time < visibility_end) {
      // [event_time, now] intersects a backgrounded range.
      return true;
    }
    curr_hidden = change_event.was_hidden;
    visibility_start = visibility_end;
  }

  return false;
}

void InteractiveDetector::HandleForInputDelay(
    const Event& event,
    base::TimeTicks event_platform_timestamp,
    base::TimeTicks processing_start) {
  DCHECK(event.isTrusted());
  DCHECK(event.type() == event_type_names::kPointerdown ||
         event.type() == event_type_names::kPointerup ||
         event.type() == event_type_names::kMousedown ||
         event.type() == event_type_names::kMouseup ||
         event.type() == event_type_names::kKeydown ||
         event.type() == event_type_names::kClick);

  // This only happens sometimes on tests unrelated to InteractiveDetector. It
  // is safe to ignore events that are not properly initialized.
  if (event_platform_timestamp.is_null())
    return;

  // These variables track the values which will be reported to histograms.
  base::TimeDelta delay;
  base::TimeTicks event_timestamp;

  // We can't report a pointerDown until the pointerUp, in case it turns into a
  // scroll.
  if (event.type() == event_type_names::kPointerdown) {
    pending_pointerdown_delay_ = processing_start - event_platform_timestamp;
    pending_pointerdown_timestamp_ = event_platform_timestamp;
    return;
  } else if (event.type() == event_type_names::kPointerup) {
    // PointerUp by itself is not considered a significant input.
    if (pending_pointerdown_timestamp_.is_null())
      return;

    // It is possible that this pointer up doesn't match with the pointer down
    // whose delay is stored in pending_pointerdown_delay_. In this case, the
    // user gesture started by this event contained some non-scroll input, so we
    // consider it reasonable to use the delay of the initial event.
    delay = pending_pointerdown_delay_;
    event_timestamp = pending_pointerdown_timestamp_;
  } else {
    if (event.type() == event_type_names::kMousedown) {
      pending_mousedown_delay_ = processing_start - event_platform_timestamp;
      pending_mousedown_timestamp_ = event_platform_timestamp;
      return;
    } else if (event.type() == event_type_names::kMouseup) {
      if (pending_mousedown_timestamp_.is_null())
        return;
      delay = pending_mousedown_delay_;
      event_timestamp = pending_mousedown_timestamp_;
      pending_mousedown_delay_ = base::TimeDelta();
      pending_mousedown_timestamp_ = base::TimeTicks();
    } else {
      // Record delays for click, keydown.
      delay = processing_start - event_platform_timestamp;
      event_timestamp = event_platform_timestamp;
    }
  }
  pending_pointerdown_delay_ = base::TimeDelta();
  pending_pointerdown_timestamp_ = base::TimeTicks();
  bool interactive_timing_metrics_changed = false;

  if (!page_event_times_.first_input_delay.has_value()) {
    page_event_times_.first_input_delay = delay;
    page_event_times_.first_input_timestamp = event_timestamp;
    interactive_timing_metrics_changed = true;

    if (delay > kFirstInputDelayTraceEventThreshold) {
      // Emit a trace event to highlight long first input delays.
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
          "latency", "Long First Input Delay",
          TRACE_ID_WITH_SCOPE("Long First Input Delay",
                              g_num_long_input_events),
          event_timestamp);
      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
          "latency", "Long First Input Delay",
          TRACE_ID_WITH_SCOPE("Long First Input Delay",
                              g_num_long_input_events),
          event_timestamp + delay);
      g_num_long_input_events++;
    }
  } else if (delay > kInputDelayTraceEventThreshold) {
    // Emit a trace event to highlight long input delays from second input and
    // onwards.
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "latency", "Long Input Delay",
        TRACE_ID_WITH_SCOPE("Long Input Delay", g_num_long_input_events),
        event_timestamp);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "latency", "Long Input Delay",
        TRACE_ID_WITH_SCOPE("Long Input Delay", g_num_long_input_events),
        event_timestamp + delay);
    // Apply metadata on stack samples.
    base::ApplyMetadataToPastSamples(
        event_timestamp, event_timestamp + delay,
        "PageLoad.InteractiveTiming.LongInputDelay", g_num_long_input_events, 1,
        base::SampleMetadataScope::kProcess);
    g_num_long_input_events++;
  }

  // Elements in |first_input_delays_after_back_forward_cache_restore| is
  // allocated when the page is restored from the back-forward cache. If the
  // last element exists and this is nullopt value, the first input has not come
  // yet after the last time when the page is restored from the cache.
  if (!page_event_times_.first_input_delays_after_back_forward_cache_restore
           .empty() &&
      !page_event_times_.first_input_delays_after_back_forward_cache_restore
           .back()
           .has_value()) {
    page_event_times_.first_input_delays_after_back_forward_cache_restore
        .back() = delay;
  }

  UMA_HISTOGRAM_CUSTOM_TIMES(kHistogramInputDelay, delay, base::Milliseconds(1),
                             base::Seconds(60), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(kHistogramInputTimestamp,
                             event_timestamp - page_event_times_.nav_start,
                             base::Milliseconds(10), base::Minutes(10), 100);


  if (GetSupplementable()->Loader() && interactive_timing_metrics_changed) {
    GetSupplementable()->Loader()->DidChangePerformanceTiming();
  }
}

void InteractiveDetector::BeginNetworkQuietPeriod(
    base::TimeTicks current_time) {
  // Value of 0.0 indicates there is no currently actively network quiet window.
  DCHECK(active_network_quiet_window_start_.is_null());
  active_network_quiet_window_start_ = current_time;

  StartOrPostponeCITimer(current_time + kTimeToInteractiveWindow);
}

void InteractiveDetector::EndNetworkQuietPeriod(base::TimeTicks current_time) {
  DCHECK(!active_network_quiet_window_start_.is_null());

  if (current_time - active_network_quiet_window_start_ >=
      kTimeToInteractiveWindow) {
    network_quiet_windows_.emplace_back(active_network_quiet_window_start_,
                                        current_time);
  }
  active_network_quiet_window_start_ = base::TimeTicks();
}

// The optional opt_current_time, if provided, saves us a call to
// clock_->NowTicks().
void InteractiveDetector::UpdateNetworkQuietState(
    double request_count,
    std::optional<base::TimeTicks> opt_current_time) {
  if (request_count <= kNetworkQuietMaximumConnections &&
      active_network_quiet_window_start_.is_null()) {
    // Not using `value_or(clock_->NowTicks())` here because arguments to
    // functions are eagerly evaluated, which always call clock_->NowTicks.
    base::TimeTicks current_time =
        opt_current_time ? opt_current_time.value() : clock_->NowTicks();
    BeginNetworkQuietPeriod(current_time);
  } else if (request_count > kNetworkQuietMaximumConnections &&
             !active_network_quiet_window_start_.is_null()) {
    base::TimeTicks current_time =
        opt_current_time ? opt_current_time.value() : clock_->NowTicks();
    EndNetworkQuietPeriod(current_time);
  }
}

void InteractiveDetector::OnResourceLoadBegin(
    std::optional<base::TimeTicks> load_begin_time) {
  if (!GetSupplementable())
    return;
  if (!interactive_time_.is_null())
    return;
  // The request that is about to begin is not counted in ActiveConnections(),
  // so we add one to it.
  UpdateNetworkQuietState(ActiveConnections() + 1, load_begin_time);
}

// The optional load_finish_time, if provided, saves us a call to
// clock_->NowTicks.
void InteractiveDetector::OnResourceLoadEnd(
    std::optional<base::TimeTicks> load_finish_time) {
  if (!GetSupplementable())
    return;
  if (!interactive_time_.is_null())
    return;
  UpdateNetworkQuietState(ActiveConnections(), load_finish_time);
}

void InteractiveDetector::OnLongTaskDetected(base::TimeTicks start_time,
                                             base::TimeTicks end_time) {
  // We should not be receiving long task notifications after Time to
  // Interactive has already been reached.
  DCHECK(interactive_time_.is_null());
  long_tasks_.emplace_back(start_time, end_time);
  StartOrPostponeCITimer(end_time + kTimeToInteractiveWindow);
}

void InteractiveDetector::OnFirstContentfulPaint(
    base::TimeTicks first_contentful_paint) {
  // TODO(yoav): figure out what we should do when FCP is set multiple times!
  page_event_times_.first_contentful_paint = first_contentful_paint;
  if (clock_->NowTicks() - first_contentful_paint >= kTimeToInteractiveWindow) {
    // We may have reached TTI already. Check right away.
    CheckTimeToInteractiveReached();
  } else {
    StartOrPostponeCITimer(page_event_times_.first_contentful_paint +
                           kTimeToInteractiveWindow);
  }
}

void InteractiveDetector::OnDomContentLoadedEnd(base::TimeTicks dcl_end_time) {
  // InteractiveDetector should only receive the first DCL event.
  DCHECK(page_event_times_.dom_content_loaded_end.is_null());
  page_event_times_.dom_content_loaded_end = dcl_end_time;
  CheckTimeToInteractiveReached();
}

void InteractiveDetector::OnInvalidatingInputEvent(
    base::TimeTicks invalidation_time) {
  if (!page_event_times_.first_invalidating_input.is_null())
    return;

  // In some edge cases (e.g. inaccurate input timestamp provided through remote
  // debugging protocol) we might receive an input timestamp that is earlier
  // than navigation start. Since invalidating input timestamp before navigation
  // start in non-sensical, we clamp it at navigation start.
  page_event_times_.first_invalidating_input =
      std::max(invalidation_time, page_event_times_.nav_start);

  if (GetSupplementable()->Loader())
    GetSupplementable()->Loader()->DidChangePerformanceTiming();
}

void InteractiveDetector::OnPageHiddenChanged(bool is_hidden) {
  visibility_change_events_.push_back(
      VisibilityChangeEvent{clock_->NowTicks(), is_hidden});
}

void InteractiveDetector::TimeToInteractiveTimerFired(TimerBase*) {
  if (!GetSupplementable() || !interactive_time_.is_null())
    return;

  // Value of 0.0 indicates there is currently no active timer.
  time_to_interactive_timer_fire_time_ = base::TimeTicks();
  CheckTimeToInteractiveReached();
}

void InteractiveDetector::AddCurrentlyActiveNetworkQuietInterval(
    base::TimeTicks current_time) {
  // Network is currently quiet.
  if (!active_network_quiet_window_start_.is_null()) {
    if (current_time - active_network_quiet_window_start_ >=
        kTimeToInteractiveWindow) {
      network_quiet_windows_.emplace_back(active_network_quiet_window_start_,
                                          current_time);
    }
  }
}

void InteractiveDetector::RemoveCurrentlyActiveNetworkQuietInterval() {
  if (!network_quiet_windows_.empty() &&
      network_quiet_windows_.back().Low() ==
          active_network_quiet_window_start_) {
    network_quiet_windows_.pop_back();
  }
}

base::TimeTicks InteractiveDetector::FindInteractiveCandidate(
    base::TimeTicks lower_bound,
    base::TimeTicks current_time) {
  // Network iterator.
  auto it_net = network_quiet_windows_.begin();
  // Long tasks iterator.
  auto it_lt = long_tasks_.begin();

  base::TimeTicks main_quiet_start = page_event_times_.nav_start;

  while (main_quiet_start < current_time &&
         it_net < network_quiet_windows_.end()) {
    base::TimeTicks main_quiet_end =
        it_lt == long_tasks_.end() ? current_time : it_lt->Low();
    base::TimeTicks next_main_quiet_start =
        it_lt == long_tasks_.end() ? current_time : it_lt->High();
    if (main_quiet_end - main_quiet_start < kTimeToInteractiveWindow) {
      // The main thread quiet window is too short.
      ++it_lt;
      main_quiet_start = next_main_quiet_start;
      continue;
    }
    if (main_quiet_end <= lower_bound) {
      // The main thread quiet window is before |lower_bound|.
      ++it_lt;
      main_quiet_start = next_main_quiet_start;
      continue;
    }
    if (it_net->High() <= lower_bound) {
      // The network quiet window is before |lower_bound|.
      ++it_net;
      continue;
    }

    // First handling the no overlap cases.
    // [ main thread interval ]
    //                                     [ network interval ]
    if (main_quiet_end <= it_net->Low()) {
      ++it_lt;
      main_quiet_start = next_main_quiet_start;
      continue;
    }
    //                                     [ main thread interval ]
    // [   network interval   ]
    if (it_net->High() <= main_quiet_start) {
      ++it_net;
      continue;
    }

    // At this point we know we have a non-empty overlap after lower_bound.
    base::TimeTicks overlap_start =
        std::max({main_quiet_start, it_net->Low(), lower_bound});
    base::TimeTicks overlap_end = std::min(main_quiet_end, it_net->High());
    base::TimeDelta overlap_duration = overlap_end - overlap_start;
    if (overlap_duration >= kTimeToInteractiveWindow) {
      return std::max(lower_bound, main_quiet_start);
    }

    // The interval with earlier end time will not produce any more overlap, so
    // we move on from it.
    if (main_quiet_end <= it_net->High()) {
      ++it_lt;
      main_quiet_start = next_main_quiet_start;
    } else {
      ++it_net;
    }
  }

  // Time To Interactive candidate not found.
  return base::TimeTicks();
}

void InteractiveDetector::CheckTimeToInteractiveReached() {
  // Already detected Time to Interactive.
  if (!interactive_time_.is_null())
    return;

  const bool ignore_fcp =
      base::FeatureList::IsEnabled(features::kInteractiveDetectorIgnoreFcp);
  // FCP and DCL have not been detected yet.
  if ((page_event_times_.first_contentful_paint.is_null() && !ignore_fcp) ||
      page_event_times_.dom_content_loaded_end.is_null()) {
    return;
  }

  const base::TimeTicks current_time = clock_->NowTicks();
  if (!ignore_fcp && (current_time - page_event_times_.first_contentful_paint <
                      kTimeToInteractiveWindow)) {
    // Too close to FCP to determine Time to Interactive.
    return;
  }

  AddCurrentlyActiveNetworkQuietInterval(current_time);
  base::TimeTicks interactive_candidate = FindInteractiveCandidate(
      page_event_times_.first_contentful_paint, current_time);
  RemoveCurrentlyActiveNetworkQuietInterval();

  // No Interactive Candidate found.
  if (interactive_candidate.is_null()) {
    if (ignore_fcp) {
      interactive_candidate = page_event_times_.dom_content_loaded_end;
    } else {
      return;
    }
  }

  interactive_time_ = std::max(
      {interactive_candidate, page_event_times_.dom_content_loaded_end});
  interactive_detection_time_ = clock_->NowTicks();
  OnTimeToInteractiveDetected();
}

void InteractiveDetector::OnTimeToInteractiveDetected() {
  LongTaskDetector::Instance().UnregisterObserver(this);
  network_quiet_windows_.clear();
  LocalFrame* frame = GetSupplementable()->GetFrame();
  DocumentLoader* loader = GetSupplementable()->Loader();
  probe::LifecycleEvent(frame, loader, "InteractiveTime",
                        base::TimeTicks::Now().since_origin().InSecondsF());

  TRACE_EVENT_MARK_WITH_TIMESTAMP2(
      "loading,rail", "InteractiveTime", interactive_time_, "frame",
      GetFrameIdForTracing(GetSupplementable()->GetFrame()), "args",
      [&](perfetto::TracedValue context) {
        // We log the trace event even if there is user input, but annotate the
        // event with whether that happened.
        bool had_user_input_before_interactive =
            !page_event_times_.first_invalidating_input.is_null() &&
            page_event_times_.first_invalidating_input < interactive_time_;

        auto dict = std::move(context).WriteDictionary();
        dict.Add("had_user_input_before_interactive",
                 had_user_input_before_interactive);
        dict.Add("total_blocking_time_ms",
                 ComputeTotalBlockingTime().InMillisecondsF());
      });

  long_tasks_.clear();

  if (frame != nullptr && frame->IsMainFrame() && frame->GetFrameScheduler()) {
    frame->GetFrameScheduler()->OnMainFrameInteractive();
  }
}

base::TimeDelta InteractiveDetector::ComputeTotalBlockingTime() {
  // We follow the same logic as the lighthouse computation in
  // https://github.com/GoogleChrome/lighthouse/blob/f150573b5970cc90c8d0c2214f5738df5cde8a31/lighthouse-core/computed/metrics/total-blocking-time.js#L60-L74.
  // In particular, tasks are clipped [FCP, TTI], and then all positive values
  // of (task_length - 50) are added to the blocking time.
  base::TimeDelta total_blocking_time;
  for (const auto& long_task : long_tasks_) {
    base::TimeTicks clipped_start =
        std::max(long_task.Low(), page_event_times_.first_contentful_paint);
    base::TimeTicks clipped_end = std::min(long_task.High(), interactive_time_);
    total_blocking_time +=
        std::max(base::TimeDelta(),
                 clipped_end - clipped_start - base::Milliseconds(50));
  }
  return total_blocking_time;
}

void InteractiveDetector::ContextDestroyed() {
  LongTaskDetector::Instance().UnregisterObserver(this);
}

void InteractiveDetector::Trace(Visitor* visitor) const {
  visitor->Trace(time_to_interactive_timer_);
  Supplement<Document>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void InteractiveDetector::SetTickClockForTesting(const base::TickClock* clock) {
  clock_ = clock;
}

void InteractiveDetector::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing) {
  time_to_interactive_timer_.MoveToNewTaskRunner(task_runner_for_testing);
}

void InteractiveDetector::RecordInputEventTimingUMA(
    base::TimeDelta processing_time,
    base::TimeDelta time_to_next_paint) {
  UmaHistogramCustomTimes(kHistogramProcessingTime, processing_time,
                          base::Milliseconds(1), base::Seconds(60), 50);
  UmaHistogramCustomTimes(kHistogramTimeToNextPaint, time_to_next_paint,
                          base::Milliseconds(1), base::Seconds(60), 50);
}

void InteractiveDetector::DidObserveFirstScrollDelay(
    base::TimeDelta first_scroll_delay,
    base::TimeTicks first_scroll_timestamp) {
  if (!page_event_times_.frist_scroll_delay.has_value()) {
    page_event_times_.frist_scroll_delay = first_scroll_delay;
    page_event_times_.first_scroll_timestamp = first_scroll_timestamp;
    if (GetSupplementable()->Loader()) {
      GetSupplementable()->Loader()->DidChangePerformanceTiming();
    }
  }
}

void InteractiveDetector::OnRestoredFromBackForwardCache() {
  // Allocate the last element with 0, which indicates that the first input
  // after this navigation doesn't happen yet.
  page_event_times_.first_input_delays_after_back_forward_cache_restore
      .push_back(std::nullopt);
}

}  // namespace blink
