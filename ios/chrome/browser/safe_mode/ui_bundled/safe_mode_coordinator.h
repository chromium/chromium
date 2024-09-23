// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_MODE_UI_BUNDLED_SAFE_MODE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SAFE_MODE_UI_BUNDLED_SAFE_MODE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import <UIKit/UIKit.h>

class Browser;
@class SafeModeCoordinator;

@protocol SafeModeCoordinatorDelegate<NSObject>
- (void)coordinatorDidExitSafeMode:(SafeModeCoordinator*)coordinator;
@end

// Coordinator to manage the Safe Mode UI. This should be self-contained.
@interface SafeModeCoordinator : ChromeCoordinator

- (instancetype)initWithWindow:(UIWindow*)window NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate for this coordinator.
@property(nonatomic, weak) id<SafeModeCoordinatorDelegate> delegate;

// If YES, there's a reason to show this coordinator.
+ (BOOL)shouldStart;

@end

#endif  // IOS_CHROME_BROWSER_SAFE_MODE_UI_BUNDLED_SAFE_MODE_COORDINATOR_H_
