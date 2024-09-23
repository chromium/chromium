// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_cadence_estimator.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numeric>
#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "media/base/media_switches.h"

namespace media {

// To prevent oscillation in and out of cadence or between cadence values, we
// require some time to elapse before a cadence switch is accepted.
const int kMinimumCadenceDurationMs = 100;

// The numbers are used to decide whether the current video is variable FPS or
// constant FPS. If ratio of the sample deviation and the render length is
// above |kVariableFPSFactor|, then it is recognized as a variable FPS, and if
// the ratio is below |kConstantFPSFactor|, then it is recognized as a constant
// FPS, and if the ratio is in between the two factors, then we do not change
// previous recognition.
const double kVariableFPSFactor = 0.55;
const double kConstantFPSFactor = 0.45;

// Records the number of cadence changes to UMA.
static void HistogramCadenceChangeCount(int cadence_changes) {
  const int kCadenceChangeMax = 10;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Media.VideoRenderer.CadenceChanges",
                              cadence_changes, 1, kCadenceChangeMax,
                              kCadenceChangeMax);
}

// Construct a Cadence vector, a vector of integers satisfying the following
// conditions:
// 1. Size is |n|.
// 2. Sum of entries is |k|.
// 3. Each entry is in {|k|/|n|, |k|/|n| + 1}.
// 4. Distribution of |k|/|n| and |k|/|n| + 1 is as even as possible.
VideoCadenceEstimator::Cadence ConstructCadence(int k, int n) {
  const int quotient = k / n;
  std::vector<int> output(n, 0);

  // Fill the vector entries with |quotient| or |quotient + 1|, and make sure
  // the two values are distributed as evenly as possible.
  int target_accumulate = 0;
  int actual_accumulate = 0;
  for (int i = 0; i < n; ++i) {
    // After each loop
    // target_accumulate = (i + 1) * k
    // actual_accumulate = \sum_{j = 0}^i {n * V[j]} where V is output vector
    // We want to make actual_accumulate as close to target_accumulate as
    // possible.
    // One exception is that in case k < n, we always want the vector to start
    // with 1 to make sure the first frame is always rendered.
    // (To avoid float calculation, we use scaled version of accumulated count)
    target_accumulate += k;
    const int target_current = target_accumulate - actual_accumulate;
    if ((i == 0 && k < n) || target_current * 2 >= n * (quotient * 2 + 1)) {
      output[i] = quotient + 1;
    } else {
      output[i] = quotient;
    }
    actual_accumulate += output[i] * n;
  }

  return output;
}

VideoCadenceEstimator::Cadence ConstructSimpleCadence(
    double perfect_cadence,
    base::TimeDelta max_acceptable_drift,
    base::TimeDelta* time_until_max_drift) {
  int cadence_value = round(perfect_cadence);
  if (cadence_value < 0) {
    return VideoCadenceEstimator::Cadence();
  }
  if (cadence_value == 0) {
    cadence_value = 1;
  }
  VideoCadenceEstimator::Cadence result = ConstructCadence(cadence_value, 1);

  // The computation for time_until_max_drift below is equivalent to
  // time_until_max_drift = render_interval * (max_acceptable_drift /
  // abs(frame_duration - render_interval))
  const double error = std::fabs(1.0 - perfect_cadence / cadence_value);
  *time_until_max_drift = max_acceptable_drift / error;
  return result;
}

VideoCadenceEstimator::VideoCadenceEstimator(
    base::TimeDelta minimum_time_until_max_drift)
    : cadence_hysteresis_threshold_(
          base::Milliseconds(kMinimumCadenceDurationMs)),
      minimum_time_until_max_drift_(minimum_time_until_max_drift),
      is_variable_frame_rate_(false) {
  Reset();
}

VideoCadenceEstimator::~VideoCadenceEstimator() = default;

void VideoCadenceEstimator::Reset() {
  cadence_.clear();
  pending_cadence_.clear();
  cadence_changes_ = render_intervals_cadence_held_ = 0;
  first_update_call_ = true;
}

