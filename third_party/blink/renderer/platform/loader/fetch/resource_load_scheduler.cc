// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_clock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/loading_behavior_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/scheduler/public/aggregated_metric_reporter.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_status.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kRendererSideResourceScheduler[] =
    "RendererSideResourceScheduler";

// Used in the tight mode (see the header file for details).
constexpr size_t kTightLimitForRendererSideResourceScheduler = 2u;
// Used in the normal mode (see the header file for details).
constexpr size_t kLimitForRendererSideResourceScheduler = 1024u;

constexpr char kTightLimitForRendererSideResourceSchedulerName[] =
    "tight_limit";
constexpr char kLimitForRendererSideResourceSchedulerName[] = "limit";

// Represents a resource load circumstance, e.g. from main frame vs sub-frames,
// or on throttled state vs on not-throttled state.
// Used to report histograms. Do not reorder or insert new items.
enum class ReportCircumstance {
  kMainframeThrottled,
  kMainframeNotThrottled,
  kSubframeThrottled,
  kSubframeNotThrottled,
  // Append new items here.
  kNumOfCircumstances,
};

uint32_t GetFieldTrialUint32Param(const char* trial_name,
                                  const char* parameter_name,
                                  uint32_t default_param) {
  base::FieldTrialParams trial_params;
  bool result = base::GetFieldTrialParams(trial_name, &trial_params);
  if (!result)
    return default_param;

  const auto& found = trial_params.find(parameter_name);
  if (found == trial_params.end())
    return default_param;

  uint32_t param;
  if (!base::StringToUint(found->second, &param))
    return default_param;

  return param;
}

}  // namespace

constexpr ResourceLoadScheduler::ClientId
    ResourceLoadScheduler::kInvalidClientId;

ResourceLoadScheduler::ResourceLoadScheduler(
    ThrottlingPolicy initial_throttling_policy,
    ThrottleOptionOverride throttle_option_override,
    const DetachableResourceFetcherProperties& resource_fetcher_properties,
    FrameOrWorkerScheduler* frame_or_worker_scheduler,
    DetachableConsoleLogger& console_logger,
    LoadingBehaviorObserver* loading_behavior_observer)
    : resource_fetcher_properties_(resource_fetcher_properties),
      policy_(initial_throttling_policy),
      outstanding_limit_for_throttled_frame_scheduler_(
          resource_fetcher_properties_->GetOutstandingThrottledLimit()),
      console_logger_(console_logger),
      clock_(base::DefaultClock::GetInstance()),
      throttle_option_override_(throttle_option_override),
      loading_behavior_observer_(loading_behavior_observer) {
  if (!frame_or_worker_scheduler)
    return;

  normal_outstanding_limit_ =
      GetFieldTrialUint32Param(kRendererSideResourceScheduler,
                               kLimitForRendererSideResourceSchedulerName,
                               kLimitForRendererSideResourceScheduler);
  tight_outstanding_limit_ =
      GetFieldTrialUint32Param(kRendererSideResourceScheduler,
                               kTightLimitForRendererSideResourceSchedulerName,
                               kTightLimitForRendererSideResourceScheduler);

  if (base::FeatureList::IsEnabled(features::kBoostImagePriority)) {
    tight_medium_limit_ = features::kBoostImagePriorityTightMediumLimit.Get();
  }

  scheduler_observer_handle_ = frame_or_worker_scheduler->AddLifecycleObserver(
      FrameScheduler::ObserverType::kLoader,
      WTF::BindRepeating(&ResourceLoadScheduler::OnLifecycleStateChanged,
                         WrapWeakPersistent(this)));
}

ResourceLoadScheduler::~ResourceLoadScheduler() = default;

void ResourceLoadScheduler::Trace(Visitor* visitor) const {
  visitor->Trace(pending_request_map_);
  visitor->Trace(resource_fetcher_properties_);
  visitor->Trace(console_logger_);
  visitor->Trace(loading_behavior_observer_);
}

void ResourceLoadScheduler::LoosenThrottlingPolicy() {
  switch (policy_) {
    case ThrottlingPolicy::kTight:
      break;
    case ThrottlingPolicy::kNormal:
      return;
  }
  policy_ = ThrottlingPolicy::kNormal;
  MaybeRun();
}

void ResourceLoadScheduler::Shutdown() {
  // Do nothing if the feature is not enabled, or Shutdown() was already called.
  if (is_shutdown_)
    return;
  is_shutdown_ = true;

  scheduler_observer_handle_.reset();
}

