// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/scanner/scanner_presenting.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_consumer.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_mediator.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CreditCardScannerCoordinator () <CreditCardScannerMediatorDelegate,
                                            ScannerPresenting>

// The view controller attached to this coordinator.
@property(nonatomic, strong)
    CreditCardScannerViewController* creditCardScannerViewController;

// The mediator for credit card scanner.
@property(nonatomic, strong)
    CreditCardScannerMediator* creditCardScannerMediator;

// The consumer for credit card scanner.
@property(nonatomic, weak) id<CreditCardConsumer> creditCardConsumer;

@end

@implementation CreditCardScannerCoordinator

#pragma mark - Lifecycle

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                        creditCardConsumer:
                            (id<CreditCardConsumer>)creditCardConsumer {
  self = [super initWithBaseViewController:baseViewController browserState:nil];
  if (self) {
    DCHECK(self);
    _creditCardConsumer = creditCardConsumer;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  self.creditCardScannerMediator = [[CreditCardScannerMediator alloc]
        initWithDelegate:self
      creditCardConsumer:self.creditCardConsumer];

  self.creditCardScannerViewController =
      [[CreditCardScannerViewController alloc]
          initWithPresentationProvider:self
                              delegate:self.creditCardScannerMediator];

  self.creditCardScannerViewController.modalPresentationStyle =
      UIModalPresentationFullScreen;
  [self.baseViewController
      presentViewController:self.creditCardScannerViewController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [super stop];
  self.creditCardScannerViewController.cameraController = nil;
  [self.creditCardScannerViewController dismissViewControllerAnimated:YES
                                                           completion:nil];
  self.creditCardScannerViewController = nil;
  self.creditCardScannerMediator = nil;
}

#pragma mark - ScannerPresenting

- (void)dismissScannerViewController:(UIViewController*)controller
                          completion:(void (^)(void))completion {
  [self stop];
}

#pragma mark - CreditCardScannerMediatorDelegate

- (void)creditCardScannerMediatorDidFinishScan:
    (CreditCardScannerMediator*)mediator {
  [self stop];
}

@end
