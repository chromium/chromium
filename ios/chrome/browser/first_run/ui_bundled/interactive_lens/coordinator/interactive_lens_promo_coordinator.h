// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERACTIVE_LENS_COORDINATOR_INTERACTIVE_LENS_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERACTIVE_LENS_COORDINATOR_INTERACTIVE_LENS_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol FirstRunScreenDelegate;

// Coordinator to present the Interactive Lens Promo.
@interface InteractiveLensPromoCoordinator : ChromeCoordinator

// Delegate for communicating with the FirstRunCoordinator.
@property(nonatomic, weak) id<FirstRunScreenDelegate> firstRunDelegate;

// Initializes a InteractiveLensPromoCoordinator with `navigationController` and
// `browser`.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERACTIVE_LENS_COORDINATOR_INTERACTIVE_LENS_PROMO_COORDINATOR_H_
