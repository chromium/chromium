// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_IOS_TRACKER_SESSION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_IOS_TRACKER_SESSION_CONTROLLER_H_

#import "components/feature_engagement/public/session_controller.h"

#import "base/feature_list.h"
#import "base/time/clock.h"
#import "base/time/default_clock.h"
#import "base/time/time.h"

// An implementation with iOS-specific logic of managing the lifecycle of the
// SessionController. Injected into the feature engagement tracker replacing
// the default implementation.
class IOSTrackerSessionController
    : public feature_engagement::SessionController {
 public:
  IOSTrackerSessionController(
      raw_ptr<base::Clock> clock = base::DefaultClock::GetInstance());
  ~IOSTrackerSessionController() override;

  bool ShouldResetSession() override;

 private:
  raw_ptr<base::Clock> clock_;
  base::Time session_start_time_;
  const base::TimeDelta max_session_duration_;
};

#endif  // IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_IOS_TRACKER_SESSION_CONTROLLER_H_
