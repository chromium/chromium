// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_H_
#define IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_H_

#import <Foundation/Foundation.h>

#import "base/test/scoped_feature_list.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"

@class AppLaunchManager;


// Protocol that test cases can implement to be notified by AppLaunchManager.
@protocol AppLaunchManagerObserver
@optional
// Called when app gets relaunched (due to force restart, or changing the
// arguments).
// |runResets| indicates whether to reset all app status and provide clean test
// case setups.
- (void)appLaunchManagerDidRelaunchApp:(AppLaunchManager*)appLaunchManager
                             runResets:(BOOL)runResets;
@end

// Provides control of the single application-under-test to EarlGrey 2 tests.
@interface AppLaunchManager : NSObject

// True if the app has been successfully launched.
@property(readonly) BOOL appIsLaunched;

// True if the app is currently running (in the foreground).
@property(readonly) BOOL appIsRunning;

// Returns the singleton instance of this class.
+ (AppLaunchManager*)sharedManager;

- (instancetype)init NS_UNAVAILABLE;

// Makes sure the app has been started with the |configuration|. Provides
// different ways of killing and resetting app during relaunch. In EG2, the app
// will be launched from scratch if:
// * The app is not running, or
// * The app is currently running with a different feature set, or
// * |relaunchPolicy| forces a relaunch.
// Otherwise, the app will be activated instead of (re)launched.
// Will wait until app is activated or launched, and fail the test if it
// fails to do so.
// |configuration| sets features, variations, arguments and relaunch manners.
// If you're trying to call this method in |-setUp()|, please specify an
// |AppLaunchConfiguration| in |-appConfigurationForTestCase()| instead for
// better efficiency.
- (void)ensureAppLaunchedWithConfiguration:
    (AppLaunchConfiguration)configuration;

// DEPRECATED. Use |ensureAppLaunchedWithConfiguration:| instead.
- (void)ensureAppLaunchedWithFeaturesEnabled:
            (std::vector<base::test::FeatureRef>)featuresEnabled
                                    disabled:
                                        (std::vector<base::test::FeatureRef>)
                                            featuresDisabled
                              relaunchPolicy:(RelaunchPolicy)relaunchPolicy;

// Moves app to background and then moves it back. In EG1, this method is a
// no-op.
- (void)backgroundAndForegroundApp;

// Adds an observer for AppLaunchManager.
- (void)addObserver:(id<AppLaunchManagerObserver>)observer;

// Removes an observer for AppLaunchManager.
- (void)removeObserver:(id<AppLaunchManagerObserver>)observer;

@end

#endif  // IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_H_
