// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class CreditCardScannerCoordinator;

@protocol CreditCardScannerCoordinatorDelegate <NSObject>

- (void)creditCardScannerCoordinatorDidFinish:
    (CreditCardScannerCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_COORDINATOR_DELEGATE_H_
