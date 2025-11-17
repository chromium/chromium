// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/instructions_bottom_sheet/instructions_bottom_sheet_coordinator.h"

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/instructions_bottom_sheet/instructions_bottom_sheet_view_controller.h"

@interface InstructionsBottomSheetCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation InstructionsBottomSheetCoordinator {
  // The view controller for this coordinator.
  InstructionsBottomSheetViewController* _viewController;
  // The title of the bottom sheet.
  NSString* _title;
  // The instruction steps to be displayed.
  NSArray<NSString*>* _steps;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     title:(NSString*)title
                                     steps:(NSArray<NSString*>*)steps {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _title = [title copy];
    _steps = [steps copy];
  }
  return self;
}

#pragma mark - Chrome Coordinator

- (void)start {
  [super start];

  _viewController =
      [[InstructionsBottomSheetViewController alloc] initWithTitle:_title
                                                      instructions:_steps];
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
