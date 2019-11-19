// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <algorithm>

#include <net/nqe/network_congestion_analyzer.h>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/url_request/url_request.h"

namespace net {

namespace {

// The threshold for the observed peak queueing delay in milliseconds.
// A peak queueing delay is HIGH if it exceeds this threshold. The value is the
// 98th percentile value of the peak queueing delay observed by all requests.
static constexpr int64_t kHighQueueingDelayMsec = 5000;

// The minimal time interval between two consecutive empty queue observations
// when the number of in-flight requests is relatively low (i.e. 2). This time
// interval is required so that a new measurement period could start.
static constexpr int64_t kMinEmptyQueueObservingTimeMsec = 1500;

// The min and max values for the peak queueing delay level.
static constexpr size_t kQueueingDelayLevelMinVal = 1;
static constexpr size_t kQueueingDelayLevelMaxVal = 10;

// The array of thresholds for bucketizing a peak queueing delay sample.
constexpr base::TimeDelta kQueueingDelayBucketThresholds[] = {
    base::TimeDelta::FromMilliseconds(0),
    base::TimeDelta::FromMilliseconds(30),
    base::TimeDelta::FromMilliseconds(60),
    base::TimeDelta::FromMilliseconds(120),
    base::TimeDelta::FromMilliseconds(250),
    base::TimeDelta::FromMilliseconds(500),
    base::TimeDelta::FromMilliseconds(1000),
    base::TimeDelta::FromMilliseconds(2000),
    base::TimeDelta::FromMilliseconds(4000),
    base::TimeDelta::FromMilliseconds(8000)};

// The array of thresholds for determining whether a queueing delay sample is
// low under different effective connection types (ECTs). Based on the initial
// measurement, the queueing delay shows different distributions under different
// ECTs. For example, a 300-msec queueing delay is low in a 2G connection, and
// indicates the network queue is empty. However, the delay is the 90th
// percentile value on a 4G connection, and indicates many packets are in the
// network queue. These thresholds are the 33rd percentile values from these
// delay distributions. A default value (400 msec) is used when the ECT is
// UNKNOWN or OFFLINE.
constexpr base::TimeDelta
    kLowQueueingDelayThresholds[EFFECTIVE_CONNECTION_TYPE_LAST] = {
        base::TimeDelta::FromMilliseconds(400),
        base::TimeDelta::FromMilliseconds(400),
        base::TimeDelta::FromMilliseconds(400),
        base::TimeDelta::FromMilliseconds(400),
        base::TimeDelta::FromMilliseconds(40),
        base::TimeDelta::FromMilliseconds(15)};

// The min and max values for the count of in-flight requests in
// |count_inflight_requests_to_queueing_delay_| cache. This range covers more
// than 95% of cases under all types of connection types.
static constexpr size_t kMinCountOfRequests = 1u;
static constexpr size_t kMaxCountOfRequests = 30u;

// The max number of samples that can be hold in a bucket in the
// |count_inflight_requests_to_queueing_delay_| cache.
static constexpr size_t kMaxCountOfSamplesPerBucket = 10u;

// The min value for a count-delay mapping sample to be reasonable enough to be
// inserted into the cache.
static constexpr size_t kMinScoreForValidSamples = 50;

}  // namespace

namespace nqe {

namespace internal {

NetworkCongestionAnalyzer::NetworkCongestionAnalyzer(
    NetworkQualityEstimator* network_quality_estimator,
    const base::TickClock* tick_clock)
    : network_quality_estimator_(network_quality_estimator),
      tick_clock_(tick_clock),
      recent_active_hosts_count_(0u),
      count_inflight_requests_for_peak_queueing_delay_(0u),
      peak_count_inflight_requests_measurement_period_(0u),
      count_peak_queueing_delay_mapping_sample_(0u) {
  DCHECK(tick_clock_);
  DCHECK(network_quality_estimator_);
  DCHECK_EQ(kQueueingDelayLevelMaxVal,
            base::size(kQueueingDelayBucketThresholds));
}

NetworkCongestionAnalyzer::~NetworkCongestionAnalyzer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

size_t NetworkCongestionAnalyzer::GetActiveHostsCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return recent_active_hosts_count_;
}

void NetworkCongestionAnalyzer::NotifyStartTransaction(
    const URLRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Starts tracking the peak queueing delay after |request| starts.
  TrackPeakQueueingDelayBegin(&request);
}

void NetworkCongestionAnalyzer::NotifyRequestCompleted(
    const URLRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ends tracking of the peak queueing delay.
  base::Optional<base::TimeDelta> peak_observed_delay =
      TrackPeakQueueingDelayEnd(&request);
  if (peak_observed_delay.has_value()) {
    // Records the peak queueing delay keyed by the request priority.
    base::UmaHistogramMediumTimes(
        "ResourceScheduler.PeakObservedQueueingDelay.Priority" +
            base::NumberToString(request.priority()),
        peak_observed_delay.value());

    // Records the peak queueing delay for all types of requests.
    UMA_HISTOGRAM_MEDIUM_TIMES("ResourceScheduler.PeakObservedQueueingDelay",
                               peak_observed_delay.value());
  }
}

void NetworkCongestionAnalyzer::TrackPeakQueueingDelayBegin(
    const URLRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Returns if |request| has already been tracked.
  if (request_peak_delay_.find(request) != request_peak_delay_.end())
    return;

  request_peak_delay_[request] = base::nullopt;
}

base::Optional<base::TimeDelta>
NetworkCongestionAnalyzer::TrackPeakQueueingDelayEnd(
    const URLRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto request_delay = request_peak_delay_.find(request);
  if (request_delay == request_peak_delay_.end())
    return base::nullopt;

  base::Optional<base::TimeDelta> peak_delay = request_delay->second;
  request_peak_delay_.erase(request_delay);
  return peak_delay;
}

// net::EffectiveConnectionTypeObserver:
void NetworkCongestionAnalyzer::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (effective_connection_type_ == effective_connection_type)
    return;

