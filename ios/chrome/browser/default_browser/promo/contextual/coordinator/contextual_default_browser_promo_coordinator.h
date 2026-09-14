// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_COORDINATOR_CONTEXTUAL_DEFAULT_BROWSER_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_COORDINATOR_CONTEXTUAL_DEFAULT_BROWSER_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/default_browser/promo/contextual/public/contextual_default_browser_promo_constants.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator that manages the presentation and lifecycle of the contextual
// default browser promo.
@interface ContextualDefaultBrowserPromoCoordinator : ChromeCoordinator

// Initializes the coordinator with a specific `promoType`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                 promoType:(ContextualDefaultBrowserPromoType)
                                               promoType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_COORDINATOR_CONTEXTUAL_DEFAULT_BROWSER_PROMO_COORDINATOR_H_