void ResourceLoadScheduler::Request(ResourceLoadSchedulerClient* client,
                                    ThrottleOption option,
                                    ResourceLoadPriority priority,
                                    int intra_priority,
                                    ResourceLoadScheduler::ClientId* id) {
  *id = GenerateClientId();
  if (is_shutdown_)
    return;

  if (option == ThrottleOption::kStoppable &&
      throttle_option_override_ ==
          ThrottleOptionOverride::kStoppableAsThrottleable) {
    option = ThrottleOption::kThrottleable;
  }

  // Check if the request can be throttled.
  ClientIdWithPriority request_info(*id, priority, intra_priority);
  if (!IsClientDelayable(option)) {
    Run(*id, client, /*throttleable=*/false, priority);
    return;
  }

  DCHECK(ThrottleOption::kStoppable == option ||
         ThrottleOption::kThrottleable == option);
  if (pending_requests_[option].empty())
    pending_queue_update_times_[option] = clock_->Now();
  pending_requests_[option].insert(request_info);
  pending_request_map_.insert(
      *id, MakeGarbageCollected<ClientInfo>(client, option, priority,
                                            intra_priority));

  // Remember the ClientId since MaybeRun() below may destruct the caller
  // instance and |id| may be inaccessible after the call.
  MaybeRun();
}

void ResourceLoadScheduler::SetPriority(ClientId client_id,
                                        ResourceLoadPriority priority,
                                        int intra_priority) {
  auto client_it = pending_request_map_.find(client_id);
  if (client_it == pending_request_map_.end())
    return;

  auto& throttle_option_queue = pending_requests_[client_it->value->option];

  auto it = throttle_option_queue.find(ClientIdWithPriority(
      client_id, client_it->value->priority, client_it->value->intra_priority));

  CHECK(it != throttle_option_queue.end(), base::NotFatalUntil::M130);
  throttle_option_queue.erase(it);

  client_it->value->priority = priority;
  client_it->value->intra_priority = intra_priority;

  throttle_option_queue.emplace(client_id, priority, intra_priority);
  MaybeRun();
}

bool ResourceLoadScheduler::Release(
    ResourceLoadScheduler::ClientId id,
    ResourceLoadScheduler::ReleaseOption option,
    const ResourceLoadScheduler::TrafficReportHints& hints) {
  // Check kInvalidClientId that can not be passed to the HashSet.
  if (id == kInvalidClientId)
    return false;

  auto running_request = running_requests_.find(id);
  if (running_request != running_requests_.end()) {
    if (running_request->value)
      in_flight_on_multiplexed_connections_--;

    running_requests_.erase(id);
    running_throttleable_requests_.erase(id);
    running_medium_requests_.erase(id);

    if (option == ReleaseOption::kReleaseAndSchedule)
      MaybeRun();
    return true;
  }

  // The client may not appear in the |pending_request_map_|. For example,
  // non-delayable requests are immediately granted and skip being placed into
  // this map.
  auto pending_request = pending_request_map_.find(id);
  if (pending_request != pending_request_map_.end()) {
    pending_request_map_.erase(pending_request);
    // Intentionally does not remove it from |pending_requests_|.

    // Didn't release any running requests, but the outstanding limit might be
    // changed to allow another request.
    if (option == ReleaseOption::kReleaseAndSchedule)
      MaybeRun();
    return true;
  }
  return false;
}

void ResourceLoadScheduler::SetOutstandingLimitForTesting(
    size_t tight_limit,
    size_t normal_limit,
    size_t tight_medium_limit) {
  tight_outstanding_limit_ = tight_limit;
  normal_outstanding_limit_ = normal_limit;
  tight_medium_limit_ = tight_medium_limit;
  MaybeRun();
}

bool ResourceLoadScheduler::IsClientDelayable(ThrottleOption option) const {
  switch (frame_scheduler_lifecycle_state_) {
    case scheduler::SchedulingLifecycleState::kNotThrottled:
    case scheduler::SchedulingLifecycleState::kHidden:
    case scheduler::SchedulingLifecycleState::kThrottled:
      return option == ThrottleOption::kThrottleable;
    case scheduler::SchedulingLifecycleState::kStopped:
      return option != ThrottleOption::kCanNotBeStoppedOrThrottled;
  }
}

void ResourceLoadScheduler::OnLifecycleStateChanged(
    scheduler::SchedulingLifecycleState state) {
  if (frame_scheduler_lifecycle_state_ == state)
    return;

  frame_scheduler_lifecycle_state_ = state;

  if (state == scheduler::SchedulingLifecycleState::kNotThrottled)
    ShowConsoleMessageIfNeeded();

  MaybeRun();
}

