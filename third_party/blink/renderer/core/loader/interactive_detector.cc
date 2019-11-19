// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/interactive_detector.h"

#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

// Required length of main thread and network quiet window for determining
// Time to Interactive.
constexpr auto kTimeToInteractiveWindow = base::TimeDelta::FromSeconds(5);
// Network is considered "quiet" if there are no more than 2 active network
// requests for this duration of time.
constexpr int kNetworkQuietMaximumConnections = 2;

const char kHistogramInputDelay[] = "PageLoad.InteractiveTiming.InputDelay3";
const char kHistogramInputTimestamp[] =
    "PageLoad.InteractiveTiming.InputTimestamp3";

// static
const char InteractiveDetector::kSupplementName[] = "InteractiveDetector";

InteractiveDetector* InteractiveDetector::From(Document& document) {
  InteractiveDetector* detector =
      Supplement<Document>::From<InteractiveDetector>(document);
  if (!detector) {
    detector = MakeGarbageCollected<InteractiveDetector>(
        document, new NetworkActivityChecker(&document));
    Supplement<Document>::ProvideTo(document, detector);
  }
  return detector;
}

const char* InteractiveDetector::SupplementName() {
  return "InteractiveDetector";
}

InteractiveDetector::InteractiveDetector(
    Document& document,
    NetworkActivityChecker* network_activity_checker)
    : Supplement<Document>(document),
      ContextLifecycleObserver(&document),
      clock_(base::DefaultTickClock::GetInstance()),
      network_activity_checker_(network_activity_checker),
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

  active_main_thread_quiet_window_start_ = navigation_start_time;
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
  timer_fire_time += base::TimeDelta::FromMilliseconds(1);

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

base::TimeTicks InteractiveDetector::GetInteractiveTime() const {
  // TODO(crbug.com/808685) Simplify FMP and TTI input invalidation.
  return page_event_times_.first_meaningful_paint_invalidated
             ? base::TimeTicks()
             : interactive_time_;
}

base::TimeTicks InteractiveDetector::GetInteractiveDetectionTime() const {
  // TODO(crbug.com/808685) Simplify FMP and TTI input invalidation.
  return page_event_times_.first_meaningful_paint_invalidated
             ? base::TimeTicks()
             : interactive_detection_time_;
}

base::TimeTicks InteractiveDetector::GetFirstInvalidatingInputTime() const {
  return page_event_times_.first_invalidating_input;
}

base::TimeDelta InteractiveDetector::GetFirstInputDelay() const {
  return page_event_times_.first_input_delay;
}

base::TimeTicks InteractiveDetector::GetFirstInputTimestamp() const {
  return page_event_times_.first_input_timestamp;
}

base::TimeDelta InteractiveDetector::GetLongestInputDelay() const {
  return page_event_times_.longest_input_delay;
}

base::TimeTicks InteractiveDetector::GetLongestInputTimestamp() const {
  return page_event_times_.longest_input_timestamp;
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
}  // namespace blink

