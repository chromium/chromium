// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_IMAGE_PROCESSOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_IMAGE_PROCESSOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanned_image_delegate.h"

@protocol CreditCardConsumer;

// A class process credit card images to recognise the text.
API_AVAILABLE(ios(13.0))
@interface CreditCardScannerImageProcessor
    : NSObject <CreditCardScannedImageDelegate>

// Initializes with Credit Card consumer.
- (instancetype)initWithConsumer:(id<CreditCardConsumer>)creditCardConsumer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_IMAGE_PROCESSOR_H_
