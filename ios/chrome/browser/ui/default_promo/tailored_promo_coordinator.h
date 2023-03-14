// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_TAILORED_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_TAILORED_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "ios/chrome/browser/ui/default_promo/default_browser_promo_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"

// Coordinator for a fullscreen modal default browser promo.
@interface TailoredPromoCoordinator : ChromeCoordinator

// Creates a coordinator that uses `viewController`, `browser` and `type`.
// Only DefaultPromoTypeStaySafe, DefaultPromoTypeMadeForIOS and
// DefaultPromoTypeAllTabs are supported.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      type:(DefaultPromoType)type
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Handler for all actions of this coordinator.
@property(nonatomic, weak) id<DefaultBrowserPromoCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_TAILORED_PROMO_COORDINATOR_H_
