// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CHROME_ROOT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CHROME_ROOT_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// A coordinator specialization for the case where the coordinator is
// creating and managing the root view controller for a UIWindow.

@interface ChromeRootCoordinator : ChromeCoordinator

- (instancetype)initWithWindow:(UIWindow*)window NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(weak, nonatomic, readonly) UIWindow* window;

@end

#endif  // IOS_CHROME_BROWSER_CHROME_ROOT_COORDINATOR_H_
