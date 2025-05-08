// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_FULLSCREEN_SIGNIN_COORDINATOR_FULLSCREEN_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_FULLSCREEN_SIGNIN_COORDINATOR_FULLSCREEN_SIGNIN_COORDINATOR_H_

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/animated_coordinator.h"

@class ScreenProvider;

// Main coordinator that manages the overall fullscreen sign-in flow.
// It contains a child coordinator, FullscreenSigninScreenCoordinator,
// which is responsible for presenting the actual fullscreen sign-in screen.
@interface FullscreenSigninCoordinator : SigninCoordinator

// Initiate the coordinator.
// `browser` used for authentication. It must not be off the record (incognito).
// `screenProvider` helps decide which screen to show.
- (instancetype)
           initWithBaseViewController:(UIViewController*)viewController
                              browser:(Browser*)browser
                       screenProvider:(ScreenProvider*)screenProvider
                         contextStyle:(SigninContextStyle)contextStyle
                          accessPoint:(signin_metrics::AccessPoint)accessPoint
    changeProfileContinuationProvider:(const ChangeProfileContinuationProvider&)
                                          changeProfileContinuationProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_FULLSCREEN_SIGNIN_COORDINATOR_FULLSCREEN_SIGNIN_COORDINATOR_H_