void InteractiveDetector::HandleForInputDelay(
    const Event& event,
    base::TimeTicks event_platform_timestamp,
    base::TimeTicks processing_start) {
  DCHECK(event.isTrusted());

  // This only happens sometimes on tests unrelated to InteractiveDetector. It
  // is safe to ignore events that are not properly initialized.
  if (event_platform_timestamp.is_null())
    return;

  // We can't report a pointerDown until the pointerUp, in case it turns into a
  // scroll.
  if (event.type() == event_type_names::kPointerdown) {
    pending_pointerdown_delay_ = processing_start - event_platform_timestamp;
    pending_pointerdown_timestamp_ = event_platform_timestamp;
    return;
  }

  // We receive any event relevant for EventTiming, but we only care about
  // events relevant for FirstInputDelay.
  bool event_is_meaningful = event.type() == event_type_names::kPointerup ||
                             event.type() == event_type_names::kClick ||
                             event.type() == event_type_names::kKeydown ||
                             event.type() == event_type_names::kMousedown;

  if (!event_is_meaningful)
    return;

  // These variables track the values which will be reported to histograms.
  base::TimeDelta delay;
  base::TimeTicks event_timestamp;
  if (event.type() == event_type_names::kPointerup) {
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
    delay = processing_start - event_platform_timestamp;
    event_timestamp = event_platform_timestamp;
  }

  pending_pointerdown_delay_ = base::TimeDelta();
  pending_pointerdown_timestamp_ = base::TimeTicks();
  bool input_delay_metrics_changed = false;

  if (page_event_times_.first_input_delay.is_zero()) {
    page_event_times_.first_input_delay = delay;
    page_event_times_.first_input_timestamp = event_timestamp;
    input_delay_metrics_changed = true;
  }

  UMA_HISTOGRAM_CUSTOM_TIMES(kHistogramInputDelay, delay,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromSeconds(60), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(kHistogramInputTimestamp,
                             event_timestamp - page_event_times_.nav_start,
                             base::TimeDelta::FromMilliseconds(10),
                             base::TimeDelta::FromMinutes(10), 100);

  // Only update longest input delay if page was not backgrounded while the
  // input was queued.
  if (delay > page_event_times_.longest_input_delay &&
      !PageWasBackgroundedSinceEvent(event_timestamp)) {
    page_event_times_.longest_input_delay = delay;
    page_event_times_.longest_input_timestamp = event_timestamp;
    input_delay_metrics_changed = true;
  }

  if (GetSupplementable()->Loader() && input_delay_metrics_changed)
    GetSupplementable()->Loader()->DidChangePerformanceTiming();
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
    base::Optional<base::TimeTicks> opt_current_time) {
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
    base::Optional<base::TimeTicks> load_begin_time) {
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
    base::Optional<base::TimeTicks> load_finish_time) {
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
  base::TimeDelta quiet_window_length =
      start_time - active_main_thread_quiet_window_start_;
  if (quiet_window_length >= kTimeToInteractiveWindow) {
    main_thread_quiet_windows_.emplace_back(
        active_main_thread_quiet_window_start_, start_time);
  }
  active_main_thread_quiet_window_start_ = end_time;
  StartOrPostponeCITimer(end_time + kTimeToInteractiveWindow);
}

void InteractiveDetector::OnFirstMeaningfulPaintDetected(
    base::TimeTicks fmp_time,
    FirstMeaningfulPaintDetector::HadUserInput user_input_before_fmp) {
  DCHECK(page_event_times_.first_meaningful_paint
             .is_null());  // Should not set FMP twice.
  page_event_times_.first_meaningful_paint = fmp_time;
  page_event_times_.first_meaningful_paint_invalidated =
      user_input_before_fmp == FirstMeaningfulPaintDetector::kHadUserInput;
  if (clock_->NowTicks() - fmp_time >= kTimeToInteractiveWindow) {
    // We may have reached TTCI already. Check right away.
    CheckTimeToInteractiveReached();
  } else {
    StartOrPostponeCITimer(page_event_times_.first_meaningful_paint +
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

void InteractiveDetector::AddCurrentlyActiveQuietIntervals(
    base::TimeTicks current_time) {
  // Network is currently quiet.
  if (!active_network_quiet_window_start_.is_null()) {
    if (current_time - active_network_quiet_window_start_ >=
        kTimeToInteractiveWindow) {
      network_quiet_windows_.emplace_back(active_network_quiet_window_start_,
                                          current_time);
    }
  }

  // Since this code executes on the main thread, we know that no task is
  // currently running on the main thread. We can therefore skip checking.
  // main_thread_quiet_window_being != 0.0.
  if (current_time - active_main_thread_quiet_window_start_ >=
      kTimeToInteractiveWindow) {
    main_thread_quiet_windows_.emplace_back(
        active_main_thread_quiet_window_start_, current_time);
  }
}

void InteractiveDetector::RemoveCurrentlyActiveQuietIntervals() {
  if (!network_quiet_windows_.IsEmpty() &&
      network_quiet_windows_.back().Low() ==
          active_network_quiet_window_start_) {
    network_quiet_windows_.pop_back();
  }

  if (!main_thread_quiet_windows_.IsEmpty() &&
      main_thread_quiet_windows_.back().Low() ==
          active_main_thread_quiet_window_start_) {
    main_thread_quiet_windows_.pop_back();
  }
}

base::TimeTicks InteractiveDetector::FindInteractiveCandidate(
    base::TimeTicks lower_bound) {
  // Main thread iterator.
  auto* it_mt = main_thread_quiet_windows_.begin();
  // Network iterator.
  auto* it_net = network_quiet_windows_.begin();

  while (it_mt < main_thread_quiet_windows_.end() &&
         it_net < network_quiet_windows_.end()) {
    if (it_mt->High() <= lower_bound) {
      it_mt++;
      continue;
    }
    if (it_net->High() <= lower_bound) {
      it_net++;
      continue;
    }

    // First handling the no overlap cases.
    // [ main thread interval ]
    //                                     [ network interval ]
    if (it_mt->High() <= it_net->Low()) {
      it_mt++;
      continue;
    }
    //                                     [ main thread interval ]
    // [   network interval   ]
    if (it_net->High() <= it_mt->Low()) {
      it_net++;
      continue;
    }

    // At this point we know we have a non-empty overlap after lower_bound.
    base::TimeTicks overlap_start =
        std::max({it_mt->Low(), it_net->Low(), lower_bound});
    base::TimeTicks overlap_end = std::min(it_mt->High(), it_net->High());
    base::TimeDelta overlap_duration = overlap_end - overlap_start;
    if (overlap_duration >= kTimeToInteractiveWindow) {
      return std::max(lower_bound, it_mt->Low());
    }

    // The interval with earlier end time will not produce any more overlap, so
    // we move on from it.
    if (it_mt->High() <= it_net->High()) {
      it_mt++;
    } else {
      it_net++;
    }
  }

  // Time To Interactive candidate not found.
  return base::TimeTicks();
}

void InteractiveDetector::CheckTimeToInteractiveReached() {
  // Already detected Time to Interactive.
  if (!interactive_time_.is_null())
    return;

  // FMP and DCL have not been detected yet.
  if (page_event_times_.first_meaningful_paint.is_null() ||
      page_event_times_.dom_content_loaded_end.is_null())
    return;

  const base::TimeTicks current_time = clock_->NowTicks();
  if (current_time - page_event_times_.first_meaningful_paint <
      kTimeToInteractiveWindow) {
    // Too close to FMP to determine Time to Interactive.
    return;
  }

  AddCurrentlyActiveQuietIntervals(current_time);
  const base::TimeTicks interactive_candidate =
      FindInteractiveCandidate(page_event_times_.first_meaningful_paint);
  RemoveCurrentlyActiveQuietIntervals();

  // No Interactive Candidate found.
  if (interactive_candidate.is_null())
    return;

  interactive_time_ = std::max(
      {interactive_candidate, page_event_times_.dom_content_loaded_end});
  interactive_detection_time_ = clock_->NowTicks();
  OnTimeToInteractiveDetected();
}

void InteractiveDetector::OnTimeToInteractiveDetected() {
  LongTaskDetector::Instance().UnregisterObserver(this);
  main_thread_quiet_windows_.clear();
  network_quiet_windows_.clear();

  bool had_user_input_before_interactive =
      !page_event_times_.first_invalidating_input.is_null() &&
      page_event_times_.first_invalidating_input < interactive_time_;

  // We log the trace event even if there is user input, but annotate the event
  // with whether that happened.
  TRACE_EVENT_MARK_WITH_TIMESTAMP2(
      "loading,rail", "InteractiveTime", interactive_time_, "frame",
      ToTraceValue(GetSupplementable()->GetFrame()),
      "had_user_input_before_interactive", had_user_input_before_interactive);

  // We only send TTI to Performance Timing Observers if FMP was not invalidated
  // by input.
  // TODO(crbug.com/808685) Simplify FMP and TTI input invalidation.
  if (!page_event_times_.first_meaningful_paint_invalidated) {
    if (GetSupplementable()->Loader())
      GetSupplementable()->Loader()->DidChangePerformanceTiming();
  }
}

void InteractiveDetector::ContextDestroyed(ExecutionContext*) {
  LongTaskDetector::Instance().UnregisterObserver(this);
}

void InteractiveDetector::Trace(Visitor* visitor) {
  Supplement<Document>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void InteractiveDetector::SetTickClockForTesting(const base::TickClock* clock) {
  clock_ = clock;
}

void InteractiveDetector::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing) {
  time_to_interactive_timer_.MoveToNewTaskRunner(task_runner_for_testing);
}

}  // namespace blink
