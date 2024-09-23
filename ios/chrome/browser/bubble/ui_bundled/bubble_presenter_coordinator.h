// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_PRESENTER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_PRESENTER_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol BubblePresenterDelegate;

// Coordinator to present in-product help bubble.
@interface BubblePresenterCoordinator : ChromeCoordinator

// The base view controller for this coordinator. Exposing this property since
// the `BubblePresenterCoordinator` is initiated before the browser coordinator
// initiates the base view controller, and is unable to pass it to the
// initializer of this coordinator.
@property(nonatomic, weak, readwrite) UIViewController* baseViewController;

// Delegate object for the internal BubblePresenter.
@property(nonatomic, weak) id<BubblePresenterDelegate> bubblePresenterDelegate;

// Initializes this Coordinator with its `browser` and a nil base view
// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_PRESENTER_COORDINATOR_H_
