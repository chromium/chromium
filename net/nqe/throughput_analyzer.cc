// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/throughput_analyzer.h"

#include <cmath>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_activity_monitor.h"
#include "net/base/url_util.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/nqe/network_quality_estimator_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace net {

class HostResolver;

namespace {

// Maximum number of accuracy degrading requests, and requests that do not
// degrade accuracy held in the memory.
static const size_t kMaxRequestsSize = 300;

// Returns true if the request should be discarded because it does not provide
// meaningful observation.
bool ShouldDiscardRequest(const URLRequest& request) {
  return request.method() != "GET";
}

}  // namespace

namespace nqe::internal {
// The default content size of a HTML response body. It is set to the median
// HTML response content size, i.e. 1.8kB.
constexpr int64_t kDefaultContentSizeBytes = 1800;

ThroughputAnalyzer::ThroughputAnalyzer(
    const NetworkQualityEstimator* network_quality_estimator,
    const NetworkQualityEstimatorParams* params,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ThroughputObservationCallback throughput_observation_callback,
    const base::TickClock* tick_clock,
    const NetLogWithSource& net_log)
    : network_quality_estimator_(network_quality_estimator),
      params_(params),
      task_runner_(task_runner),
      throughput_observation_callback_(throughput_observation_callback),
      tick_clock_(tick_clock),
      last_connection_change_(tick_clock_->NowTicks()),
      window_start_time_(base::TimeTicks()),
      net_log_(net_log) {
  DCHECK(tick_clock_);
  DCHECK(network_quality_estimator_);
  DCHECK(params_);
  DCHECK(task_runner_);
  DCHECK(!IsCurrentlyTrackingThroughput());
}

ThroughputAnalyzer::~ThroughputAnalyzer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ThroughputAnalyzer::MaybeStartThroughputObservationWindow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disable_throughput_measurements_)
    return;

  // Throughput observation window can be started only if no accuracy degrading
  // requests are currently active, the observation window is not already
  // started, and there is at least one active request that does not degrade
  // throughput computation accuracy.
  if (accuracy_degrading_requests_.size() > 0 ||
      IsCurrentlyTrackingThroughput() ||
      requests_.size() < params_->throughput_min_requests_in_flight()) {
    return;
  }
  window_start_time_ = tick_clock_->NowTicks();
  bits_received_at_window_start_ = GetBitsReceived();
}

void ThroughputAnalyzer::EndThroughputObservationWindow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Mark the throughput observation window as stopped by resetting the window
  // parameters.
  window_start_time_ = base::TimeTicks();
  bits_received_at_window_start_ = 0;
  DCHECK(!IsCurrentlyTrackingThroughput());
}

bool ThroughputAnalyzer::IsCurrentlyTrackingThroughput() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (window_start_time_.is_null())
    return false;

  // If the throughput observation window is running, then at least one request
  // that does not degrade throughput computation accuracy should be active.
  DCHECK_GT(requests_.size(), 0U);

  // If the throughput observation window is running, then no accuracy degrading
  // requests should be currently active.
  DCHECK_EQ(0U, accuracy_degrading_requests_.size());

  DCHECK_LE(params_->throughput_min_requests_in_flight(), requests_.size());

  return true;
}

void ThroughputAnalyzer::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tick_clock_ = tick_clock;
  DCHECK(tick_clock_);
}

void ThroughputAnalyzer::UpdateResponseContentSize(const URLRequest* request,
                                                   int64_t response_size) {
  DCHECK_LE(0, response_size);
  // Updates the map and the counter. Subtracts the previous stored response
  // content size if an old record exists in the map.
  if (response_content_sizes_.find(request) != response_content_sizes_.end()) {
    total_response_content_size_ +=
        response_size - response_content_sizes_[request];
  } else {
    total_response_content_size_ += response_size;
  }
  response_content_sizes_[request] = response_size;
}

void ThroughputAnalyzer::NotifyStartTransaction(const URLRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateResponseContentSize(&request, kDefaultContentSizeBytes);

  if (disable_throughput_measurements_)
    return;

  const bool degrades_accuracy = DegradesAccuracy(request);
  if (degrades_accuracy) {
    accuracy_degrading_requests_.insert(&request);

    BoundRequestsSize();

    // Call EndThroughputObservationWindow since observations cannot be
    // recorded in the presence of requests that degrade throughput computation
    // accuracy.
    EndThroughputObservationWindow();
    DCHECK(!IsCurrentlyTrackingThroughput());
    return;
  } else if (ShouldDiscardRequest(request)) {
    return;
  }

  EraseHangingRequests(request);

  requests_[&request] = tick_clock_->NowTicks();
  BoundRequestsSize();
  MaybeStartThroughputObservationWindow();
}

