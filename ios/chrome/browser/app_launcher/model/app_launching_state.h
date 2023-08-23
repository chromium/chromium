// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHING_STATE_H_
#define IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHING_STATE_H_

#import <Foundation/Foundation.h>

// Default maximum allowed seconds between 2 launches to be considered
// consecutive.
extern const double kDefaultMaxSecondsBetweenConsecutiveExternalAppLaunches;

// AppLaunchingState is a state for a single external application
// represented by timestamp of the last time the app was launched, and the
// number of consecutive launches. Launches are considered consecutive when the
// time difference between them are less than
// `kDefaultMaxSecondsBetweenConsecutiveExternalAppLaunches`.
// The AppLaunchingState doesn't know the source URL nor the destination
// URL, the AppLauncherAbuseDetector object will have an
// AppLaunchingState object for  each sourceURL/Application Scheme pair.
@interface AppLaunchingState : NSObject
// The max allowed seconds between 2 launches to be considered consecutive.
@property(class, nonatomic) double maxSecondsBetweenConsecutiveLaunches;
// Counts the number of current consecutive launches for the app.
@property(nonatomic, readonly) int consecutiveLaunchesCount;
// YES if the user blocked this app from launching for the current session.
@property(nonatomic, getter=isAppLaunchingBlocked) BOOL appLaunchingBlocked;

// Updates the state with one more try to open the application, this method will
// check the last time the application was opened and the number of times it was
// opened consecutively then update the state.
- (void)updateWithLaunchRequest;
@end

#endif  // IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHING_STATE_H_
