// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_mediator.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_image_processor.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_mediator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

@interface CreditCardScannerMediator ()

// Delegate notified when a card has been scanned.
@property(nonatomic, weak) id<CreditCardScannerMediatorDelegate>
    creditCardScannerMediatorDelegate;

// This property is for an interface which notfies the credit card consumer.
@property(nonatomic, weak) id<CreditCardConsumer> creditCardConsumer;

// The card number set after |textRecognitionRequest| from recognised text on
// the card.
@property(nonatomic, strong) NSString* cardNumber;

// The card expiration month set after |textRecognitionRequest| from recognised
// text on the card.
@property(nonatomic, strong) NSString* expirationMonth;

// The card expiration year set after |textRecognitionRequest| from recognised
// text on the card.
@property(nonatomic, strong) NSString* expirationYear;

// Object to Perform image processing and return the text on the image.
@property(nonatomic, strong)
    CreditCardScannerImageProcessor* creditCardImageScanner;

@end

@implementation CreditCardScannerMediator

#pragma mark - Lifecycle

- (instancetype)initWithDelegate:(id<CreditCardScannerMediatorDelegate>)
                                     creditCardScannerMediatorDelegate
              creditCardConsumer:(id<CreditCardConsumer>)creditCardConsumer {
  self = [super init];
  if (self) {
    _creditCardScannerMediatorDelegate = creditCardScannerMediatorDelegate;
    _creditCardConsumer = creditCardConsumer;
    _creditCardImageScanner =
        [[CreditCardScannerImageProcessor alloc] initWithConsumer:self];
  }

  return self;
}

#pragma mark - CreditCardScannerImageDelegate

- (void)processOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                         viewport:(CGRect)viewport {
  // Current thread is unknown background thread as is a callback from UIKit.
  DCHECK(!NSThread.isMainThread);
  [self.creditCardImageScanner processOutputSampleBuffer:sampleBuffer
                                                viewport:viewport];
}

#pragma mark - CreditCardConsumer

- (void)setCreditCardNumber:(NSString*)cardNumber
            expirationMonth:(NSString*)expirationMonth
             expirationYear:(NSString*)expirationYear {
  [self.creditCardConsumer setCreditCardNumber:cardNumber
                               expirationMonth:expirationMonth
                                expirationYear:expirationYear];
  self.creditCardImageScanner = nil;
  [self dismissScannerOnCardScanned];
}

#pragma mark - Helper Methods

// Dismisses the scanner when credit card number is found.
- (void)dismissScannerOnCardScanned {
  base::RecordAction(UserMetricsAction("MobileCreditCardScannerScannedCard"));
  [self.creditCardScannerMediatorDelegate
      creditCardScannerMediatorDidFinishScan:self];
}

@end
