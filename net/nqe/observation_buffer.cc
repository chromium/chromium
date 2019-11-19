// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/observation_buffer.h"

#include <float.h>

#include <algorithm>
#include <utility>

#include "base/macros.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/nqe/weighted_observation.h"

namespace net {

namespace nqe {

namespace internal {
CanonicalStats::CanonicalStats() = default;

CanonicalStats::CanonicalStats(std::map<int32_t, int32_t>& canonical_pcts,
                               int32_t most_recent_val,
                               size_t observation_count)
    : canonical_pcts(canonical_pcts),
      most_recent_val(most_recent_val),
      observation_count(observation_count) {}

CanonicalStats::CanonicalStats(const CanonicalStats& other)
    : canonical_pcts(other.canonical_pcts),
      most_recent_val(other.most_recent_val),
      observation_count(other.observation_count) {}

CanonicalStats::~CanonicalStats() = default;

CanonicalStats& CanonicalStats::operator=(const CanonicalStats& other) =
    default;

ObservationBuffer::ObservationBuffer(
    const NetworkQualityEstimatorParams* params,
    const base::TickClock* tick_clock,
    double weight_multiplier_per_second,
    double weight_multiplier_per_signal_level)
    : params_(params),
      weight_multiplier_per_second_(weight_multiplier_per_second),
      weight_multiplier_per_signal_level_(weight_multiplier_per_signal_level),
      tick_clock_(tick_clock) {
  DCHECK_LT(0u, params_->observation_buffer_size());
  DCHECK_LE(0.0, weight_multiplier_per_second_);
  DCHECK_GE(1.0, weight_multiplier_per_second_);
  DCHECK_LE(0.0, weight_multiplier_per_signal_level_);
  DCHECK_GE(1.0, weight_multiplier_per_signal_level_);
  DCHECK(params_);
  DCHECK(tick_clock_);
}

ObservationBuffer::ObservationBuffer(const ObservationBuffer& other)
    : params_(other.params_),
      weight_multiplier_per_second_(other.weight_multiplier_per_second_),
      weight_multiplier_per_signal_level_(
          other.weight_multiplier_per_signal_level_),
      tick_clock_(other.tick_clock_) {
  DCHECK(other.observations_.empty());
}

ObservationBuffer::~ObservationBuffer() = default;

void ObservationBuffer::AddObservation(const Observation& observation) {
  DCHECK_LE(observations_.size(), params_->observation_buffer_size());

  // Observations must be in the non-decreasing order of the timestamps.
  DCHECK(observations_.empty() ||
         observation.timestamp() >= observations_.back().timestamp());

  DCHECK(observation.signal_strength() == INT32_MIN ||
         (observation.signal_strength() >= 0 &&
          observation.signal_strength() <= 4));

  // Evict the oldest element if the buffer is already full.
  if (observations_.size() == params_->observation_buffer_size())
    observations_.pop_front();

  observations_.push_back(observation);
  DCHECK_LE(observations_.size(), params_->observation_buffer_size());
}

base::Optional<int32_t> ObservationBuffer::GetPercentile(
    base::TimeTicks begin_timestamp,
    int32_t current_signal_strength,
    int percentile,
    size_t* observations_count) const {
  DCHECK(current_signal_strength == INT32_MIN ||
         (current_signal_strength >= 0 && current_signal_strength <= 4));

  // Stores weighted observations in increasing order by value.
  std::vector<WeightedObservation> weighted_observations;

  // Total weight of all observations in |weighted_observations|.
  double total_weight = 0.0;

  ComputeWeightedObservations(begin_timestamp, current_signal_strength,
                              &weighted_observations, &total_weight);

  if (observations_count) {
    // |observations_count| may be null.
    *observations_count = weighted_observations.size();
  }

  if (weighted_observations.empty())
    return base::nullopt;

  double desired_weight = percentile / 100.0 * total_weight;

  double cumulative_weight_seen_so_far = 0.0;
  for (const auto& weighted_observation : weighted_observations) {
    cumulative_weight_seen_so_far += weighted_observation.weight;
    if (cumulative_weight_seen_so_far >= desired_weight)
      return weighted_observation.value;
  }

  // Computation may reach here due to floating point errors. This may happen
  // if |percentile| was 100 (or close to 100), and |desired_weight| was
  // slightly larger than |total_weight| (due to floating point errors).
  // In this case, we return the highest |value| among all observations.
  // This is same as value of the last observation in the sorted vector.
  return weighted_observations.at(weighted_observations.size() - 1).value;
}

std::map<IPHash, CanonicalStats>
ObservationBuffer::GetCanonicalStatsKeyedByHosts(
    const base::TimeTicks& begin_timestamp,
    const std::set<IPHash>& target_hosts) const {
  DCHECK_GE(Capacity(), Size());

  // Computes for all hosts if |target_hosts| is empty. Otherwise, only
  // updates map entries for hosts in |target_hosts| and ignores observations
  // from other hosts.
  bool filter_on_target_hosts = !(target_hosts.empty());

  // Split observations into several subgroups keyed by their corresponding
  // hosts. Skip observations without a host tag. Filter observations based
  // on begin_timestamp. If |target_hosts| is not empty, filter obesrvations
  // that do not belong to any host in the set.
  std::map<IPHash, std::vector<int32_t>> host_keyed_observations;
  for (const auto& observation : observations_) {
    if (!observation.host())
      continue;
    if (observation.timestamp() < begin_timestamp)
      continue;
    // Skip zero values. Transport RTTs can have zero values in the beginning
    // of a connection. It happens because the implementation of TCP's
    // Exponentially Weighted Moving Average (EWMA) starts from zero.
    if (observation.value() < 1)
      continue;

    IPHash host = observation.host().value();
    if (filter_on_target_hosts && target_hosts.find(host) == target_hosts.end())
      continue;

    // Create the map entry if it did not already exist.
    host_keyed_observations.emplace(host, std::vector<int32_t>());
    host_keyed_observations[host].push_back(observation.value());
  }

  std::map<IPHash, CanonicalStats> host_keyed_stats;
  if (host_keyed_observations.empty())
    return host_keyed_stats;

  // Calculate the canonical percentile values for each host.
  for (auto& host_observations : host_keyed_observations) {
    const IPHash& host = host_observations.first;
    auto& observations = host_observations.second;
    host_keyed_stats.emplace(host, CanonicalStats());
    size_t count = observations.size();

    std::sort(observations.begin(), observations.end());
    for (size_t i = 0; i < base::size(kCanonicalPercentiles); ++i) {
      int pct_index = (count - 1) * kCanonicalPercentiles[i] / 100;
      host_keyed_stats[host].canonical_pcts[kCanonicalPercentiles[i]] =
          observations[pct_index];
    }
    host_keyed_stats[host].most_recent_val = observations.back();
    host_keyed_stats[host].observation_count = count;
  }
  return host_keyed_stats;
}

void ObservationBuffer::RemoveObservationsWithSource(
    bool deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_MAX]) {
  base::EraseIf(observations_,
                [deleted_observation_sources](const Observation& observation) {
                  return deleted_observation_sources[static_cast<size_t>(
                      observation.source())];
                });
}

void ObservationBuffer::ComputeWeightedObservations(
    const base::TimeTicks& begin_timestamp,
    int32_t current_signal_strength,
    std::vector<WeightedObservation>* weighted_observations,
    double* total_weight) const {
  DCHECK_GE(Capacity(), Size());

  weighted_observations->clear();
  double total_weight_observations = 0.0;
  base::TimeTicks now = tick_clock_->NowTicks();

  for (const auto& observation : observations_) {
    if (observation.timestamp() < begin_timestamp)
      continue;

    base::TimeDelta time_since_sample_taken = now - observation.timestamp();
    double time_weight =
        pow(weight_multiplier_per_second_, time_since_sample_taken.InSeconds());

    double signal_strength_weight = 1.0;
    if (current_signal_strength >= 0 && observation.signal_strength() >= 0) {
      int32_t signal_strength_weight_diff =
          std::abs(current_signal_strength - observation.signal_strength());
      signal_strength_weight =
          pow(weight_multiplier_per_signal_level_, signal_strength_weight_diff);
    }

    double weight = time_weight * signal_strength_weight;
    weight = base::ClampToRange(weight, DBL_MIN, 1.0);

    weighted_observations->push_back(
        WeightedObservation(observation.value(), weight));
    total_weight_observations += weight;
  }

  // Sort the samples by value in ascending order.
  std::sort(weighted_observations->begin(), weighted_observations->end());
  *total_weight = total_weight_observations;

  DCHECK_LE(0.0, *total_weight);
  DCHECK(weighted_observations->empty() || 0.0 < *total_weight);

  // |weighted_observations| may have a smaller size than |observations_|
  // since the former contains only the observations later than
  // |begin_timestamp|.
  DCHECK_GE(observations_.size(), weighted_observations->size());
}

size_t ObservationBuffer::Capacity() const {
  return params_->observation_buffer_size();
}

}  // namespace internal

}  // namespace nqe

}  // namespace net
