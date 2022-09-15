// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/metrics/session_timer.h"

#include "base/metrics/histogram_functions.h"

namespace content {

SessionTimer::SessionTimer(char const* histogram_name,
                           base::TimeDelta gap_time,
                           base::TimeDelta minimum_duration)
    : histogram_name_(histogram_name),
      maximum_session_gap_time_(gap_time),
      minimum_duration_(minimum_duration) {}

SessionTimer::~SessionTimer() {
  StopSession(false, base::Time::Now());
}

void SessionTimer::StartSession(base::Time start_time) {
  // If the new start time is within the minimum session gap time from the
  // last stop, continue the previous session. Otherwise, start a new session,
  // sending the event for the last session.
  if (!stop_time_.is_null() &&
      start_time - stop_time_ <= maximum_session_gap_time_) {
    // Mark the previous segment as non-continuable, sending data and clearing
    // state.
    StopSession(false, stop_time_);
  }

  start_time_ = start_time;
}

void SessionTimer::StopSession(bool continuable, base::Time stop_time) {
  // first accumulate time from this segment of the session
  base::TimeDelta segment_duration =
      (start_time_.is_null() ? base::TimeDelta() : stop_time - start_time_);
  if (!segment_duration.is_zero() && segment_duration > minimum_duration_) {
    accumulated_time_ = accumulated_time_ + segment_duration;
  }

  if (continuable) {
    // if we are continuable, accumulate the current segment to the session,
    // and set stop_time_ so we may continue later
    accumulated_time_ = stop_time - start_time_ + accumulated_time_;
    stop_time_ = stop_time;
    start_time_ = base::Time();
  } else {
    // send the histogram now if we aren't continuable, clearing segment state
    SendAccumulatedSessionTime();

    // clear out start/stop/accumulated time
    start_time_ = base::Time();
    stop_time_ = base::Time();
    accumulated_time_ = base::TimeDelta();
  }
}

void SessionTimer::SendAccumulatedSessionTime() {
  if (!accumulated_time_.is_zero()) {
    base::UmaHistogramCustomTimes(histogram_name_, accumulated_time_,
                                  base::TimeDelta(), base::Hours(5), 100);
  }
}

}  // namespace content
