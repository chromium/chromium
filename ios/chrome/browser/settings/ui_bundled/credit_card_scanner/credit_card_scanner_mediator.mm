// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_mediator.h"

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_image_processor.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_mediator_delegate.h"

@implementation CreditCardScannerMediator {
  // Delegate notified when a card has been scanned.
  __weak id<CreditCardScannerMediatorDelegate>
      _creditCardScannerMediatorDelegate;

  // This property is for an interface which notfies the credit card consumer.
  __weak id<CreditCardScannerConsumer> _creditCardScannerConsumer;
}

#pragma mark - Lifecycle

- (instancetype)initWithDelegate:(id<CreditCardScannerMediatorDelegate>)delegate
                        consumer:(id<CreditCardScannerConsumer>)consumer {
  self = [super init];
  if (self) {
    _creditCardScannerMediatorDelegate = delegate;
    _creditCardScannerConsumer = consumer;
    _creditCardScannerImageProcessor =
        [[CreditCardScannerImageProcessor alloc] initWithConsumer:self];
  }

  return self;
}

- (void)disconnect {
  _creditCardScannerMediatorDelegate = nil;
  _creditCardScannerConsumer = nil;
  _creditCardScannerImageProcessor = nil;
}

#pragma mark - CreditCardScannerConsumer

- (void)setCreditCardNumber:(NSString*)cardNumber
            expirationMonth:(NSString*)expirationMonth
             expirationYear:(NSString*)expirationYear {
  [_creditCardScannerConsumer setCreditCardNumber:cardNumber
                                  expirationMonth:expirationMonth
                                   expirationYear:expirationYear];
  _creditCardScannerImageProcessor = nil;
  [_creditCardScannerMediatorDelegate
      creditCardScannerMediatorDidFinishScan:self];
}

@end
