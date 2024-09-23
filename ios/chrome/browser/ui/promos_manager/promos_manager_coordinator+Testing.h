// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_COORDINATOR_TESTING_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_COORDINATOR_TESTING_H_

#import "ios/chrome/browser/ui/promos_manager/bannered_promo_view_provider.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_mediator.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_view_provider.h"

// Testing category to provide access for unit tests to easily inject state.
@interface PromosManagerCoordinator (Testing)

// The current StandardPromoViewProvider, if any.
@property(nonatomic, weak) id<StandardPromoViewProvider> provider;

// The current BanneredPromoViewProvider, if any.
@property(nonatomic, weak) id<BanneredPromoViewProvider> banneredProvider;

// UIAdaptivePresentationControllerDelegate.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController;

// ConfirmationAlertActionHandler.
- (void)confirmationAlertDismissAction;

// Display promo after tracker is ready.
- (void)displayPromoCallback:(BOOL)isFirstShownPromo;

// Display the given promo.
- (void)displayPromo:(PromoDisplayData)promoData;
@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_COORDINATOR_TESTING_H_