bool VideoCadenceEstimator::UpdateCadenceEstimate(
    base::TimeDelta render_interval,
    base::TimeDelta frame_duration,
    base::TimeDelta frame_duration_deviation,
    base::TimeDelta max_acceptable_drift) {
  DCHECK_GT(render_interval, base::TimeDelta());
  DCHECK_GT(frame_duration, base::TimeDelta());

  bool render_interval_changed = false;
  constexpr float kRenderIntervalChangeThreshold = 0.1;
  if (last_render_interval_ > base::TimeDelta() &&
      (last_render_interval_ - render_interval).magnitude() >
          last_render_interval_ * kRenderIntervalChangeThreshold) {
    render_interval_changed = true;
  }
  last_render_interval_ = render_interval;

  if (frame_duration_deviation > kVariableFPSFactor * render_interval) {
    is_variable_frame_rate_ = true;
  } else if (frame_duration_deviation < kConstantFPSFactor * render_interval) {
    is_variable_frame_rate_ = false;
  }

  // Variable FPS detected, turn off Cadence by force.
  if (is_variable_frame_rate_) {
    render_intervals_cadence_held_ = 0;
    if (!cadence_.empty()) {
      cadence_.clear();
      return true;
    }
    return false;
  }

  base::TimeDelta time_until_max_drift;

  // See if we can find a cadence which fits the data.
  Cadence new_cadence =
      CalculateCadence(render_interval, frame_duration, max_acceptable_drift,
                       &time_until_max_drift);

  // If this is the first time UpdateCadenceEstimate() has been called,
  // initialize the histogram with a zero count for cadence changes; this
  // allows us to track the number of playbacks which have cadence at all.
  if (first_update_call_) {
    DCHECK_EQ(cadence_changes_, 0);
    first_update_call_ = false;
    HistogramCadenceChangeCount(0);
  }

  // Update the cadence immediately if the render interval has changed.
  if (render_interval_changed && new_cadence != cadence_) {
    UpdateCadenceInternal(new_cadence, time_until_max_drift);
    return true;
  }

  // If nothing changed, do nothing.
  if (new_cadence == cadence_) {
    // Clear cadence hold to pending values from accumulating incorrectly.
    render_intervals_cadence_held_ = 0;
    return false;
  }

  // Wait until enough render intervals have elapsed before accepting the
  // cadence change.  Prevents oscillation of the cadence selection.
  bool update_pending_cadence = true;
  if (new_cadence == pending_cadence_ ||
      cadence_hysteresis_threshold_ <= render_interval) {
    if (++render_intervals_cadence_held_ * render_interval >=
        cadence_hysteresis_threshold_) {
      UpdateCadenceInternal(new_cadence, time_until_max_drift);
      return true;
    }

    update_pending_cadence = false;
  }

  DVLOG(2) << "Hysteresis prevented cadence switch: "
           << CadenceToString(cadence_) << " -> "
           << CadenceToString(new_cadence);

  if (update_pending_cadence) {
    pending_cadence_.swap(new_cadence);
    render_intervals_cadence_held_ = 1;
  }

  return false;
}

void VideoCadenceEstimator::UpdateCadenceInternal(
    Cadence new_cadence,
    base::TimeDelta time_until_max_drift) {
  DVLOG(1) << "Cadence switch: " << CadenceToString(cadence_) << " -> "
           << CadenceToString(new_cadence)
           << " :: Time until drift exceeded: " << time_until_max_drift;
  cadence_.swap(new_cadence);

  // Note: Because this class is transitively owned by a garbage collected
  // object, WebMediaPlayer, we log cadence changes as they are encountered.
  HistogramCadenceChangeCount(++cadence_changes_);
}

int VideoCadenceEstimator::GetCadenceForFrame(uint64_t frame_number) const {
  DCHECK(has_cadence());
  return cadence_[frame_number % cadence_.size()];
}

