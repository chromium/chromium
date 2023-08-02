// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/default_browser/sc_default_browser_fullscreen_promo_coordinator.h"

#import "ios/chrome/browser/ui/default_promo/default_browser_promo_view_controller.h"

@interface SCDefaultBrowserFullscreenPromoCoordinator ()

@property(nonatomic, strong)
    DefaultBrowserPromoViewController* defaultBrowerPromoViewController;

@end

@implementation SCDefaultBrowserFullscreenPromoCoordinator
@synthesize baseViewController = _baseViewController;

#pragma mark - Public Methods.

- (void)start {
  self.defaultBrowerPromoViewController =
      [[DefaultBrowserPromoViewController alloc] init];
  self.defaultBrowerPromoViewController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  [self.baseViewController setHidesBarsOnSwipe:NO];
  [self.baseViewController
      pushViewController:self.defaultBrowerPromoViewController
                animated:YES];
}

@end
