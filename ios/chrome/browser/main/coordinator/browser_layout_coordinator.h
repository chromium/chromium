// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_COORDINATOR_BROWSER_LAYOUT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_MAIN_COORDINATOR_BROWSER_LAYOUT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class BrowserLayoutViewController;
#import "ios/chrome/browser/main/ui/browser_layout_consumer.h"

// Coordinator that manages the BrowserLayoutViewController.
@interface BrowserLayoutCoordinator : ChromeCoordinator

// The view controller managed by this coordinator.
@property(nonatomic, strong, readonly)
    BrowserLayoutViewController* viewController;

// The BrowserViewController being displayed.
@property(nonatomic, weak)
    UIViewController<BrowserLayoutConsumer>* browserViewController;

// Initializes this Coordinator with its `browser` and a nil base view
// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_MAIN_COORDINATOR_BROWSER_LAYOUT_COORDINATOR_H_
