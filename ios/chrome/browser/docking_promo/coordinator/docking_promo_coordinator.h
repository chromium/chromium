// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOCKING_PROMO_COORDINATOR_DOCKING_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_DOCKING_PROMO_COORDINATOR_DOCKING_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/docking_promo_commands.h"

@protocol FirstRunScreenDelegate;
@protocol PromosManagerUIHandler;

// Presents a fullscreen, animated, instructional promo informing users how to
// drag the Chrome icon to their iOS homescreen dock.
@interface DockingPromoCoordinator : ChromeCoordinator <DockingPromoCommands>

/// Initializes a DockingPromoCoordinator. Used for app-launch promo with the
/// Promos Manager & Feature Engagement Tracker.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

/// Initializes a DockingPromoCoordinator with a first run delegate.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate;

// The promos manager UI handler to alert about UI changes.
@property(nonatomic, weak) id<PromosManagerUIHandler> promosUIHandler;

@end

#endif  // IOS_CHROME_BROWSER_DOCKING_PROMO_COORDINATOR_DOCKING_PROMO_COORDINATOR_H_
