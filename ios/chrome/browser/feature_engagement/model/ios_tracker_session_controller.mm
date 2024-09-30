// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/model/ios_tracker_session_controller.h"
#import "base/time/clock.h"

namespace {
const base::TimeDelta kMaxSessionDuration = base::Minutes(60);
}

IOSTrackerSessionController::IOSTrackerSessionController(
    raw_ptr<base::Clock> clock)
    : clock_(clock),
      session_start_time_(clock_->Now()),
      max_session_duration_(kMaxSessionDuration) {}

IOSTrackerSessionController::~IOSTrackerSessionController() = default;

bool IOSTrackerSessionController::ShouldResetSession() {
  const base::TimeDelta time_since_session_start =
      clock_->Now() - session_start_time_;
  if (time_since_session_start > max_session_duration_) {
    session_start_time_ = clock_->Now();
    return true;
  }
  return false;
}
