// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_WIDGET_PROMO_INSTRUCTIONS_WIDGET_PROMO_INSTRUCTIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_WIDGET_PROMO_INSTRUCTIONS_WIDGET_PROMO_INSTRUCTIONS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/password_manager_reauthentication_delegate.h"

@class WidgetPromoInstructionsCoordinator;

// Delegate for WidgetPromoInstructionsCoordinator.
@protocol WidgetPromoInstructionsCoordinatorDelegate <
    PasswordManagerReauthenticationDelegate>

// Tells the delegate that the widget promo instructions coordinator needs to be
// stopped.
- (void)removeWidgetPromoInstructionsCoordinator:
    (WidgetPromoInstructionsCoordinator*)coordinator;

@end

// Coordinator to present the widget promo instructions screen.
@interface WidgetPromoInstructionsCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<WidgetPromoInstructionsCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_WIDGET_PROMO_INSTRUCTIONS_WIDGET_PROMO_INSTRUCTIONS_COORDINATOR_H_
