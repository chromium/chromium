// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/instruction_view/instructions_half_sheet_coordinator.h"

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/instruction_view/instructions_half_sheet_view_controller.h"

@interface InstructionsHalfSheetCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation InstructionsHalfSheetCoordinator {
  // The view controller for this coordinator.
  InstructionsHalfSheetViewController* _viewController;
  // The instructional steps to be displayed in the view controller.
  NSArray<NSString*>* _instructionsList;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          instructionsList:
                              (NSArray<NSString*>*)instructionsList {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _instructionsList = instructionsList;
  }
  return self;
}

#pragma mark - Chrome Coordinator

- (void)start {
  [super start];

  _viewController = [[InstructionsHalfSheetViewController alloc]
      initWithInstructionList:_instructionsList
                actionHandler:self];
  _viewController.titleText = self.titleText;

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
  // There is no primary action.
}

- (void)confirmationAlertDismissAction {
  [self stop];
}

@end
