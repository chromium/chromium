// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FEEDBACK_SIGNAL_ACCUMULATOR_H_
#define MEDIA_BASE_FEEDBACK_SIGNAL_ACCUMULATOR_H_

#include <algorithm>
#include <cmath>
#include <ostream>

#include "base/time/time.h"

namespace media {

// Utility class for maintaining an exponentially-decaying average of feedback
// signal values whose updates occur at undetermined, possibly irregular time
// intervals.
//
// Feedback signals can be made by multiple sources.  Meaning, there can be
// several values provided for the same timestamp.  In this case, the greatest
// value is retained and used to re-compute the average.  Therefore, the values
// provided to this class' methods should be appropriately translated with this
// in mind.  For example, an "fraction available" metric should be translated
// into a "fraction utilized" one.
//
// Usage note: Reset() must be called at least once before the first call to
// Update().
//
// This template class supports data points that are timestamped using either
// |base::TimeDelta| or |base::TimeTicks|.
template <typename TimeType>
class FeedbackSignalAccumulator {
 public:
  // |half_life| is the amount of time that must pass between two data points to
  // move the accumulated average value halfway in-between.  Example: If
  // |half_life| is one second, then calling Reset(0.0, t=0s) and then
  // Update(1.0, t=1s) will result in an accumulated average value of 0.5.
  explicit FeedbackSignalAccumulator(base::TimeDelta half_life)
      : half_life_(half_life), average_(NAN) {
    DCHECK(half_life_.is_positive());
  }

  // Erase all memory of historical values, re-starting with the given
  // |starting_value|.
  void Reset(double starting_value, TimeType timestamp) {
    average_ = update_value_ = prior_average_ = starting_value;
    reset_time_ = update_time_ = prior_update_time_ = timestamp;
  }

  TimeType reset_time() const { return reset_time_; }

  // Apply the given |value|, which was observed at the given |timestamp|, to
  // the accumulated average.  If the timestamp is in chronological order, the
  // update succeeds and this method returns true.  Otherwise the update has no
  // effect and false is returned.  If there are two or more updates at the same
  // |timestamp|, only the one with the greatest value will be accounted for
  // (see class comments for elaboration).
  bool Update(double value, TimeType timestamp) {
    DCHECK(!std::isnan(average_)) << "Reset() must be called once.";

    if (timestamp < update_time_) {
      return false;  // Not in chronological order.
    } else if (timestamp == update_time_) {
      if (timestamp == reset_time_) {
        // Edge case: Multiple updates at reset timestamp.
        average_ = update_value_ = prior_average_ =
            std::max(value, update_value_);
        return true;
      }
      if (value <= update_value_)
        return true;
      update_value_ = value;
    } else {
      prior_average_ = average_;
      prior_update_time_ = update_time_;
      update_value_ = value;
      update_time_ = timestamp;
    }

    const double elapsed_us = static_cast<double>(
        (update_time_ - prior_update_time_).InMicroseconds());
    const double weight =
        elapsed_us / (elapsed_us + half_life_.InMicroseconds());
    average_ = weight * update_value_ + (1.0 - weight) * prior_average_;
    DCHECK(std::isfinite(average_));

    return true;
  }

  TimeType update_time() const { return update_time_; }

  // Returns the current accumulated average value.
  double current() const { return average_; }

 private:
  // In conjunction with the |update_time_| and |prior_update_time_|, this is
  // used to compute the weight of the current update value versus the prior
  // accumulated average.
  const base::TimeDelta half_life_;

  TimeType reset_time_;   // |timestamp| passed in last call to Reset().
  double average_;        // Current accumulated average.
  double update_value_;   // Latest |value| accepted by Update().
  TimeType update_time_;  // Latest |timestamp| accepted by Update().
  double prior_average_;  // Accumulated average before last call to Update().
  TimeType prior_update_time_;  // |timestamp| in prior call to Update().
};

}  // namespace media

#endif  // MEDIA_BASE_FEEDBACK_SIGNAL_ACCUMULATOR_H_
