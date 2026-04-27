// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_COORDINATOR_H_

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_coordinator_delegate.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol CreditCardScannerConsumer;

// The coordinator for the Credit Card Scanner screen.
@interface CreditCardScannerCoordinator : ChromeCoordinator

// Delegate to handle camera scanner dismissal events.
@property(nonatomic, weak) id<CreditCardScannerCoordinatorDelegate> delegate;

// Initializes with the Credit Card consumer.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                  consumer:
                                      (id<CreditCardScannerConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_COORDINATOR_H_
