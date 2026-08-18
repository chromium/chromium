// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_COORDINATOR_H_
#define IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class BackendPromoCustomUIParams;
@protocol BackendPromoCustomUICoordinatorDelegate;

// Coordinator for presenting the backend promo custom UI modal.
@interface BackendPromoCustomUICoordinator : ChromeCoordinator

// Delegate to receive promo completion events.
@property(nonatomic, weak) id<BackendPromoCustomUICoordinatorDelegate> delegate;

// Initializes the coordinator with base view controller, browser, and params.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(BackendPromoCustomUIParams*)params
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_COORDINATOR_H_