  effective_connection_type_ = effective_connection_type;
  count_inflight_requests_to_queueing_delay_.clear();
  count_peak_queueing_delay_mapping_sample_ = 0;
}

void NetworkCongestionAnalyzer::ComputeRecentQueueingDelay(
    const std::map<nqe::internal::IPHash, nqe::internal::CanonicalStats>&
        recent_rtt_stats,
    const std::map<nqe::internal::IPHash, nqe::internal::CanonicalStats>&
        historical_rtt_stats,
    const int32_t downlink_kbps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Updates downlink throughput if a new valid observation comes.
  if (downlink_kbps != nqe::internal::INVALID_RTT_THROUGHPUT)
    set_recent_downlink_throughput_kbps(downlink_kbps);
  if (recent_rtt_stats.empty())
    return;

  int32_t delay_sample_sum = 0;
  recent_active_hosts_count_ = 0u;
  for (const auto& host_stats : recent_rtt_stats) {
    nqe::internal::IPHash host = host_stats.first;
    // Skip hosts that do not have historical statistics.
    if (historical_rtt_stats.find(host) == historical_rtt_stats.end())
      continue;

    // Skip hosts that have one or fewer RTT samples or do not have the min
    // value. They cannot provide an effective queueing delay sample.
    if (historical_rtt_stats.at(host).observation_count <= 1 ||
        historical_rtt_stats.at(host).canonical_pcts.find(kStatVal0p) ==
            historical_rtt_stats.at(host).canonical_pcts.end())
      continue;

    ++recent_active_hosts_count_;
    delay_sample_sum +=
        recent_rtt_stats.at(host).most_recent_val -
        historical_rtt_stats.at(host).canonical_pcts.at(kStatVal0p);
  }

  if (recent_active_hosts_count_ == 0u)
    return;

  DCHECK_LT(0u, recent_active_hosts_count_);

  int32_t delay_ms =
      delay_sample_sum / static_cast<int>(recent_active_hosts_count_);
  recent_queueing_delay_ = base::TimeDelta::FromMilliseconds(delay_ms);

  // Updates the peak queueing delay for all tracked in-flight requests.
  for (auto& it : request_peak_delay_) {
    if (it.second.has_value()) {
      it.second = std::max(it.second.value(), recent_queueing_delay_);
    } else {
      it.second = recent_queueing_delay_;
    }
  }

  if (recent_downlink_per_packet_time_ms_ != base::nullopt) {
    recent_queue_length_ = static_cast<float>(delay_ms) /
                           recent_downlink_per_packet_time_ms_.value();
  }
}

size_t NetworkCongestionAnalyzer::ComputePeakQueueingDelayLevel(
    const base::TimeDelta& peak_queueing_delay) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(base::TimeDelta(), peak_queueing_delay);

  // The range of queueing delay buckets includes all non-negative values. Thus,
  // the non-negative peak queueing delay must be found in one of these buckets.
  size_t level = kQueueingDelayLevelMaxVal;
  while (level > kQueueingDelayLevelMinVal) {
    // Stops searching if the peak queueing delay falls in the current bucket.
    if (peak_queueing_delay >= kQueueingDelayBucketThresholds[level - 1])
      break;

    --level;
  }
  // The queueing delay level is from 1 (LOWEST) to 10 (HIGHEST).
  DCHECK_LE(kQueueingDelayLevelMinVal, level);
  DCHECK_GE(kQueueingDelayLevelMaxVal, level);
  return level;
}

