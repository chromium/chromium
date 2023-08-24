// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_coordinator.h"

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_view_controller.h"

@interface FamilyPromoCoordinator ()

// Main view controller for this coordinator.
@property(nonatomic, strong) FamilyPromoViewController* viewController;

@end

@implementation FamilyPromoCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

- (void)start {
  [super start];

  self.viewController = [[FamilyPromoViewController alloc] init];
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
}

@end
