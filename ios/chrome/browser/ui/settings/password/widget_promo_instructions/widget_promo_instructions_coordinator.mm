// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/widget_promo_instructions/widget_promo_instructions_coordinator.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/ui/settings/password/widget_promo_instructions/widget_promo_instructions_view_controller.h"

@interface WidgetPromoInstructionsCoordinator () <
    UIAdaptivePresentationControllerDelegate>

// Password Manager widget promo instructions view controller.
@property(nonatomic, strong)
    WidgetPromoInstructionsViewController* viewController;

@end

@implementation WidgetPromoInstructionsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewController = [[WidgetPromoInstructionsViewController alloc] init];
  self.viewController.presentationController.delegate = self;

  [self.baseNavigationController presentViewController:self.viewController
                                              animated:YES
                                            completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate widgetPromoInstructionsCoordinatorDidRemove:self];
}

@end
