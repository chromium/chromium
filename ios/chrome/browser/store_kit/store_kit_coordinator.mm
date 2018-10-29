// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/store_kit/store_kit_coordinator.h"

#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface StoreKitCoordinator ()<SKStoreProductViewControllerDelegate> {
  SKStoreProductViewController* _viewController;
}
@end

@implementation StoreKitCoordinator
@synthesize iTunesProductParameters = _iTunesProductParameters;

#pragma mark - Public

- (void)start {
  DCHECK(self.iTunesProductParameters
             [SKStoreProductParameterITunesItemIdentifier]);
  // StoreKit shouldn't be launched, if there is one already presented or if
  // there is another view presented by the base view controller.
  if (_viewController || self.baseViewController.presentedViewController)
    return;
  _viewController = [[SKStoreProductViewController alloc] init];
  _viewController.delegate = self;
  [_viewController
      loadProductWithParameters:self.iTunesProductParameters
                completionBlock:^(BOOL result, NSError* _Nullable error) {
                  UMA_HISTOGRAM_BOOLEAN("IOS.StoreKitLoadedSuccessfully",
                                        result);
                }];
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
}

#pragma mark - StoreKitLauncher

- (void)openAppStore:(NSString*)iTunesItemIdentifier {
  [self openAppStoreWithParameters:@{
    SKStoreProductParameterITunesItemIdentifier : iTunesItemIdentifier
  }];
}

- (void)openAppStoreWithParameters:(NSDictionary*)productParameters {
  self.iTunesProductParameters = productParameters;
  [self start];
}

#pragma mark - SKStoreProductViewControllerDelegate

- (void)productViewControllerDidFinish:
    (SKStoreProductViewController*)viewController {
  [self stop];
}

@end
