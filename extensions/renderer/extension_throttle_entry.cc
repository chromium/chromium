// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_throttle_entry.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace extensions {

const int ExtensionThrottleEntry::kDefaultSlidingWindowPeriodMs = 2000;
const int ExtensionThrottleEntry::kDefaultMaxSendThreshold = 20;

// This set of back-off parameters will (at maximum values, i.e. without
// the reduction caused by jitter) add 0-41% (distributed uniformly
// in that range) to the "perceived downtime" of the remote server, once
// exponential back-off kicks in and is throttling requests for more than
// about a second at a time.  Once the maximum back-off is reached, the added
// perceived downtime decreases rapidly, percentage-wise.
//
// Another way to put it is that the maximum additional perceived downtime
// with these numbers is a couple of seconds shy of 15 minutes, and such
// a delay would not occur until the remote server has been actually
// unavailable at the end of each back-off period for a total of about
// 48 minutes.
//
// Ignoring the first couple of errors is just a conservative measure to
// avoid false positives.  It should help avoid back-off from kicking in e.g.
// on flaky connections.
const int ExtensionThrottleEntry::kDefaultNumErrorsToIgnore = 2;
const int ExtensionThrottleEntry::kDefaultInitialDelayMs = 700;
const double ExtensionThrottleEntry::kDefaultMultiplyFactor = 1.4;
const double ExtensionThrottleEntry::kDefaultJitterFactor = 0.4;
const int ExtensionThrottleEntry::kDefaultMaximumBackoffMs = 15 * 60 * 1000;
const int ExtensionThrottleEntry::kDefaultEntryLifetimeMs = 2 * 60 * 1000;

ExtensionThrottleEntry::ExtensionThrottleEntry(const std::string& url_id)
    : sliding_window_period_(
          base::TimeDelta::FromMilliseconds(kDefaultSlidingWindowPeriodMs)),
      max_send_threshold_(kDefaultMaxSendThreshold),
      is_backoff_disabled_(false),
      backoff_entry_(&backoff_policy_),
      url_id_(url_id) {
  Initialize();
}

ExtensionThrottleEntry::ExtensionThrottleEntry(
    const std::string& url_id,
    const net::BackoffEntry::Policy* backoff_policy)
    : sliding_window_period_(
          base::TimeDelta::FromMilliseconds(kDefaultSlidingWindowPeriodMs)),
      max_send_threshold_(kDefaultMaxSendThreshold),
      is_backoff_disabled_(false),
      backoff_entry_(&backoff_policy_),
      url_id_(url_id) {
  DCHECK_GE(backoff_policy->initial_delay_ms, 0);
  DCHECK_GT(backoff_policy->multiply_factor, 0);
  DCHECK_GE(backoff_policy->jitter_factor, 0.0);
  DCHECK_LT(backoff_policy->jitter_factor, 1.0);
  DCHECK_GE(backoff_policy->maximum_backoff_ms, 0);

  Initialize();
  backoff_policy_ = *backoff_policy;
}

bool ExtensionThrottleEntry::IsEntryOutdated() const {
  // If there are send events in the sliding window period, we still need this
  // entry.
  if (!send_log_.empty() &&
      send_log_.back() + sliding_window_period_ > ImplGetTimeNow()) {
    return false;
  }

  return GetBackoffEntry()->CanDiscard();
}

void ExtensionThrottleEntry::DisableBackoffThrottling() {
  is_backoff_disabled_ = true;
}

bool ExtensionThrottleEntry::ShouldRejectRequest() const {
  if (is_backoff_disabled_)
    return false;
  return GetBackoffEntry()->ShouldRejectRequest();
}

int64_t ExtensionThrottleEntry::ReserveSendingTimeForNextRequest(
    const base::TimeTicks& earliest_time) {
  base::TimeTicks now = ImplGetTimeNow();

  // If a lot of requests were successfully made recently,
  // sliding_window_release_time_ may be greater than
  // exponential_backoff_release_time_.
  base::TimeTicks recommended_sending_time =
      std::max(std::max(now, earliest_time),
               std::max(GetBackoffEntry()->GetReleaseTime(),
                        sliding_window_release_time_));

  DCHECK(send_log_.empty() || recommended_sending_time >= send_log_.back());
  // Log the new send event.
  send_log_.push(recommended_sending_time);

  sliding_window_release_time_ = recommended_sending_time;

  // Drop the out-of-date events in the event list.
  // We don't need to worry that the queue may become empty during this
  // operation, since the last element is sliding_window_release_time_.
  while ((send_log_.front() + sliding_window_period_ <=
          sliding_window_release_time_) ||
         send_log_.size() > static_cast<unsigned>(max_send_threshold_)) {
    send_log_.pop();
  }

  // Check if there are too many send events in recent time.
  if (send_log_.size() == static_cast<unsigned>(max_send_threshold_))
    sliding_window_release_time_ = send_log_.front() + sliding_window_period_;

  return (recommended_sending_time - now).InMillisecondsRoundedUp();
}

