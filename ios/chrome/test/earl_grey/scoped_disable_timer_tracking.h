// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_SCOPED_DISABLE_TIMER_TRACKING_H_
#define IOS_CHROME_TEST_EARL_GREY_SCOPED_DISABLE_TIMER_TRACKING_H_

// Helper class to disable EarlGrey's NSTimer tracking.
// TODO(crbug.com/40138424): This is a workaround that should be removed once a
// proper fix lands in EarlGrey.
class ScopedDisableTimerTracking {
 public:
  ScopedDisableTimerTracking();
  ~ScopedDisableTimerTracking();

 private:
  // The original NSTimer max trackable interval.
  double original_interval_;
};

#endif  // IOS_CHROME_TEST_EARL_GREY_SCOPED_DISABLE_TIMER_TRACKING_H_
