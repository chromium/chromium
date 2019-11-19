// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_H_
#define IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_H_

#import <Foundation/Foundation.h>

#include <vector>

namespace base {
struct Feature;
}

@class AppLaunchManager;

// Protocol that test cases can implement to be notified by AppLaunchManager.
@protocol AppLaunchManagerObserver
@optional
// Called when app gets relaunched (due to force restart, or changing the
// arguments).
- (void)appLaunchManagerDidRelaunchApp:(AppLaunchManager*)appLaunchManager;
@end

// Provides control of the single application-under-test to EarlGrey 2 tests.
@interface AppLaunchManager : NSObject

// Returns the singleton instance of this class.
+ (AppLaunchManager*)sharedManager;

- (instancetype)init NS_UNAVAILABLE;

// Makes sure the app has been started with the appropriate features
// enabled and disabled. In EG2, the app will be launched from scratch if:
// * The app is not running
// * The app is currently running with a different feature set.
// * |forceRestart| is YES
// Otherwise, the app will be activated instead of (re)launched.
// Will wait until app is activated or launched, and fail the test if it
// fails to do so.
// In EG1, this method is a no-op.
- (void)ensureAppLaunchedWithFeaturesEnabled:
            (const std::vector<base::Feature>&)featuresEnabled
                                    disabled:(const std::vector<base::Feature>&)
                                                 featuresDisabled
                                forceRestart:(BOOL)forceRestart;

// Adds an observer for AppLaunchManager.
- (void)addObserver:(id<AppLaunchManagerObserver>)observer;

// Removes an observer for AppLaunchManager.
- (void)removeObserver:(id<AppLaunchManagerObserver>)observer;

@end

#endif  // IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_H_
