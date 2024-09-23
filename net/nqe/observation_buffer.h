// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_OBSERVATION_BUFFER_H_
#define NET_NQE_OBSERVATION_BUFFER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "net/base/net_export.h"
#include "net/nqe/network_quality_estimator_util.h"
#include "net/nqe/network_quality_observation.h"
#include "net/nqe/network_quality_observation_source.h"

namespace base {

class TimeTicks;

}  // namespace base

namespace net {

class NetworkQualityEstimatorParams;

namespace nqe::internal {

struct WeightedObservation;

// Stores observations sorted by time and provides utility functions for
// computing weighted and non-weighted summary statistics.
class NET_EXPORT_PRIVATE ObservationBuffer {
 public:
  ObservationBuffer(const NetworkQualityEstimatorParams* params,
                    const base::TickClock* tick_clock,
                    double weight_multiplier_per_second,
                    double weight_multiplier_per_signal_level);

  //  This constructor does not copy the |observations_| from |other| to |this|.
  //  As such, this constructor should only be called before adding any
  //  observations to |other|.
  ObservationBuffer(const ObservationBuffer& other);

  ObservationBuffer& operator=(const ObservationBuffer&) = delete;

  ~ObservationBuffer();

  // Adds |observation| to the buffer. The oldest observation in the buffer
  // will be evicted to make room if the buffer is already full.
  // In that case, an evicted observation will be returned.
  std::optional<Observation> AddObservation(const Observation& observation);

  // Returns the number of observations in this buffer.
  size_t Size() const { return static_cast<size_t>(observations_.size()); }

  // Returns the capacity of this buffer.
  size_t Capacity() const;

  // Clears the observations stored in this buffer.
  void Clear() { observations_.clear(); }

  // Returns true iff the |percentile| value of the observations in this
  // buffer is available. Sets |result| to the computed |percentile|
  // value of all observations made on or after |begin_timestamp|. If the
  // value is unavailable, false is returned and |result| is not modified.
  // Percentile value is unavailable if all the values in observation buffer are
  // older than |begin_timestamp|. |current_signal_strength| is the current
  // signal strength. |result| must not be null. If |observations_count| is not
  // null, then it is set to the number of observations that were available
  // in the observation buffer for computing the percentile.
  std::optional<int32_t> GetPercentile(base::TimeTicks begin_timestamp,
                                       int32_t current_signal_strength,
                                       int percentile,
                                       size_t* observations_count) const;

  void SetTickClockForTesting(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

  // Removes all observations from the buffer whose corresponding entry in
  // |deleted_observation_sources| is set to true. For example, if index 1 and
  // 3 in |deleted_observation_sources| are set to true, then all observations
  // in the buffer that have source set to either 1 or 3 would be removed.
  void RemoveObservationsWithSource(
      bool deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_MAX]);

 private:
  // Computes the weighted observations and stores them in
  // |weighted_observations| sorted by ascending |WeightedObservation.value|.
  // Only the observations with timestamp later than |begin_timestamp| are
  // considered. |current_signal_strength| is the current signal strength
  // when the observation was taken. This method also sets |total_weight| to
  // the total weight of all observations. Should be called only when there is
  // at least one observation in the buffer.
  void ComputeWeightedObservations(
      const base::TimeTicks& begin_timestamp,
      int32_t current_signal_strength,
      std::vector<WeightedObservation>* weighted_observations,
      double* total_weight) const;

  raw_ptr<const NetworkQualityEstimatorParams> params_;

  // Holds observations sorted by time, with the oldest observation at the
  // front of the queue.
  base::circular_deque<Observation> observations_;

  // The factor by which the weight of an observation reduces every second.
  // For example, if an observation is 6 seconds old, its weight would be:
  //     weight_multiplier_per_second_ ^ 6
  // Calculated from |kHalfLifeSeconds| by solving the following equation:
  //     weight_multiplier_per_second_ ^ kHalfLifeSeconds = 0.5
  const double weight_multiplier_per_second_;

  // The factor by which the weight of an observation reduces for every unit
  // difference in the current signal strength, and the signal strength at
  // which the observation was taken.
  // For example, if the observation was taken at 1 unit, and current signal
  // strength is 4 units, the weight of the observation would be:
  // |weight_multiplier_per_signal_level_| ^ 3.
  const double weight_multiplier_per_signal_level_;

  raw_ptr<const base::TickClock> tick_clock_;
};

}  // namespace nqe::internal

}  // namespace net

#endif  // NET_NQE_OBSERVATION_BUFFER_H_
