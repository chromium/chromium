// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_commands.h"

@interface DefaultBrowserPromoCoordinator : ChromeCoordinator
// Handler for all actions of this coordinator.
@property(nonatomic, weak) id<DefaultBrowserPromoCommands> handler;

// Contains all the stats that needs to be recorded for all promo actions.
@property(nonatomic, strong) PromoStatistics* promoStats;
@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_PROMO_COORDINATOR_H_
