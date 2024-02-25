// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/store_kit/model/store_kit_coordinator.h"

#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator_delegate.h"

@interface StoreKitCoordinator () <SKStoreProductViewControllerDelegate>
// StoreKitViewController to present. Set as a weak reference so it only exists
// while its being presented by baseViewController.
@property(nonatomic, weak) SKStoreProductViewController* viewController;
@end

@implementation StoreKitCoordinator
@synthesize iTunesProductParameters = _iTunesProductParameters;

#pragma mark - Public

- (void)start {
  DCHECK(self.iTunesProductParameters
             [SKStoreProductParameterITunesItemIdentifier]);
  // StoreKit shouldn't be launched, if there is one already presented.
  if (self.viewController) {
    return;
  }
  SKStoreProductViewController* viewController =
      [[SKStoreProductViewController alloc] init];
  viewController.delegate = self;
  [viewController loadProductWithParameters:self.iTunesProductParameters
                            completionBlock:nil];
  [self.baseViewController presentViewController:viewController
                                        animated:YES
                                      completion:nil];
  self.viewController = viewController;
}

- (void)stop {
  // Do not call -dismissViewControllerAnimated:completion: on
  // `self.baseViewController`, since the receiver of the method can be
  // dismissed if there is no presented view controller. On iOS 12
  // SKStoreProductViewControllerDelegate is responsible for dismissing
  // SKStoreProductViewController. On iOS 13.0 OS dismisses
  // SKStoreProductViewController after calling -productViewControllerDidFinish:
  // On iOS 13.2 OS dismisses SKStoreProductViewController before calling
  // -productViewControllerDidFinish: Calling
  // -dismissViewControllerAnimated:completion: on `self.baseViewController` on
  // iOS 13.2 will dismiss base view controller and break the application UI.
  // According to SKStoreProductViewController documentation the delegate is
  // responsible for calling deprecated dismissModalViewControllerAnimated: so
  // the documentation is clearly outdated and this code should be resilient to
  // different SKStoreProductViewController behavior without relying on iOS
  // version check (see crbug.com/1027058).
  [self.viewController dismissViewControllerAnimated:YES completion:nil];

  self.viewController = nil;
}

#pragma mark - SKStoreProductViewControllerDelegate

- (void)productViewControllerDidFinish:
    (SKStoreProductViewController*)viewController {
  [self.delegate storeKitCoordinatorWantsToStop:self];
}

@end
