// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_FORCED_SIGNIN_FORCED_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_FORCED_SIGNIN_FORCED_SIGNIN_COORDINATOR_H_

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

@class ScreenProvider;

// Coordinator to present first run screens.
@interface ForcedSigninCoordinator : SigninCoordinator

// Initiate the coordinator.
// `browser` used for authentication. It must not be off the record (incognito).
// `screenProvider` helps decide which screen to show.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            screenProvider:(ScreenProvider*)screenProvider
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_FORCED_SIGNIN_FORCED_SIGNIN_COORDINATOR_H_
