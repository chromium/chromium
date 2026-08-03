// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/root_coordinator/root_coordinator.h"

class Browser;

// Coordinator for the app bar, the bar at the bottom of the screen on narrow
// form factors.
@interface AppBarCoordinator : RootCoordinator

// View controller for the app bar.
@property(nonatomic, strong, readonly) UIViewController* viewController;

// Initializes the coordinator with the given browsers.
- (instancetype)initWithRegularBrowser:(Browser*)regularBrowser
                      incognitoBrowser:(Browser*)incognitoBrowser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Resets the incognito browser, when it is recreated.
- (void)setIncognitoBrowser:(Browser*)incognitoBrowser;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_COORDINATOR_H_
