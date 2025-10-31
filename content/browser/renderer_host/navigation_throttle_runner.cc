// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_throttle_runner.h"

#include "base/check_deref.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {

namespace {

BASE_FEATURE(kReportStuckThrottle, base::FEATURE_DISABLED_BY_DEFAULT);

NavigationThrottle::ThrottleCheckResult ExecuteNavigationEvent(
    NavigationThrottle* throttle,
    NavigationThrottleEvent event) {
  switch (event) {
    case NavigationThrottleEvent::kNoEvent:
      DUMP_WILL_BE_NOTREACHED();
      return NavigationThrottle::CANCEL_AND_IGNORE;
    case NavigationThrottleEvent::kWillStartRequest:
      return throttle->WillStartRequest();
    case NavigationThrottleEvent::kWillRedirectRequest:
      return throttle->WillRedirectRequest();
    case NavigationThrottleEvent::kWillFailRequest:
      return throttle->WillFailRequest();
    case NavigationThrottleEvent::kWillProcessResponse:
      return throttle->WillProcessResponse();
    case NavigationThrottleEvent::kWillCommitWithoutUrlLoader:
      return throttle->WillCommitWithoutUrlLoader();
  }
  NOTREACHED();
}

perfetto::StaticString GetEventName(NavigationThrottleEvent event) {
  switch (event) {
    case NavigationThrottleEvent::kNoEvent:
      DUMP_WILL_BE_NOTREACHED();
      return "";
    case NavigationThrottleEvent::kWillStartRequest:
      return "NavigationThrottle::WillStartRequest";
    case NavigationThrottleEvent::kWillRedirectRequest:
      return "NavigationThrottle::WillRedirectRequest";
    case NavigationThrottleEvent::kWillFailRequest:
      return "NavigationThrottle::WillFailRequest";
    case NavigationThrottleEvent::kWillProcessResponse:
      return "NavigationThrottle::WillProcessResponse";
    case NavigationThrottleEvent::kWillCommitWithoutUrlLoader:
      return "NavigationThrottle::WillCommitWithoutUrlLoader";
  }
  NOTREACHED();
}

const char* GetEventNameForHistogram(NavigationThrottleEvent event) {
  switch (event) {
    case NavigationThrottleEvent::kNoEvent:
      DUMP_WILL_BE_NOTREACHED();
      return "";
    case NavigationThrottleEvent::kWillStartRequest:
      return "WillStartRequest";
    case NavigationThrottleEvent::kWillRedirectRequest:
      return "WillRedirectRequest";
    case NavigationThrottleEvent::kWillFailRequest:
      return "WillFailRequest";
    case NavigationThrottleEvent::kWillProcessResponse:
      return "WillProcessResponse";
    case NavigationThrottleEvent::kWillCommitWithoutUrlLoader:
      return "WillCommitWithoutUrlLoader";
  }
  NOTREACHED();
}

base::TimeDelta RecordHistogram(NavigationThrottleEvent event,
                                base::Time start,
                                const std::string& metric_type) {
  base::TimeDelta delta = base::Time::Now() - start;
  base::UmaHistogramTimes(base::StrCat({"Navigation.Throttle", metric_type, ".",
                                        GetEventNameForHistogram(event)}),
                          delta);
  return delta;
}

base::TimeDelta RecordDeferTimeHistogram(NavigationThrottleEvent event,
                                         base::Time start) {
  return RecordHistogram(event, start, "DeferTime");
}

void RecordExecutionTimeHistogram(NavigationThrottleEvent event,
                                  base::Time start) {
  RecordHistogram(event, start, "ExecutionTime");
}

}  // namespace

NavigationThrottleRunner::NavigationThrottleRunner(
    NavigationThrottleRegistryBase* registry,
    int64_t navigation_id,
    bool is_primary_main_frame)
    : registry_(CHECK_DEREF(registry)),
      navigation_id_(navigation_id),
      is_primary_main_frame_(is_primary_main_frame) {}

NavigationThrottleRunner::~NavigationThrottleRunner() {
  base::UmaHistogramTimes("Navigation.ThrottleTotalDeferTime",
                          total_defer_duration_time_);
  base::UmaHistogramCounts100("Navigation.ThrottleTotalDeferCount",
                              defer_count_);
  base::UmaHistogramTimes("Navigation.ThrottleTotalDeferTime.Request",
                          total_defer_duration_time_for_request_);
  base::UmaHistogramCounts100("Navigation.ThrottleTotalDeferCount.Request",
                              defer_count_for_request_);
}

void NavigationThrottleRunner::ProcessNavigationEvent(
    NavigationThrottleEvent event) {
  CHECK_NE(NavigationThrottleEvent::kNoEvent, event);
  current_event_ = event;
  next_index_ = 0;
  ProcessInternal();
}

void NavigationThrottleRunner::ResumeProcessingNavigationEvent(
    NavigationThrottle* deferring_throttle) {
  base::TimeDelta defer_time =
      RecordDeferTimeHistogram(current_event_, defer_start_time_);
  total_defer_duration_time_ += defer_time;
  defer_count_++;
  if (current_event_ == NavigationThrottleEvent::kWillStartRequest ||
      current_event_ == NavigationThrottleEvent::kWillRedirectRequest) {
    total_defer_duration_time_for_request_ += defer_time;
    defer_count_for_request_++;
  }
  base::UmaHistogramEnumeration("Navigation.ThrottleDeferredEvent",
                                current_event_);
  report_stuck_throttle_timer_ = nullptr;
  RecordDeferTimeUKM();
  ProcessInternal();
}

