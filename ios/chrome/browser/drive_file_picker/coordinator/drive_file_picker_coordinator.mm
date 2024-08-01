// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_coordinator.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"

@interface DriveFilePickerCoordinator () <
    UIAdaptivePresentationControllerDelegate>

@end

@implementation DriveFilePickerCoordinator {
  DriveFilePickerNavigationController* _navigationController;
  DriveFilePickerMediator* _mediator;
  // WebState for which the Drive file picker is presented.
  base::WeakPtr<web::WebState> _webState;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK(webState);
    _webState = webState->GetWeakPtr();
  }
  return self;
}

- (void)start {
  DriveFilePickerTableViewController* rootViewController =
      [[DriveFilePickerTableViewController alloc] init];
  _navigationController = [[DriveFilePickerNavigationController alloc]
      initWithRootViewController:rootViewController];
  _mediator =
      [[DriveFilePickerMediator alloc] initWithWebState:_webState.get()];

  _navigationController.modalInPresentation = YES;
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.presentationController.delegate = self;
  _navigationController.sheetPresentationController.prefersGrabberVisible = YES;
  _navigationController.sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  _navigationController.sheetPresentationController.selectedDetentIdentifier =
      IsCompactWidth(self.baseViewController.traitCollection)
          ? [UISheetPresentationControllerDetent mediumDetent].identifier
          : [UISheetPresentationControllerDetent largeDetent].identifier;

  id<DriveFilePickerCommands> driveFilePickerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DriveFilePickerCommands);
  rootViewController.driveFilePickerHandler = driveFilePickerHandler;
  rootViewController.mutator = _mediator;
  _navigationController.driveFilePickerHandler = driveFilePickerHandler;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  [_navigationController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  _navigationController = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // If the navigation controller is not dismissed programmatically i.e. not
  // dismissed using `dismissViewControllerAnimated:completion:`, then call
  // `-hideDriveFilePicker`.
  id<DriveFilePickerCommands> driveFilePickerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DriveFilePickerCommands);
  [driveFilePickerHandler hideDriveFilePicker];
}

@end
