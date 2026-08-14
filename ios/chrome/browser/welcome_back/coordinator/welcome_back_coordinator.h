// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WELCOME_BACK_COORDINATOR_WELCOME_BACK_COORDINATOR_H_
#define IOS_CHROME_BROWSER_WELCOME_BACK_COORDINATOR_WELCOME_BACK_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol PromosManagerUIHandler;

// Coordinator to present the Welcome Back screen.
@interface WelcomeBackCoordinator : ChromeCoordinator

// Initializes the coordinator with the base view controller, browser, and the
// promos manager UI handler.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                           promosUIHandler:
                               (id<PromosManagerUIHandler>)promosUIHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_WELCOME_BACK_COORDINATOR_WELCOME_BACK_COORDINATOR_H_
