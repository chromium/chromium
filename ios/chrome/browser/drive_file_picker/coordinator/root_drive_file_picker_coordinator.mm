// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/root_drive_file_picker_coordinator.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/browse_drive_file_picker_coordinator.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/root_drive_file_picker_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"

@interface RootDriveFilePickerCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    DriveFilePickerMediatorDelegate>

@end

@implementation RootDriveFilePickerCoordinator {
  DriveFilePickerNavigationController* _navigationController;
  DriveFilePickerMediator* _mediator;
  RootDriveFilePickerTableViewController* _viewController;
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
  _viewController = [[RootDriveFilePickerTableViewController alloc] init];
  _navigationController = [[DriveFilePickerNavigationController alloc]
      initWithRootViewController:_viewController];
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
  _viewController.driveFilePickerHandler = driveFilePickerHandler;
  _viewController.mutator = _mediator;
  _mediator.delegate = self;
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
  _viewController = nil;
  for (ChromeCoordinator* coordinator in self.childCoordinators) {
    [coordinator stop];
  }
  [self.childCoordinators removeAllObjects];
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

#pragma mark - DriveFilePickerMediatorDelegate

- (void)browseDriveFolderWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                          driveFolder:(NSString*)driveFolder {
  BrowseDriveFilePickerCoordinator* browseCoordinator =
      [[BrowseDriveFilePickerCoordinator alloc]
          initWithBaseNavigationViewController:_navigationController
                                       browser:self.browser
                                      webState:_webState
                                        folder:driveFolder];
  [browseCoordinator start];
  [self.childCoordinators addObject:browseCoordinator];
}

- (void)searchDriveFolderWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                          driveFolder:(NSString*)driveFolder {
  // TODO(crbug.com/344812548): Start the `SearchDriveFilePickerCoordinator` and
  // add it as child coordinator.
}

@end