void NavigationThrottleRunner::ProcessInternal() {
  TRACE_EVENT0("navigation", "NavigationThrottleRunner::ProcessInternal");
  CHECK_NE(NavigationThrottleEvent::kNoEvent, current_event_);
  base::Time start_time = base::Time::Now();
  if (!event_process_start_time_.has_value()) {
    event_process_start_time_ = start_time;
    event_process_execution_time_ = base::TimeDelta();
  }
  base::WeakPtr<NavigationThrottleRunner> weak_ref = weak_factory_.GetWeakPtr();

  // Capture into a local variable the |navigation_id_| value, since this
  // object can be freed by any of the throttles being invoked and the trace
  // events need to be able to use the navigation id safely in such a case.
  int64_t local_navigation_id = navigation_id_;

  auto& throttles = registry_->GetThrottles();
  for (size_t i = next_index_; i < throttles.size(); ++i) {
    TRACE_EVENT0("navigation",
                 "NavigationThrottleRunner::ProcessInternal.loop");
    TRACE_EVENT_BEGIN("navigation", GetEventName(current_event_),
                      perfetto::Track::Global(local_navigation_id), "throttle",
                      throttles[i]->GetNameForLogging());

    base::Time start = base::Time::Now();
    NavigationThrottle::ThrottleCheckResult result =
        ExecuteNavigationEvent(throttles[i].get(), current_event_);
    if (!weak_ref) {
      // The NavigationThrottle execution has destroyed this
      // NavigationThrottleRunner. Return immediately.
      // GetEventName(current_event_)
      TRACE_EVENT_END("navigation",
                      perfetto::Track::Global(local_navigation_id), "result",
                      "deleted");
      return;
    }
    RecordExecutionTimeHistogram(current_event_, start);
    // GetEventName(current_event_)
    TRACE_EVENT_END("navigation", perfetto::Track::Global(local_navigation_id),
                    "result", result.action());

    switch (result.action()) {
      case NavigationThrottle::PROCEED:
        continue;

      case NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE:
      case NavigationThrottle::BLOCK_REQUEST:
      case NavigationThrottle::BLOCK_RESPONSE:
      case NavigationThrottle::CANCEL:
      case NavigationThrottle::CANCEL_AND_IGNORE:
        next_index_ = 0;
        event_process_start_time_.reset();
        InformRegistry(result);
        return;

      case NavigationThrottle::DEFER:
        registry_->OnDeferProcessingNavigationEvent(throttles[i].get());
        next_index_ = i + 1;
        defer_start_time_ = base::Time::Now();
        event_process_execution_time_ += base::Time::Now() - start_time;
        if (base::FeatureList::IsEnabled(kReportStuckThrottle)) {
          report_stuck_throttle_timer_ = std::make_unique<base::OneShotTimer>();
          report_stuck_throttle_timer_->Start(
              FROM_HERE, base::Seconds(30),
              base::BindOnce(&NavigationThrottleRunner::ReportStuckThrottle,
                             weak_factory_.GetWeakPtr()));
        }
        return;
    }
  }

  base::Time end_time = base::Time::Now();
  event_process_execution_time_ += end_time - start_time;
  base::UmaHistogramTimes(
      base::StrCat({"Navigation.ThrottleEventExecutionTime.",
                    GetEventNameForHistogram(current_event_)}),
      event_process_execution_time_);
  base::UmaHistogramTimes(
      base::StrCat({"Navigation.ThrottleEventDurationTime.",
                    GetEventNameForHistogram(current_event_)}),
      end_time - *event_process_start_time_);
  event_process_start_time_.reset();
  next_index_ = 0;
  InformRegistry(NavigationThrottle::PROCEED);
}

void NavigationThrottleRunner::InformRegistry(
    const NavigationThrottle::ThrottleCheckResult& result) {
  // Now that the event has executed, reset the current event to kNoEvent since
  // we're no longer processing any event. Do it before the call to the
  // delegate, as it might lead to the deletion of this
  // NavigationThrottleRunner.
  NavigationThrottleEvent event = current_event_;
  current_event_ = NavigationThrottleEvent::kNoEvent;
  registry_->OnEventProcessed(event, result);
  // DO NOT ADD CODE AFTER THIS. The NavigationThrottleRunner might have been
  // deleted by the previous call.
}

void NavigationThrottleRunner::RecordDeferTimeUKM() {
  if (!is_primary_main_frame_ || registry_->GetDeferringThrottles().empty()) {
    return;
  }
  ukm::builders::NavigationThrottleDeferredTime builder(
      ukm::ConvertToSourceId(navigation_id_, ukm::SourceIdType::NAVIGATION_ID));
  builder.SetDurationOfNavigationDeferralMs(
      (base::Time::Now() - defer_start_time_).InMilliseconds());
  builder.SetNavigationThrottleEventType(static_cast<int64_t>(current_event_));
  // The logging name is converted to an MD5 int64_t hash which is recorded in
  // UKM. The possible values are sparse, and analyses should hash the values
  // returned by NavigationThrottle::GetNameForLogging to determine which
  // throttle deferred the navigation.
  builder.SetNavigationThrottleNameHash(base::HashMetricName(
      (*registry_->GetDeferringThrottles().cbegin())->GetNameForLogging()));
  builder.Record(ukm::UkmRecorder::Get());
}

void NavigationThrottleRunner::ReportStuckThrottle() {
  CHECK(next_index_ > 0);
  std::unique_ptr<NavigationThrottle>& running_throttle =
      registry_->GetThrottles()[next_index_ - 1];
  ;
  SCOPED_CRASH_KEY_STRING32("Bug", "stuck_throttle_name",
                            running_throttle->GetNameForLogging());
  base::debug::DumpWithoutCrashing();
}

}  // namespace content
