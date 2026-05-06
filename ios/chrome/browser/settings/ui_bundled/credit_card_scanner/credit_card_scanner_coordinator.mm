// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_coordinator.h"

#import "base/ios/block_types.h"
#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/scanner/ui_bundled/scanner_presenting.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_image_processor.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_mediator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

@interface CreditCardScannerCoordinator () <CreditCardScannerMediatorDelegate,
                                            ScannerPresenting>

@end

@implementation CreditCardScannerCoordinator {
  // The view controller attached to this coordinator.
  CreditCardScannerViewController* _creditCardScannerViewController;

  // The mediator for credit card scanner.
  CreditCardScannerMediator* _creditCardScannerMediator;

  // The consumer for credit card scanner.
  __weak id<CreditCardScannerConsumer> _creditCardScannerConsumer;

  // Whether the scan succeeded.
  BOOL _scanSucceeded;
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

  _creditCardScannerViewController = [[CreditCardScannerViewController alloc]
      initWithPresentationProvider:self];
  _creditCardScannerViewController.delegate =
      _creditCardScannerMediator.creditCardScannerImageProcessor;

  _creditCardScannerViewController.modalPresentationStyle =
      UIModalPresentationPageSheet;
  [self.baseViewController
      presentViewController:_creditCardScannerViewController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [super stop];

  base::UmaHistogramBoolean("IOS.ScanCardFinished", _scanSucceeded);

  __weak id<CreditCardScannerCoordinatorDelegate> delegate = self.delegate;
  __weak __typeof(self) weakSelf = self;
  [_creditCardScannerViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [delegate
                               creditCardScannerCoordinatorDidFinish:weakSelf];
                         }];
  _creditCardScannerViewController.delegate = nil;
  _creditCardScannerViewController = nil;
  [_creditCardScannerMediator disconnect];
  _creditCardScannerMediator = nil;
}

#pragma mark - ScannerPresenting

- (void)dismissScannerViewController:(UIViewController*)controller
                          completion:(ProceduralBlock)completion {
  [self stop];
}

#pragma mark - CreditCardScannerMediatorDelegate

- (void)creditCardScannerMediatorDidFinishScan:
    (CreditCardScannerMediator*)mediator {
  _scanSucceeded = YES;
  [self stop];
}

@end