void ThroughputAnalyzer::NotifyBytesRead(const URLRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disable_throughput_measurements_)
    return;

  EraseHangingRequests(request);

  if (requests_.erase(&request) == 0)
    return;

  // Update the time when the bytes were received for |request|.
  requests_[&request] = tick_clock_->NowTicks();
}

void ThroughputAnalyzer::NotifyRequestCompleted(const URLRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Remove the request from the inflight requests if it presents in the map.
  if (response_content_sizes_.find(&request) != response_content_sizes_.end()) {
    total_response_content_size_ -= response_content_sizes_[&request];
    response_content_sizes_.erase(&request);
  }

  if (disable_throughput_measurements_)
    return;

  // Return early if the |request| is not present in the collections of
  // requests. This may happen when a completed request is later destroyed.
  if (requests_.find(&request) == requests_.end() &&
      accuracy_degrading_requests_.find(&request) ==
          accuracy_degrading_requests_.end()) {
    return;
  }

  EraseHangingRequests(request);

  int32_t downstream_kbps = -1;
  if (MaybeGetThroughputObservation(&downstream_kbps)) {
    // Notify the provided callback.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(throughput_observation_callback_, downstream_kbps));
  }

  // Try to remove the request from either |accuracy_degrading_requests_| or
  // |requests_|, since it is no longer active.
  if (accuracy_degrading_requests_.erase(&request) == 1u) {
    // Generally, |request| cannot be in both |accuracy_degrading_requests_|
    // and |requests_| at the same time. However, in some cases, the same
    // request may appear in both vectors. See https://crbug.com/849604 for
    // more details.
    // It's safe to delete |request| from |requests_| since (i)
    // The observation window is currently not recording throughput, and (ii)
    // |requests_| is a best effort guess of requests that are currently
    // in-flight.
    DCHECK(!IsCurrentlyTrackingThroughput());
    requests_.erase(&request);

    // If a request that degraded the accuracy of throughput computation has
    // completed, then it may be possible to start the tracking window.
    MaybeStartThroughputObservationWindow();
    return;
  }

  if (requests_.erase(&request) == 1u) {
    // If there is no network activity, stop tracking throughput to prevent
    // recording of any observations.
    if (requests_.size() < params_->throughput_min_requests_in_flight())
      EndThroughputObservationWindow();
    return;
  }
  MaybeStartThroughputObservationWindow();
}

void ThroughputAnalyzer::NotifyExpectedResponseContentSize(
    const URLRequest& request,
    int64_t expected_content_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Updates when the value is valid.
  if (expected_content_size >= 0) {
    UpdateResponseContentSize(&request, expected_content_size);
  }
}

bool ThroughputAnalyzer::IsHangingWindow(int64_t bits_received,
                                         base::TimeDelta duration) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (params_->throughput_hanging_requests_cwnd_size_multiplier() <= 0)
    return false;

  if (params_->use_small_responses())
    return false;

  if (!duration.is_positive())
    return false;

  // Initial congestion window size for TCP connections.
  static constexpr size_t kCwndSizeKilobytes = 10 * 1.5;
  static constexpr size_t kCwndSizeBits = kCwndSizeKilobytes * 1000 * 8;

  // Scale the |duration| to one HTTP RTT, and compute the number of bits that
  // would be received over a duration of one HTTP RTT.
  size_t bits_received_over_one_http_rtt =
      bits_received *
      (network_quality_estimator_->GetHttpRTT().value_or(base::Seconds(10)) /
       duration);

  // If |is_hanging| is true, it implies that less than
  // kCwndSizeKilobytes were received over a period of 1 HTTP RTT. For a network
  // that is not under-utilized, it is expected that at least |kCwndSizeBits|
  // are received over a duration of 1 HTTP RTT.
  bool is_hanging =
      bits_received_over_one_http_rtt <
      (kCwndSizeBits *
       params_->throughput_hanging_requests_cwnd_size_multiplier());

  return is_hanging;
}

bool ThroughputAnalyzer::MaybeGetThroughputObservation(
    int32_t* downstream_kbps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(downstream_kbps);

  if (disable_throughput_measurements_)
    return false;

  // Return early if the window that records downstream throughput is currently
  // inactive because throughput observations can be taken only when the window
  // is active.
  if (!IsCurrentlyTrackingThroughput())
    return false;

  DCHECK_GE(requests_.size(), params_->throughput_min_requests_in_flight());
  DCHECK_EQ(0U, accuracy_degrading_requests_.size());

  base::TimeTicks now = tick_clock_->NowTicks();

  int64_t bits_received = GetBitsReceived() - bits_received_at_window_start_;
  DCHECK_LE(window_start_time_, now);
  DCHECK_LE(0, bits_received);
  const base::TimeDelta duration = now - window_start_time_;

  // Ignore tiny/short transfers, which will not produce accurate rates. Skip
  // the checks if |use_small_responses_| is true.
  if (!params_->use_small_responses() &&
      bits_received < params_->GetThroughputMinTransferSizeBits()) {
    return false;
  }

  double downstream_kbps_double = bits_received * duration.ToHz() / 1000;

  if (IsHangingWindow(bits_received, duration)) {
    requests_.clear();
    EndThroughputObservationWindow();
    return false;
  }

  // Round-up |downstream_kbps_double|.
  *downstream_kbps = base::ClampCeil<int32_t>(downstream_kbps_double);
  DCHECK(IsCurrentlyTrackingThroughput());

  // Stop the observation window since a throughput measurement has been taken.
  EndThroughputObservationWindow();
  DCHECK(!IsCurrentlyTrackingThroughput());

  // Maybe start the throughput observation window again so that another
  // throughput measurement can be taken.
  MaybeStartThroughputObservationWindow();
  return true;
}