ResourceLoadScheduler::ClientId ResourceLoadScheduler::GenerateClientId() {
  ClientId id = ++current_id_;
  CHECK_NE(0u, id);
  return id;
}

bool ResourceLoadScheduler::IsPendingRequestEffectivelyEmpty(
    ThrottleOption option) {
  for (const auto& client : pending_requests_[option]) {
    // The request in |pending_request_| is erased when it is scheduled. So if
    // the request is canceled, or Release() is called before firing its Run(),
    // the entry for the request remains in |pending_request_| until it is
    // popped in GetNextPendingRequest().
    if (base::Contains(pending_request_map_, client.client_id)) {
      return false;
    }
  }
  // There is no entry, or no existing entries are alive in
  // |pending_request_map_|.
  return true;
}

bool ResourceLoadScheduler::GetNextPendingRequest(ClientId* id) {
  auto& stoppable_queue = pending_requests_[ThrottleOption::kStoppable];
  auto& throttleable_queue = pending_requests_[ThrottleOption::kThrottleable];

  // Check if stoppable or throttleable requests are allowed to be run.
  auto stoppable_it = stoppable_queue.begin();
  bool has_runnable_stoppable_request =
      stoppable_it != stoppable_queue.end() &&
      (!IsClientDelayable(ThrottleOption::kStoppable) ||
       IsRunningThrottleableRequestsLessThanOutStandingLimit(
           GetOutstandingLimit(stoppable_it->priority),
           stoppable_it->priority));

  auto throttleable_it = throttleable_queue.begin();
  bool has_runnable_throttleable_request =
      throttleable_it != throttleable_queue.end() &&
      (!IsClientDelayable(ThrottleOption::kThrottleable) ||
       IsRunningThrottleableRequestsLessThanOutStandingLimit(
           GetOutstandingLimit(throttleable_it->priority),
           throttleable_it->priority));

  if (!has_runnable_throttleable_request && !has_runnable_stoppable_request)
    return false;

  // If both requests are allowed to be run, run the high priority requests
  // first.
  ClientIdWithPriority::Compare compare;
  bool use_stoppable = has_runnable_stoppable_request &&
                       (!has_runnable_throttleable_request ||
                        compare(*stoppable_it, *throttleable_it));

  // Remove the iterator from the correct set of |pending_requests_|, and update
  // corresponding |pending_queue_update_times_|.
  if (use_stoppable) {
    *id = stoppable_it->client_id;
    stoppable_queue.erase(stoppable_it);
    pending_queue_update_times_[ThrottleOption::kStoppable] = clock_->Now();
    return true;
  }

  *id = throttleable_it->client_id;
  throttleable_queue.erase(throttleable_it);
  pending_queue_update_times_[ThrottleOption::kThrottleable] = clock_->Now();
  return true;
}

void ResourceLoadScheduler::MaybeRun() {
  // Requests for keep-alive loaders could be remained in the pending queue,
  // but ignore them once Shutdown() is called.
  if (is_shutdown_)
    return;

  // Updates the RTT before getting the next pending request in the tight mode.
  if (policy_ == ThrottlingPolicy::kTight) {
    http_rtt_ = http_rtt_for_testing_ ? http_rtt_for_testing_
                                      : GetNetworkStateNotifier().HttpRtt();
  }

  ClientId id = kInvalidClientId;
  while (GetNextPendingRequest(&id)) {
    auto found = pending_request_map_.find(id);
    if (found == pending_request_map_.end())
      continue;  // Already released.

    ResourceLoadSchedulerClient* client = found->value->client;
    ThrottleOption option = found->value->option;
    ResourceLoadPriority priority = found->value->priority;
    pending_request_map_.erase(found);
    Run(id, client, option == ThrottleOption::kThrottleable, priority);
  }
}

void ResourceLoadScheduler::Run(ResourceLoadScheduler::ClientId id,
                                ResourceLoadSchedulerClient* client,
                                bool throttleable,
                                ResourceLoadPriority priority) {
  // Assuming the request connection is not multiplexed.
  running_requests_.insert(id, IsMultiplexedConnection(false));
  if (throttleable)
    running_throttleable_requests_.insert(id);
  if (priority == ResourceLoadPriority::kMedium) {
    running_medium_requests_.insert(id);
  }
  client->Run();
}