bool NetworkCongestionAnalyzer::IsQueueingDelayLow(
    const base::TimeDelta& delay) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net::EffectiveConnectionType current_ect =
      network_quality_estimator_->GetEffectiveConnectionType();
  return delay <= kLowQueueingDelayThresholds[current_ect];
}

bool NetworkCongestionAnalyzer::ShouldStartNewMeasurement(
    const base::TimeDelta& delay,
    size_t count_inflight_requests) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The queue is not empty if either the queueing delay is high or the number
  // of in-flight requests is high.
  if (!IsQueueingDelayLow(delay) || count_inflight_requests >= 3) {
    observing_empty_queue_timestamp_ = base::nullopt;
    return false;
  }

  // Starts a new measurement period immediately if there is very few number of
  // in-flight requests.
  if (count_inflight_requests <= 1) {
    observing_empty_queue_timestamp_ = base::nullopt;
    return true;
  }

  base::TimeTicks now = tick_clock_->NowTicks();
  // Requires a sufficient time interval between consecutive empty queue
  // observations to claim the queue is empty.
  if (observing_empty_queue_timestamp_.has_value()) {
    if (now - observing_empty_queue_timestamp_.value() >=
        base::TimeDelta::FromMilliseconds(kMinEmptyQueueObservingTimeMsec)) {
      observing_empty_queue_timestamp_ = base::nullopt;
      return true;
    }
  } else {
    observing_empty_queue_timestamp_ = now;
  }
  return false;
}

void NetworkCongestionAnalyzer::UpdatePeakDelayMapping(
    const base::TimeDelta& delay,
    size_t count_inflight_requests) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Discards an abnormal observation. This high queueing delay is likely
  // caused by retransmission packets from a previous measurement period.
  if (delay >= base::TimeDelta::FromSeconds(20))
    return;

  if (ShouldStartNewMeasurement(delay, count_inflight_requests)) {
    FinalizeCurrentMeasurementPeriod();

    // Resets the tracked data for the new measurement period.
    peak_queueing_delay_ = delay;
    count_inflight_requests_for_peak_queueing_delay_ = count_inflight_requests;
    peak_count_inflight_requests_measurement_period_ = count_inflight_requests;
  } else {
    // This is the logic to update the tracking data.
    // First, updates the pending peak count of in-flight requests if a higher
    // number of in-flight requests is observed.
    // Second, updates the peak queueing delay and the peak count of inflight
    // requests if a higher queueing delay is observed. The new peak queueing
    // delay should be mapped to the peak count of in-flight requests that are
    // observed before within this measurement period.
    peak_count_inflight_requests_measurement_period_ =
        std::max(peak_count_inflight_requests_measurement_period_,
                 count_inflight_requests);

    if (delay > peak_queueing_delay_) {
      // Updates the peak queueing delay and the count of in-flight requests
      // that are responsible for the delay.
      peak_queueing_delay_ = delay;
      count_inflight_requests_for_peak_queueing_delay_ =
          peak_count_inflight_requests_measurement_period_;
    }
  }
}

void NetworkCongestionAnalyzer::FinalizeCurrentMeasurementPeriod() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Does nothing if the peak count of in-flight requests is less than 3.
  if (peak_count_inflight_requests_measurement_period_ < 3)
    return;

  // Exports the tracked mapping data from the current measurement period.
  // Updates the count of in-flight requests that would likely cause a high
  // network queueing delay.
  if (peak_queueing_delay_ >=
      base::TimeDelta::FromMilliseconds(kHighQueueingDelayMsec)) {
    count_inflight_requests_causing_high_delay_ =
        count_inflight_requests_for_peak_queueing_delay_;
  }

  size_t peak_queueing_delay_level =
      ComputePeakQueueingDelayLevel(peak_queueing_delay_);
  DCHECK_GE(kQueueingDelayLevelMaxVal, peak_queueing_delay_level);

  if (peak_queueing_delay_level >= kQueueingDelayLevelMinVal &&
      peak_queueing_delay_level <= kQueueingDelayLevelMaxVal) {
    // Records the count of in-flight requests causing the peak queueing delay
    // within the current measurement period. These samples are bucketized
    // into 10 peak queueing delay levels.
    base::UmaHistogramCounts100(
        "NQE.CongestionAnalyzer.CountInflightRequestsForPeakQueueingDelay."
        "Level" +
            base::NumberToString(peak_queueing_delay_level),
        count_inflight_requests_for_peak_queueing_delay_);
  }

  UpdateRequestsCountAndPeakQueueingDelayMapping();
}

