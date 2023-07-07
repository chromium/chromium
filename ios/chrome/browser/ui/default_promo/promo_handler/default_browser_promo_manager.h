// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_PROMO_HANDLER_DEFAULT_BROWSER_PROMO_MANAGER_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_PROMO_HANDLER_DEFAULT_BROWSER_PROMO_MANAGER_H_

#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol PromosManagerUIHandler;

@interface DefaultBrowserPromoManager : ChromeCoordinator

// The promos manager ui handler to alert for promo UI changes. Should only be
// set if this coordinator was a promo presented by the PromosManager.
@property(nonatomic, weak) id<PromosManagerUIHandler> promosUIHandler;

// Test-only method mocked in test to verify the promo that will be shown.
+ (void)showPromoForTesting:(DefaultPromoType)promoType;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_PROMO_HANDLER_DEFAULT_BROWSER_PROMO_MANAGER_H_
