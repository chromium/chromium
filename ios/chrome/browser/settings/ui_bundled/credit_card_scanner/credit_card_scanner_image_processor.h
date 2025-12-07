// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_IMAGE_PROCESSOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_IMAGE_PROCESSOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanned_image_delegate.h"

@protocol CreditCardScannerConsumer;

// A class which processes credit card images to recognise the credit card
// number and expiry date.
@interface CreditCardScannerImageProcessor
    : NSObject <CreditCardScannedImageDelegate>

- (instancetype)initWithConsumer:(id<CreditCardScannerConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_IMAGE_PROCESSOR_H_
