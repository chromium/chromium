// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_commands.h"
#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_mediator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol PromosManagerUIHandler;

@interface DefaultBrowserGenericPromoCoordinator : ChromeCoordinator

// Handler for all actions of this coordinator.
@property(nonatomic, weak) id<DefaultBrowserGenericPromoCommands> handler;

// Whether or not the current showing came from a past Remind Me Later.
@property(nonatomic, assign) BOOL promoWasFromRemindMeLater;

// The promos manager ui handler to alert for promo UI changes. Should only be
// set if this coordinator was a promo presented by the PromosManager.
@property(nonatomic, weak) id<PromosManagerUIHandler> promosUIHandler;

// The mediator for the generic default browser promo.
@property(nonatomic, strong) DefaultBrowserGenericPromoMediator* mediator;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_COORDINATOR_H_
