// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_WIDGET_PROMO_INSTRUCTIONS_WIDGET_PROMO_INSTRUCTIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_WIDGET_PROMO_INSTRUCTIONS_WIDGET_PROMO_INSTRUCTIONS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class WidgetPromoInstructionsCoordinator;

// Delegate for WidgetPromoInstructionsCoordinator.
@protocol WidgetPromoInstructionsCoordinatorDelegate

// Tells the delegate that the view controller was removed from the navigation
// controller.
- (void)widgetPromoInstructionsCoordinatorDidRemove:
    (WidgetPromoInstructionsCoordinator*)coordinator;

@end

// Coordinator to present the widget promo instructions screen.
@interface WidgetPromoInstructionsCoordinator : ChromeCoordinator

// Initiates a WidgetPromoInstructionsCoordinator with `navigationController`,
// `browser`.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, weak) id<WidgetPromoInstructionsCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_WIDGET_PROMO_INSTRUCTIONS_WIDGET_PROMO_INSTRUCTIONS_COORDINATOR_H_
