// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_ANIMATED_LENS_COORDINATOR_ANIMATED_LENS_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_ANIMATED_LENS_COORDINATOR_ANIMATED_LENS_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@protocol FirstRunScreenDelegate;

// Coordinator to present the Animated Lens Promo.
@interface AnimatedLensPromoCoordinator
    : ChromeCoordinator <PromoStyleViewControllerDelegate>

// Delegate for communicating with the FirstRunCoordinator.
@property(nonatomic, weak) id<FirstRunScreenDelegate> firstRunDelegate;

// Initializes an AnimatedLensPromoCoordinator with `navigationController` and
// `browser`.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_ANIMATED_LENS_COORDINATOR_ANIMATED_LENS_PROMO_COORDINATOR_H_
