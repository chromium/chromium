// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/bannered_promo_view_provider.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_mediator.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_view_provider.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_COORDINATOR_INTERNAL_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_COORDINATOR_INTERNAL_H_

// Extension to provide access for unit tests to easily inject state.
@interface PromosManagerCoordinator (Testing)

// A mediator that observes when it's a good time to display a promo.
@property(nonatomic, strong) PromosManagerMediator* mediator;

// The current StandardPromoViewProvider, if any.
@property(nonatomic, weak) id<StandardPromoViewProvider> provider;

// The current BanneredPromoViewProvider, if any.
@property(nonatomic, weak) id<BanneredPromoViewProvider> banneredProvider;

// The current ConfirmationAlertViewController, if any.
@property(nonatomic, strong) ConfirmationAlertViewController* viewController;

// The current PromoStyleViewController, if any.
@property(nonatomic, strong) PromoStyleViewController* banneredViewController;

// UIAdaptivePresentationControllerDelegate.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController;

// ConfirmationAlertActionHandler.
- (void)confirmationAlertDismissAction;

// Dismisses the current promo view controllers, if any.
- (void)dismissViewControllers;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_COORDINATOR_INTERNAL_H_
