// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/model/app_launching_state.h"

const double kDefaultMaxSecondsBetweenConsecutiveExternalAppLaunches = 30.0;

@implementation AppLaunchingState {
  // Timestamp of the last app launch request.
  NSDate* _lastAppLaunchTime;
}
static double _maxSecondsBetweenConsecutiveLaunches =
    kDefaultMaxSecondsBetweenConsecutiveExternalAppLaunches;
@synthesize consecutiveLaunchesCount = _consecutiveLaunchesCount;
@synthesize appLaunchingBlocked = _appLaunchingBlocked;

+ (double)maxSecondsBetweenConsecutiveLaunches {
  return _maxSecondsBetweenConsecutiveLaunches;
}

+ (void)setMaxSecondsBetweenConsecutiveLaunches:(double)seconds {
  _maxSecondsBetweenConsecutiveLaunches = seconds;
}

- (void)updateWithLaunchRequest {
  if (_appLaunchingBlocked) {
    return;
  }
  if (!_lastAppLaunchTime ||
      -_lastAppLaunchTime.timeIntervalSinceNow >
          [[self class] maxSecondsBetweenConsecutiveLaunches]) {
    _consecutiveLaunchesCount = 1;
  } else {
    _consecutiveLaunchesCount++;
  }
  _lastAppLaunchTime = [NSDate date];
}

@end