base::TimeTicks ExtensionThrottleEntry::GetExponentialBackoffReleaseTime()
    const {
  // If a site opts out, it's likely because they have problems that trigger
  // the back-off mechanism when it shouldn't be triggered, in which case
  // returning the calculated back-off release time would probably be the
  // wrong thing to do (i.e. it would likely be too long).  Therefore, we
  // return "now" so that retries are not delayed.
  if (is_backoff_disabled_)
    return ImplGetTimeNow();

  return GetBackoffEntry()->GetReleaseTime();
}

void ExtensionThrottleEntry::UpdateWithResponse(int status_code) {
  GetBackoffEntry()->InformOfRequest(IsConsideredSuccess(status_code));
}

void ExtensionThrottleEntry::ReceivedContentWasMalformed(int response_code) {
  // A malformed body can only occur when the request to fetch a resource
  // was successful.  Therefore, in such a situation, we will receive one
  // call to ReceivedContentWasMalformed() and one call to
  // UpdateWithResponse() with a response categorized as "good".  To end
  // up counting one failure, we need to count two failures here against
  // the one success in UpdateWithResponse().
  //
  // We do nothing for a response that is already being considered an error
  // based on its status code (otherwise we would count 3 errors instead of 1).
  if (IsConsideredSuccess(response_code)) {
    GetBackoffEntry()->InformOfRequest(false);
    GetBackoffEntry()->InformOfRequest(false);
  }
}

const std::string& ExtensionThrottleEntry::GetURLIdForDebugging() const {
  return url_id_;
}

ExtensionThrottleEntry::~ExtensionThrottleEntry() {}

void ExtensionThrottleEntry::Initialize() {
  sliding_window_release_time_ = base::TimeTicks::Now();
  backoff_policy_.num_errors_to_ignore = kDefaultNumErrorsToIgnore;
  backoff_policy_.initial_delay_ms = kDefaultInitialDelayMs;
  backoff_policy_.multiply_factor = kDefaultMultiplyFactor;
  backoff_policy_.jitter_factor = kDefaultJitterFactor;
  backoff_policy_.maximum_backoff_ms = kDefaultMaximumBackoffMs;
  backoff_policy_.entry_lifetime_ms = kDefaultEntryLifetimeMs;
  backoff_policy_.always_use_initial_delay = false;
}

bool ExtensionThrottleEntry::IsConsideredSuccess(int response_code) {
  // We throttle only for the status codes most likely to indicate the server
  // is failing because it is too busy or otherwise are likely to be
  // because of DDoS.
  //
  // 500 is the generic error when no better message is suitable, and
  //     as such does not necessarily indicate a temporary state, but
  //     other status codes cover most of the permanent error states.
  // 503 is explicitly documented as a temporary state where the server
  //     is either overloaded or down for maintenance.
  // 509 is the (non-standard but widely implemented) Bandwidth Limit Exceeded
  //     status code, which might indicate DDoS.
  //
  // We do not back off on 502 or 504, which are reported by gateways
  // (proxies) on timeouts or failures, because in many cases these requests
  // have not made it to the destination server and so we do not actually
  // know that it is down or busy.  One degenerate case could be a proxy on
  // localhost, where you are not actually connected to the network.
  return !(response_code == 500 || response_code == 503 ||
           response_code == 509);
}

base::TimeTicks ExtensionThrottleEntry::ImplGetTimeNow() const {
  return base::TimeTicks::Now();
}

const net::BackoffEntry* ExtensionThrottleEntry::GetBackoffEntry() const {
  return &backoff_entry_;
}

net::BackoffEntry* ExtensionThrottleEntry::GetBackoffEntry() {
  return &backoff_entry_;
}

}  // namespace extensions
