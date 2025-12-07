// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_consumer.h"

@class CreditCardScannerImageProcessor;
@protocol CreditCardScannerMediatorDelegate;

// A mediator for CreditCardScanner which manages processing images.
@interface CreditCardScannerMediator : NSObject <CreditCardScannerConsumer>

// An image processor that can extract credit card details.
@property(nonatomic, strong, readonly)
    CreditCardScannerImageProcessor* creditCardScannerImageProcessor;

// Initializes with Credit Card mediator delegate and Credit Card consumer.
- (instancetype)initWithDelegate:(id<CreditCardScannerMediatorDelegate>)delegate
                        consumer:(id<CreditCardScannerConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Cleans up and disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_MEDIATOR_H_