bool VideoCadenceEstimator::HasSimpleCadence(
    base::TimeDelta render_interval,
    base::TimeDelta frame_duration,
    base::TimeDelta minimum_time_until_max_drift) {
  if (render_interval.is_zero()) {
    return false;
  }
  const double perfect_cadence = frame_duration / render_interval;
  base::TimeDelta time_until_max_drift;
  auto cadence = ConstructSimpleCadence(perfect_cadence, render_interval,
                                        &time_until_max_drift);
  return !cadence.empty() &&
         time_until_max_drift >= minimum_time_until_max_drift;
}

VideoCadenceEstimator::Cadence VideoCadenceEstimator::CalculateCadence(
    base::TimeDelta render_interval,
    base::TimeDelta frame_duration,
    base::TimeDelta max_acceptable_drift,
    base::TimeDelta* time_until_max_drift) const {
  // The perfect cadence is the number of render intervals per frame.
  const double perfect_cadence = frame_duration / render_interval;

  // This case is very simple, just return a single frame cadence, because it
  // is impossible for us to accumulate drift as large as max_acceptable_drift
  // within minimum_time_until_max_drift.
  if (max_acceptable_drift >= minimum_time_until_max_drift_) {
    return ConstructSimpleCadence(perfect_cadence, max_acceptable_drift,
                                  time_until_max_drift);
  }

  // We want to construct a cadence pattern to approximate the perfect cadence
  // while ensuring error doesn't accumulate too quickly.
  const double drift_ratio =
      max_acceptable_drift / minimum_time_until_max_drift_;
  const double minimum_acceptable_cadence =
      perfect_cadence / (1.0 + drift_ratio);
  const double maximum_acceptable_cadence =
      perfect_cadence / (1.0 - drift_ratio);

  // We've arbitrarily chosen the maximum allowable cadence length as 5. It's
  // proven sufficient to support most standard frame and render rates, while
  // being small enough that small frame and render timing errors don't render
  // it useless.
  const int kMaxCadenceSize = 5;

  double best_error = 0;
  int best_n = 0;
  int best_k = 0;
  for (int n = 1; n <= kMaxCadenceSize; ++n) {
    // A cadence pattern only exists if there exists an integer K such that K/N
    // is between |minimum_acceptable_cadence| and |maximum_acceptable_cadence|.
    // The best pattern is the one with the smallest error over time relative to
    // the |perfect_cadence|.
    if (std::floor(minimum_acceptable_cadence * n) <
        std::floor(maximum_acceptable_cadence * n)) {
      const int k = round(perfect_cadence * n);

      const double error = std::fabs(1.0 - perfect_cadence * n / k);

      // Prefer the shorter cadence pattern unless a longer one "significantly"
      // reduces the error.
      if (!best_n || error < best_error * 0.99) {
        best_error = error;
        best_k = k;
        best_n = n;
      }
    }
  }

  if (!best_n) {
    Cadence cadence = ConstructSimpleCadence(
        perfect_cadence, max_acceptable_drift, time_until_max_drift);
    if (!cadence.empty() &&
        *time_until_max_drift >= minimum_time_until_max_drift_) {
      return cadence;
    }
    return Cadence();
  }

  // If we've found a solution.
  Cadence best_result = ConstructCadence(best_k, best_n);
  *time_until_max_drift = max_acceptable_drift / best_error;

  return best_result;
}

std::string VideoCadenceEstimator::CadenceToString(
    const Cadence& cadence) const {
  if (cadence.empty())
    return std::string("[]");

  std::ostringstream os;
  os << "[";
  std::copy(cadence.begin(), cadence.end() - 1,
            std::ostream_iterator<int>(os, ":"));
  os << cadence.back() << "]";
  return os.str();
}

double VideoCadenceEstimator::avg_cadence_for_testing() const {
  if (!has_cadence())
    return 0.0;

  int sum = std::accumulate(begin(cadence_), end(cadence_), 0);
  return static_cast<double>(sum) / cadence_.size();
}

}  // namespace media
