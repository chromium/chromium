// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_PAYMENTS_COORDINATOR_AUTOFILL_CREDIT_CARD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_PAYMENTS_COORDINATOR_AUTOFILL_CREDIT_CARD_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AutofillCreditCardCoordinatorDelegate;

// Coordinator for the Autofill Credit Card / Payment Methods settings screen.
@interface AutofillCreditCardCoordinator : ChromeCoordinator

// Designated initializer.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Delegate for this coordinator.
@property(nonatomic, weak) id<AutofillCreditCardCoordinatorDelegate> delegate;

// Whether the Level Up Payment Methods walkthrough IPH should be presented.
@property(nonatomic, assign) BOOL shouldShowLevelUpPaymentMethodsWalkthroughIPH;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_PAYMENTS_COORDINATOR_AUTOFILL_CREDIT_CARD_COORDINATOR_H_
