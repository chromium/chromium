// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_CONTROLLER_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/app_lifetime_observer.h"

@class AppState;
@class MetricsMediator;
@class ProfileState;

// The controller for a single Profile, owned by MainController. Owns all
// the top-level UI controllers for this Profile.
@interface ProfileController : NSObject <AppLifetimeObserver>

// Contains information about the Profile state.
@property(nonatomic, readonly) ProfileState* state;

// Used to check and update the metrics according to user preferences.
@property(nonatomic, readonly) MetricsMediator* metricsMediator;

// The designated initializer.
- (instancetype)initWithAppState:(AppState*)appState
                 metricsMediator:(MetricsMediator*)metricsMediator
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Informs the ProfileController that it will be destroyed and that it
// should perform any cleanup required.
- (void)shutdown;

@end

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_CONTROLLER_H_
