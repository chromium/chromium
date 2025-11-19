// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/instructions_bottom_sheet/ui/instructions_bottom_sheet_coordinator.h"

#import "ios/chrome/browser/instructions_bottom_sheet/ui/instructions_bottom_sheet_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface InstructionsBottomSheetCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation InstructionsBottomSheetCoordinator {
  // The view controller for this coordinator.
  InstructionsBottomSheetViewController* _viewController;
  // The navigation controller containing the instructions.
  UINavigationController* _navigationController;
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
  _viewController =
      [[InstructionsBottomSheetViewController alloc] initWithTitle:_title
                                                      instructions:_steps];
  _viewController.actionHandler = self;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];

  _viewController.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(stop)];

  self.baseViewController.presentationController.delegate = self;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _viewController = nil;
  _navigationController = nil;
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

@end