void ThroughputAnalyzer::OnConnectionTypeChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // All the requests that were previously not degrading the througpput
  // computation are now spanning a connection change event. These requests
  // would now degrade the throughput computation accuracy. So, move them to
  // |accuracy_degrading_requests_|.
  for (const auto& request : requests_) {
    accuracy_degrading_requests_.insert(request.first);
  }
  requests_.clear();
  BoundRequestsSize();
  EndThroughputObservationWindow();

  last_connection_change_ = tick_clock_->NowTicks();
}

void ThroughputAnalyzer::SetUseLocalHostRequestsForTesting(
    bool use_localhost_requests) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  use_localhost_requests_for_tests_ = use_localhost_requests;
}

int64_t ThroughputAnalyzer::GetBitsReceived() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return activity_monitor::GetBytesReceived() * 8;
}

size_t ThroughputAnalyzer::CountActiveInFlightRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return requests_.size();
}

size_t ThroughputAnalyzer::CountTotalInFlightRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return response_content_sizes_.size();
}

int64_t ThroughputAnalyzer::CountTotalContentSizeBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return total_response_content_size_;
}

bool ThroughputAnalyzer::DegradesAccuracy(const URLRequest& request) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool private_network_request =
      nqe::internal::IsRequestForPrivateHost(request, net_log_);

  return !(use_localhost_requests_for_tests_ || !private_network_request) ||
         request.creation_time() < last_connection_change_;
}

void ThroughputAnalyzer::BoundRequestsSize() {
  if (accuracy_degrading_requests_.size() > kMaxRequestsSize) {
    // Clear |accuracy_degrading_requests_| since its size has exceeded its
    // capacity.
    accuracy_degrading_requests_.clear();
    // Disable throughput measurements since |this| has lost track of the
    // accuracy degrading requests.
    disable_throughput_measurements_ = true;

    // Reset other variables related to tracking since the tracking is now
    // disabled.
    EndThroughputObservationWindow();
    DCHECK(!IsCurrentlyTrackingThroughput());
    requests_.clear();

    // TODO(tbansal): crbug.com/609174 Add UMA to record how frequently this
    // happens.
  }

  if (requests_.size() > kMaxRequestsSize) {
    // Clear |requests_| since its size has exceeded its capacity.
    EndThroughputObservationWindow();
    DCHECK(!IsCurrentlyTrackingThroughput());
    requests_.clear();

    // TODO(tbansal): crbug.com/609174 Add UMA to record how frequently this
    // happens.
  }
}

void ThroughputAnalyzer::EraseHangingRequests(const URLRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_LT(0, params_->hanging_request_duration_http_rtt_multiplier());

  const base::TimeTicks now = tick_clock_->NowTicks();

  const base::TimeDelta http_rtt =
      network_quality_estimator_->GetHttpRTT().value_or(base::Seconds(60));

  size_t count_request_erased = 0;
  auto request_it = requests_.find(&request);
  if (request_it != requests_.end()) {
    base::TimeDelta time_since_last_received = now - request_it->second;

    if (time_since_last_received >=
            params_->hanging_request_duration_http_rtt_multiplier() *
                http_rtt &&
        time_since_last_received >= params_->hanging_request_min_duration()) {
      count_request_erased++;
      requests_.erase(request_it);
    }
  }

  if (now - last_hanging_request_check_ >= base::Seconds(1)) {
    // Hanging request check is done at most once per second.
    last_hanging_request_check_ = now;

    for (auto it = requests_.begin(); it != requests_.end();) {
      base::TimeDelta time_since_last_received = now - it->second;

      if (time_since_last_received >=
              params_->hanging_request_duration_http_rtt_multiplier() *
                  http_rtt &&
          time_since_last_received >= params_->hanging_request_min_duration()) {
        count_request_erased++;
        requests_.erase(it++);
      } else {
        ++it;
      }
    }
  }

  if (count_request_erased > 0) {
    // End the observation window since there is at least one hanging GET in
    // flight, which may lead to inaccuracies in the throughput estimate
    // computation.
    EndThroughputObservationWindow();
  }
}

}  // namespace nqe::internal

}  // namespace net