void NetworkCongestionAnalyzer::
    UpdateRequestsCountAndPeakQueueingDelayMapping() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t truncated_count =
      std::min(std::max(count_inflight_requests_for_peak_queueing_delay_,
                        kMinCountOfRequests),
               kMaxCountOfRequests);

  DCHECK_LE(kMinCountOfRequests, truncated_count);
  DCHECK_GE(kMaxCountOfRequests, truncated_count);

  base::Optional<size_t> mapping_score =
      ComputePeakDelayMappingSampleScore(truncated_count, peak_queueing_delay_);
  // Records the score that evaluates the mapping between the count of requests
  // to the peak observed queueing delay. Only records when there are at least
  // 10 samples in the cache. The goal is to eliminate low-score samples because
  // only few requests are in cache. For example, when there are only 5 samples
  // in the cache, a mapping score can be 40 if the new mapping sample violates
  // 3 of them.
  if (count_peak_queueing_delay_mapping_sample_ >= 10 &&
      mapping_score.has_value()) {
    UMA_HISTOGRAM_COUNTS_100(
        "NQE.CongestionAnalyzer.PeakQueueingDelayMappingScore",
        mapping_score.value());
  }

  // Discards the mapping sample if there are at least 10 samples in the cache
  // and its score is less than the threshold. The purpose is to make the
  // majority of cached samples reasonable so that they can be used to evaluate
  // whether a new sample is valid or not.
  if (count_peak_queueing_delay_mapping_sample_ >= 10 &&
      mapping_score.value_or(0) < kMinScoreForValidSamples) {
    return;
  }

  AddRequestsCountAndPeakQueueingDelaySample(truncated_count,
                                             peak_queueing_delay_);
}

base::Optional<size_t>
NetworkCongestionAnalyzer::ComputePeakDelayMappingSampleScore(
    const size_t count_inflight_requests,
    const base::TimeDelta& peak_queueing_delay) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (count_inflight_requests < kMinCountOfRequests ||
      count_inflight_requests > kMaxCountOfRequests) {
    return base::nullopt;
  }

  if (count_peak_queueing_delay_mapping_sample_ < 5)
    return base::nullopt;

  size_t count_positive_samples = 0;
  size_t delay_level = ComputePeakQueueingDelayLevel(peak_queueing_delay);
  DCHECK_LE(1u, delay_level);

  for (const auto& cached_entry : count_inflight_requests_to_queueing_delay_) {
    if (cached_entry.first < count_inflight_requests) {
      for (const auto& it : cached_entry.second) {
        if (it < peak_queueing_delay)
          ++count_positive_samples;
      }
    } else if (cached_entry.first == count_inflight_requests) {
      for (const auto& it : cached_entry.second) {
        size_t it_delay_level = ComputePeakQueueingDelayLevel(it);
        // Two samples are considered near if the difference in queueing delay
        // levels is small. The absolute time difference is small for samples
        // whose queueing delay level is from 1 to 5 (max val=500 msec). The two
        // samples are also considered near if the absolute time difference is
        // less than the 500 msec threshold.
        if (it_delay_level <= delay_level + 1 ||
            it_delay_level >= delay_level - 1 ||
            std::abs(it.InMilliseconds() -
                     peak_queueing_delay.InMilliseconds()) <= 500) {
          ++count_positive_samples;
        }
      }
    } else {
      for (const auto& it : cached_entry.second) {
        if (it > peak_queueing_delay)
          ++count_positive_samples;
      }
    }
  }

  return count_positive_samples * 100 /
         count_peak_queueing_delay_mapping_sample_;
}

void NetworkCongestionAnalyzer::AddRequestsCountAndPeakQueueingDelaySample(
    const size_t count_inflight_requests,
    const base::TimeDelta& peak_queueing_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (count_inflight_requests < kMinCountOfRequests ||
      count_inflight_requests > kMaxCountOfRequests) {
    return;
  }

  auto count_map_it =
      count_inflight_requests_to_queueing_delay_.find(count_inflight_requests);
  // Creates a new entry if no matching record is found.
  if (count_map_it == count_inflight_requests_to_queueing_delay_.end()) {
    count_inflight_requests_to_queueing_delay_[count_inflight_requests]
        .push_front(peak_queueing_delay);
    ++count_peak_queueing_delay_mapping_sample_;
  } else {
    count_map_it->second.push_front(peak_queueing_delay);
    ++count_peak_queueing_delay_mapping_sample_;

    if (count_map_it->second.size() > kMaxCountOfSamplesPerBucket) {
      count_map_it->second.pop_back();
      --count_peak_queueing_delay_mapping_sample_;
    }
  }
}

void NetworkCongestionAnalyzer::set_recent_downlink_throughput_kbps(
    const int32_t downlink_kbps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recent_downlink_throughput_kbps_ = downlink_kbps;
  // Time in msec to transmit one TCP packet (1500 Bytes).
  // |recent_downlink_per_packet_time_ms_| = 1500 * 8 /
  // |recent_downlink_throughput_kbps_|.
  recent_downlink_per_packet_time_ms_ = 12000 / downlink_kbps;
}

}  // namespace internal

}  // namespace nqe

}  // namespace net
