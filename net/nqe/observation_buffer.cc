// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/nqe/observation_buffer.h"

#include <float.h>

#include <algorithm>
#include <utility>

#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/nqe/weighted_observation.h"

namespace net::nqe::internal {

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

std::optional<Observation> ObservationBuffer::AddObservation(
    const Observation& observation) {
  DCHECK_LE(observations_.size(), params_->observation_buffer_size());

  // Observations must be in the non-decreasing order of the timestamps.
  DCHECK(observations_.empty() ||
         observation.timestamp() >= observations_.back().timestamp());

  DCHECK(observation.signal_strength() == INT32_MIN ||
         (observation.signal_strength() >= 0 &&
          observation.signal_strength() <= 4));

  std::optional<Observation> evicted_observation;
  // Evict the oldest element if the buffer is already full.
  if (observations_.size() == params_->observation_buffer_size()) {
    evicted_observation = observations_.front();
    observations_.pop_front();
  }

  observations_.push_back(observation);
  DCHECK_LE(observations_.size(), params_->observation_buffer_size());
  return evicted_observation;
}

std::optional<int32_t> ObservationBuffer::GetPercentile(
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
    return std::nullopt;

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
    weight = std::clamp(weight, DBL_MIN, 1.0);

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

}  // namespace net::nqe::internal
