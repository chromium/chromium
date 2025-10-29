// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/coordinator/best_features_instruction_steps_coordinator.h"

#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_instruction_steps_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface BestFeaturesInstructionStepsCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation BestFeaturesInstructionStepsCoordinator {
  // The view controller for this coordinator.
  BestFeaturesInstructionStepsViewController* _viewController;
  // The item containing the instructions to be displayed.
  BestFeaturesItem* _item;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      item:(BestFeaturesItem*)item {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _item = item;
  }
  return self;
}

#pragma mark - Chrome Coordinator

- (void)start {
  [super start];

  _viewController =
      [[BestFeaturesInstructionStepsViewController alloc] initWithItem:_item];
  _viewController.actionHandler = self;

  self.baseViewController.presentationController.delegate = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;

  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // No primary action.
}

- (void)confirmationAlertDismissAction {
  [self stop];
}

@end
