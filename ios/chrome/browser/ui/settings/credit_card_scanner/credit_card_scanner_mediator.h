// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_consumer.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanned_image_delegate.h"

@protocol CreditCardScannerMediatorDelegate;

// A mediator for CreditCardScanner which manages processing images.
API_AVAILABLE(ios(13.0))
@interface CreditCardScannerMediator
    : NSObject <CreditCardConsumer, CreditCardScannedImageDelegate>

// Initializes with Credit Card mediator delegate and Credit Card consumer.
- (instancetype)initWithDelegate:(id<CreditCardScannerMediatorDelegate>)
                                     creditCardScannerMediatorDelegate
              creditCardConsumer:(id<CreditCardConsumer>)creditCardConsumer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_MEDIATOR_H_
