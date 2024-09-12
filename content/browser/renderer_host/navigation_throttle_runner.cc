// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_throttle_runner.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/preloading/prefetch/contamination_delay_navigation_throttle.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prerender/prerender_navigation_throttle.h"
#include "content/browser/preloading/prerender/prerender_subframe_navigation_throttle.h"
#include "content/browser/renderer_host/ancestor_throttle.h"
#include "content/browser/renderer_host/back_forward_cache_subframe_navigation_throttle.h"
#include "content/browser/renderer_host/blocked_scheme_navigation_throttle.h"
#include "content/browser/renderer_host/http_error_navigation_throttle.h"
#include "content/browser/renderer_host/isolated_web_app_throttle.h"
#include "content/browser/renderer_host/mixed_content_navigation_throttle.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/partitioned_popins/partitioned_popins_navigation_throttle.h"
#include "content/browser/renderer_host/renderer_cancellation_throttle.h"
#include "content/browser/renderer_host/subframe_history_navigation_throttle.h"
#include "content/public/browser/navigation_handle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/browser/picture_in_picture/document_picture_in_picture_navigation_throttle.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace content {

namespace {

NavigationThrottle::ThrottleCheckResult ExecuteNavigationEvent(
    NavigationThrottle* throttle,
    NavigationThrottleRunner::Event event) {
  switch (event) {
    case NavigationThrottleRunner::Event::kNoEvent:
      DUMP_WILL_BE_NOTREACHED();
      return NavigationThrottle::CANCEL_AND_IGNORE;
    case NavigationThrottleRunner::Event::kWillStartRequest:
      return throttle->WillStartRequest();
    case NavigationThrottleRunner::Event::kWillRedirectRequest:
      return throttle->WillRedirectRequest();
    case NavigationThrottleRunner::Event::kWillFailRequest:
      return throttle->WillFailRequest();
    case NavigationThrottleRunner::Event::kWillProcessResponse:
      return throttle->WillProcessResponse();
    case NavigationThrottleRunner::Event::kWillCommitWithoutUrlLoader:
      return throttle->WillCommitWithoutUrlLoader();
  }
  NOTREACHED_IN_MIGRATION();
  return NavigationThrottle::CANCEL_AND_IGNORE;
}

const char* GetEventName(NavigationThrottleRunner::Event event) {
  switch (event) {
    case NavigationThrottleRunner::Event::kNoEvent:
      DUMP_WILL_BE_NOTREACHED();
      return "";
    case NavigationThrottleRunner::Event::kWillStartRequest:
      return "NavigationThrottle::WillStartRequest";
    case NavigationThrottleRunner::Event::kWillRedirectRequest:
      return "NavigationThrottle::WillRedirectRequest";
    case NavigationThrottleRunner::Event::kWillFailRequest:
      return "NavigationThrottle::WillFailRequest";
    case NavigationThrottleRunner::Event::kWillProcessResponse:
      return "NavigationThrottle::WillProcessResponse";
    case NavigationThrottleRunner::Event::kWillCommitWithoutUrlLoader:
      return "NavigationThrottle::WillCommitWithoutUrlLoader";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* GetEventNameForHistogram(NavigationThrottleRunner::Event event) {
  switch (event) {
    case NavigationThrottleRunner::Event::kNoEvent:
      DUMP_WILL_BE_NOTREACHED();
      return "";
    case NavigationThrottleRunner::Event::kWillStartRequest:
      return "WillStartRequest";
    case NavigationThrottleRunner::Event::kWillRedirectRequest:
      return "WillRedirectRequest";
    case NavigationThrottleRunner::Event::kWillFailRequest:
      return "WillFailRequest";
    case NavigationThrottleRunner::Event::kWillProcessResponse:
      return "WillProcessResponse";
    case NavigationThrottleRunner::Event::kWillCommitWithoutUrlLoader:
      return "WillCommitWithoutUrlLoader";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

base::TimeDelta RecordHistogram(NavigationThrottleRunner::Event event,
                                base::Time start,
                                const std::string& metric_type) {
  base::TimeDelta delta = base::Time::Now() - start;
  base::UmaHistogramTimes(base::StrCat({"Navigation.Throttle", metric_type, ".",
                                        GetEventNameForHistogram(event)}),
                          delta);
  return delta;
}

base::TimeDelta RecordDeferTimeHistogram(NavigationThrottleRunner::Event event,
                                         base::Time start) {
  return RecordHistogram(event, start, "DeferTime");
}

void RecordExecutionTimeHistogram(NavigationThrottleRunner::Event event,
                                  base::Time start) {
  RecordHistogram(event, start, "ExecutionTime");
}

}  // namespace

NavigationThrottleRunner::NavigationThrottleRunner(Delegate* delegate,
                                                   int64_t navigation_id,
                                                   bool is_primary_main_frame)
    : delegate_(delegate),
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

void NavigationThrottleRunner::ProcessNavigationEvent(Event event) {
  DCHECK_NE(Event::kNoEvent, event);
  current_event_ = event;
  next_index_ = 0;
  ProcessInternal();
}

void NavigationThrottleRunner::ResumeProcessingNavigationEvent(
    NavigationThrottle* deferring_throttle) {
  DCHECK_EQ(GetDeferringThrottle(), deferring_throttle);
  base::TimeDelta defer_time =
      RecordDeferTimeHistogram(current_event_, defer_start_time_);
  total_defer_duration_time_ += defer_time;
  defer_count_++;
  if (current_event_ == Event::kWillStartRequest ||
      current_event_ == Event::kWillRedirectRequest) {
    total_defer_duration_time_for_request_ += defer_time;
    defer_count_for_request_++;
  }
  base::UmaHistogramEnumeration("Navigation.ThrottleDeferredEvent",
                                current_event_);
  RecordDeferTimeUKM();
  ProcessInternal();
}

void NavigationThrottleRunner::CallResumeForTesting() {
  RecordDeferTimeUKM();
  ProcessInternal();
}

void NavigationThrottleRunner::RegisterNavigationThrottles() {
  TRACE_EVENT0("navigation",
               "NavigationThrottleRunner::RegisterNavigationThrottles");
  // Note: |throttle_| might not be empty. Some NavigationThrottles might have
  // been registered with RegisterThrottleForTesting. These must reside at the
  // end of |throttles_|. TestNavigationManagerThrottle expects that the
  // NavigationThrottles added for test are the last NavigationThrottles to
  // execute. Take them out while appending the rest of the
  // NavigationThrottles.
  std::vector<std::unique_ptr<NavigationThrottle>> testing_throttles =
      std::move(throttles_);

  // The NavigationRequest associated with the NavigationThrottles this
  // NavigationThrottleRunner manages.
  // Unit tests that do not use NavigationRequest should never call
  // RegisterNavigationThrottles as this function expects |delegate_| to be a
  // NavigationRequest.
  NavigationRequest* request = static_cast<NavigationRequest*>(delegate_);

  throttles_ = request->GetDelegate()->CreateThrottlesForNavigation(request);

  // Check for renderer-inititated main frame navigations to blocked URL schemes
  // (data, filesystem). This is done early as it may block the main frame
  // navigation altogether.
  AddThrottle(
      BlockedSchemeNavigationThrottle::CreateThrottleForNavigation(request));

#if !BUILDFLAG(IS_ANDROID)
  // Prevent cross-document navigations from document picture-in-picture
  // windows.
  AddThrottle(
      DocumentPictureInPictureNavigationThrottle::MaybeCreateThrottleFor(
          request));
#endif  // !BUILDFLAG(IS_ANDROID)

  AddThrottle(AncestorThrottle::MaybeCreateThrottleFor(request));

  // Check for mixed content. This is done after the AncestorThrottle and the
  // FormSubmissionThrottle so that when folks block mixed content with a CSP
  // policy, they don't get a warning. They'll still get a warning in the
  // console about CSP blocking the load.
  AddThrottle(
      MixedContentNavigationThrottle::CreateThrottleForNavigation(request));

  if (base::FeatureList::IsEnabled(
          features::kPrefetchStateContaminationMitigation)) {
    // Delay response processing for certain prefetch responses where it might
    // otherwise reveal information about cross-site state.
    AddThrottle(
        std::make_unique<ContaminationDelayNavigationThrottle>(request));
  }

  // Block certain requests that are not permitted for prerendering.
  AddThrottle(PrerenderNavigationThrottle::MaybeCreateThrottleFor(request));

  // Defer cross-origin subframe loading during prerendering state.
  AddThrottle(
      PrerenderSubframeNavigationThrottle::MaybeCreateThrottleFor(request));

  // Prevent navigations to/from Isolated Web Apps.
  AddThrottle(IsolatedWebAppThrottle::MaybeCreateThrottleFor(request));

  for (auto& throttle :
       devtools_instrumentation::CreateNavigationThrottles(request)) {
    AddThrottle(std::move(throttle));
  }

  // Make main frame navigations with error HTTP status code and an empty body
  // commit an error page instead. Note that this should take lower priority
  // than other throttles that might care about those navigations, e.g.
  // throttles handling pages with 407 errors that require extra authentication.
  AddThrottle(HttpErrorNavigationThrottle::MaybeCreateThrottleFor(*request));

  // Wait for renderer-initiated navigation cancelation window to end. This will
  // wait for the JS task that starts the navigation to finish, so add it close
  // to the end to not delay running other throttles.
  AddThrottle(RendererCancellationThrottle::MaybeCreateThrottleFor(request));

  // Defer any cross-document subframe history navigations if there is an
  // associated main-frame same-document history navigation in progress, until
  // the main frame has had an opportunity to fire a navigate event in the
  // renderer. If the navigate event cancels the history navigation, the
  // subframe navigations should not proceed.
  AddThrottle(
      SubframeHistoryNavigationThrottle::MaybeCreateThrottleFor(request));

  // Defer subframe navigation in bfcached page if it hasn't sent a network
  // request.
  // This must be the last throttle to run. See https://crrev.com/c/5316738.
  if (base::FeatureList::IsEnabled(
          features::kEnableBackForwardCacheForOngoingSubframeNavigation)) {
    AddThrottle(
        BackForwardCacheSubframeNavigationThrottle::MaybeCreateThrottleFor(
            request));
  }

  // Add a throttle to manage top-frame navigations from a partitioned popin.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  AddThrottle(
      PartitionedPopinsNavigationThrottle::MaybeCreateThrottleFor(request));
  // DO NOT ADD any throttles after this line.

  // Insert all testing NavigationThrottles last.
  throttles_.insert(throttles_.end(),
                    std::make_move_iterator(testing_throttles.begin()),
                    std::make_move_iterator(testing_throttles.end()));

  base::UmaHistogramCounts100("Navigation.ThrottleCount", throttles_.size());
}

void NavigationThrottleRunner::
    RegisterNavigationThrottlesForCommitWithoutUrlLoader() {
  // Note: |throttle_| might not be empty. Some NavigationThrottles might have
  // been registered with RegisterThrottleForTesting. These must reside at the
  // end of |throttles_|. TestNavigationManagerThrottle expects that the
  // NavigationThrottles added for test are the last NavigationThrottles to
  // execute. Take them out while appending the rest of the
  // NavigationThrottles.
  std::vector<std::unique_ptr<NavigationThrottle>> testing_throttles =
      std::move(throttles_);

  // The NavigationRequest associated with the NavigationThrottles this
  // NavigationThrottleRunner manages.
  // Unit tests that do not use NavigationRequest should never call
  // RegisterNavigationThrottlesForCommitWithoutUrlLoader as this function
  // expects |delegate_| to be a NavigationRequest.
  NavigationRequest* request = static_cast<NavigationRequest*>(delegate_);

  // Defer any same-document subframe history navigations if there is an
  // associated main-frame same-document history navigation in progress, until
  // the main frame has had an opportunity to fire a navigate event in the
  // renderer. If the navigate event cancels the history navigation, the
  // subframe navigations should not proceed.
  AddThrottle(
      SubframeHistoryNavigationThrottle::MaybeCreateThrottleFor(request));

  // Defer cross-origin about:srcdoc subframe loading during prerendering state.
  AddThrottle(
      PrerenderSubframeNavigationThrottle::MaybeCreateThrottleFor(request));

  // Defer subframe navigation in bfcached page.
  if (base::FeatureList::IsEnabled(
          features::kEnableBackForwardCacheForOngoingSubframeNavigation)) {
    AddThrottle(
        BackForwardCacheSubframeNavigationThrottle::MaybeCreateThrottleFor(
            request));
  }

  AddThrottle(RendererCancellationThrottle::MaybeCreateThrottleFor(request));

  // Insert all testing NavigationThrottles last.
  throttles_.insert(throttles_.end(),
                    std::make_move_iterator(testing_throttles.begin()),
                    std::make_move_iterator(testing_throttles.end()));
}

NavigationThrottle* NavigationThrottleRunner::GetDeferringThrottle() const {
  if (next_index_ == 0) {
    return nullptr;
  }
  return throttles_[next_index_ - 1].get();
}

void NavigationThrottleRunner::AddThrottle(
    std::unique_ptr<NavigationThrottle> navigation_throttle) {
  if (navigation_throttle) {
    TRACE_EVENT1("navigation", "NavigationThrottleRunner::AddThrottle",
                 "navigation_throttle",
                 navigation_throttle->GetNameForLogging());
    throttles_.push_back(std::move(navigation_throttle));
  }
}

void NavigationThrottleRunner::ProcessInternal() {
  TRACE_EVENT0("navigation", "NavigationThrottleRunner::ProcessInternal");
  DCHECK_NE(Event::kNoEvent, current_event_);
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

  for (size_t i = next_index_; i < throttles_.size(); ++i) {
    TRACE_EVENT0("navigation",
                 "NavigationThrottleRunner::ProcessInternal.loop");
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        "navigation", GetEventName(current_event_), local_navigation_id,
        "throttle", throttles_[i]->GetNameForLogging());

    base::Time start = base::Time::Now();
    NavigationThrottle::ThrottleCheckResult result =
        ExecuteNavigationEvent(throttles_[i].get(), current_event_);
    if (!weak_ref) {
      // The NavigationThrottle execution has destroyed this
      // NavigationThrottleRunner. Return immediately.
      TRACE_EVENT_NESTABLE_ASYNC_END1("navigation", "", local_navigation_id,
                                      "result", "deleted");
      return;
    }
    RecordExecutionTimeHistogram(current_event_, start);
    TRACE_EVENT_NESTABLE_ASYNC_END1("navigation", GetEventName(current_event_),
                                    local_navigation_id, "result",
                                    result.action());

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
        InformDelegate(result);
        return;

      case NavigationThrottle::DEFER:
        next_index_ = i + 1;
        defer_start_time_ = base::Time::Now();
        if (first_deferral_callback_for_testing_) {
          std::move(first_deferral_callback_for_testing_).Run();
        }
        event_process_execution_time_ += base::Time::Now() - start_time;
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
  InformDelegate(NavigationThrottle::PROCEED);
}

void NavigationThrottleRunner::InformDelegate(
    const NavigationThrottle::ThrottleCheckResult& result) {
  // Now that the event has executed, reset the current event to kNoEvent since
  // we're no longer processing any event. Do it before the call to the
  // delegate, as it might lead to the deletion of this
  // NavigationThrottleRunner.
  Event event = current_event_;
  current_event_ = Event::kNoEvent;
  delegate_->OnNavigationEventProcessed(event, result);
  // DO NOT ADD CODE AFTER THIS. The NavigationThrottleRunner might have been
  // deleted by the previous call.
}

void NavigationThrottleRunner::RecordDeferTimeUKM() {
  if (!is_primary_main_frame_ || !GetDeferringThrottle()) {
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
  builder.SetNavigationThrottleNameHash(
      base::HashMetricName(GetDeferringThrottle()->GetNameForLogging()));
  builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace content
