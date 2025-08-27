// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_coordinator.h"

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_mediator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

@interface CreditCardScannerCoordinator () <CreditCardScannerMediatorDelegate>

@end

@implementation CreditCardScannerCoordinator {
  // The view controller attached to this coordinator.
  CreditCardScannerViewController* _creditCardScannerViewController;

  // The mediator for credit card scanner.
  CreditCardScannerMediator* _creditCardScannerMediator;

  // The consumer for credit card scanner.
  __weak id<CreditCardScannerConsumer> _creditCardScannerConsumer;
}

#pragma mark - Lifecycle

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                  consumer:
                                      (id<CreditCardScannerConsumer>)consumer {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _creditCardScannerConsumer = consumer;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  _creditCardScannerMediator = [[CreditCardScannerMediator alloc]
      initWithDelegate:self
              consumer:_creditCardScannerConsumer];

  _creditCardScannerViewController =
      [[CreditCardScannerViewController alloc] init];

  _creditCardScannerViewController.modalPresentationStyle =
      UIModalPresentationPageSheet;
  [self.baseViewController
      presentViewController:_creditCardScannerViewController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [super stop];
  [_creditCardScannerViewController dismissViewControllerAnimated:YES
                                                       completion:nil];
  _creditCardScannerViewController = nil;
  _creditCardScannerMediator = nil;
}

#pragma mark - CreditCardScannerMediatorDelegate

- (void)creditCardScannerMediatorDidFinishScan:
    (CreditCardScannerMediator*)mediator {
  [self stop];
}

@end