size_t ResourceLoadScheduler::GetOutstandingLimit(
    ResourceLoadPriority priority) const {
  size_t limit = kOutstandingUnlimited;

  switch (frame_scheduler_lifecycle_state_) {
    case scheduler::SchedulingLifecycleState::kHidden:
    case scheduler::SchedulingLifecycleState::kThrottled:
      limit = std::min(limit, outstanding_limit_for_throttled_frame_scheduler_);
      break;
    case scheduler::SchedulingLifecycleState::kNotThrottled:
      break;
    case scheduler::SchedulingLifecycleState::kStopped:
      limit = 0;
      break;
  }

  size_t policy_limit = normal_outstanding_limit_;
  switch (policy_) {
    case ThrottlingPolicy::kTight:
      if (priority < ResourceLoadPriority::kHigh) {
        if (CanRequestForMultiplexedConnectionsInTight()) {
          policy_limit = static_cast<size_t>(
              features::kMaxNumOfThrottleableRequestsInTightMode.Get());
        } else {
          policy_limit = tight_outstanding_limit_;
        }
      }
      limit = std::min(limit, policy_limit);
      break;
    case ThrottlingPolicy::kNormal:
      limit = std::min(limit, normal_outstanding_limit_);
      break;
  }
  return limit;
}

void ResourceLoadScheduler::ShowConsoleMessageIfNeeded() {
  if (is_console_info_shown_ || pending_request_map_.empty())
    return;

  const base::Time limit = clock_->Now() - base::Seconds(60);
  if ((pending_queue_update_times_[ThrottleOption::kThrottleable] >= limit ||
       IsPendingRequestEffectivelyEmpty(ThrottleOption::kThrottleable)) &&
      (pending_queue_update_times_[ThrottleOption::kStoppable] >= limit ||
       IsPendingRequestEffectivelyEmpty(ThrottleOption::kStoppable))) {
    // At least, one of the top requests in pending queues was handled in the
    // last 1 minutes, or there is no pending requests in the inactive queue.
    return;
  }
  console_logger_->AddConsoleMessage(
      mojom::ConsoleMessageSource::kOther, mojom::ConsoleMessageLevel::kInfo,
      "Some resource load requests were throttled while the tab was in "
      "background, and no request was sent from the queue in the last 1 "
      "minute. This means previously requested in-flight requests haven't "
      "received any response from servers. See "
      "https://www.chromestatus.com/feature/5527160148197376 for more details");
  is_console_info_shown_ = true;
}

bool ResourceLoadScheduler::
    IsRunningThrottleableRequestsLessThanOutStandingLimit(
        size_t out_standing_limit,
        ResourceLoadPriority priority) {
  // Allow for a minimum number of medium-priority requests to be in-flight
  // independent of the overall number of pending requests.
  if (priority == ResourceLoadPriority::kMedium &&
      running_medium_requests_.size() < tight_medium_limit_) {
    return true;
  }
  if (CanRequestForMultiplexedConnectionsInTight()) {
    DCHECK_EQ(policy_, ThrottlingPolicy::kTight);
    return (running_throttleable_requests_.size() -
            in_flight_on_multiplexed_connections_ *
                features::kCostReductionOfMultiplexedRequests.Get()) <
           out_standing_limit;
  }

  return running_throttleable_requests_.size() < out_standing_limit;
}

void ResourceLoadScheduler::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void ResourceLoadScheduler::SetConnectionInfo(
    ClientId id,
    net::HttpConnectionInfo connection_info) {
  DCHECK_NE(kInvalidClientId, id);

  // `is_multiplexed` will be set false if the connection of the given client
  // doesn't support multiplexing (e.g., HTTP/1.x).
  bool is_multiplexed = true;
  switch (connection_info) {
    case net::HttpConnectionInfo::kHTTP0_9:
    case net::HttpConnectionInfo::kHTTP1_0:
    case net::HttpConnectionInfo::kHTTP1_1:
    case net::HttpConnectionInfo::kUNKNOWN:
      is_multiplexed = false;
      break;
    default:
      break;
  }

  auto running_request = running_requests_.find(id);
  if (running_request != running_requests_.end() && is_multiplexed) {
    running_request->value = IsMultiplexedConnection(true);
    in_flight_on_multiplexed_connections_++;
  }

  MaybeRun();
}

bool ResourceLoadScheduler::CanRequestForMultiplexedConnectionsInTight() const {
  // `kDelayLowPriorityRequestAccordingToNetworkState` will be triggered
  // practically iff it's in the tight mode and the value of RTT is less than
  // the `kThresholdOfHttpRtt`.
  return base::FeatureList::IsEnabled(
             features::kDelayLowPriorityRequestsAccordingToNetworkState) &&
         policy_ == ThrottlingPolicy::kTight && http_rtt_ &&
         http_rtt_.value() < features::kHttpRttThreshold.Get();
}

}  // namespace blink
