// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_LAUNCHER_FAKE_APP_LAUNCHER_ABUSE_DETECTOR_H_
#define IOS_CHROME_BROWSER_APP_LAUNCHER_FAKE_APP_LAUNCHER_ABUSE_DETECTOR_H_

#import "ios/chrome/browser/app_launcher/app_launcher_abuse_detector.h"

// An AppLauncherAbuseDetector for testing.
@interface FakeAppLauncherAbuseDetector : AppLauncherAbuseDetector

// The policy returned by |-launchPolicyforURL:fromSourcePageURL:|.  Default
// value is ExternalAppLaunchPolicyAllow.
@property(nonatomic, assign) ExternalAppLaunchPolicy policy;

@end

#endif  // IOS_CHROME_BROWSER_APP_LAUNCHER_FAKE_APP_LAUNCHER_ABUSE_DETECTOR_H_
