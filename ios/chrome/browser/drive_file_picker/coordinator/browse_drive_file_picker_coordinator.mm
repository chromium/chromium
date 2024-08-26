// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/browse_drive_file_picker_coordinator.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_item_identifier.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"

@interface BrowseDriveFilePickerCoordinator () <DriveFilePickerMediatorDelegate>

@end

@implementation BrowseDriveFilePickerCoordinator {
  DriveFilePickerMediator* _mediator;

  DriveFilePickerTableViewController* _viewController;

  // WebState for which the Drive file picker is presented.
  base::WeakPtr<web::WebState> _webState;

  // A child `BrowseDriveFilePickerCoordinator` created and started to browse an
  // inner folder.
  BrowseDriveFilePickerCoordinator* _childBrowseCoordinator;

  // The folder associated to the current `BrowseDriveFilePickerCoordinator`.
  DriveItemIdentifier* _driveFolderID;

  // Identity whose Drive is being browsed.
  id<SystemIdentity> _identity;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationViewController:
        (UINavigationController*)baseNavigationController
                                 browser:(Browser*)browser
                                webState:(base::WeakPtr<web::WebState>)webState
                           driveFolderID:(DriveItemIdentifier*)driveFolderID
                                identity:(id<SystemIdentity>)identity {
  self = [super initWithBaseViewController:baseNavigationController
                                   browser:browser];
  if (self) {
    CHECK(webState);
    CHECK(identity);
    _baseNavigationController = baseNavigationController;
    _webState = webState;
    _driveFolderID = driveFolderID;
    _identity = identity;
  }
  return self;
}

- (void)start {
  _viewController = [[DriveFilePickerTableViewController alloc] init];
  _mediator = [[DriveFilePickerMediator alloc] initWithWebState:_webState.get()
                                                       identity:_identity
                                                  driveFolderID:_driveFolderID];

  id<DriveFilePickerCommands> driveFilePickerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DriveFilePickerCommands);
  _viewController.driveFilePickerHandler = driveFilePickerHandler;
  _viewController.mutator = _mediator;
  _mediator.consumer = _viewController;
  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  [_baseNavigationController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  _viewController = nil;
  [_childBrowseCoordinator stop];
  _childBrowseCoordinator = nil;
  _identity = nil;
  _driveFolderID = nil;
}

#pragma mark - DriveFilePickerMediatorDelegate

- (void)browseDriveFolderWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                        driveFolderID:(DriveItemIdentifier*)driveFolderID {
  _childBrowseCoordinator = [[BrowseDriveFilePickerCoordinator alloc]
      initWithBaseNavigationViewController:_baseNavigationController
                                   browser:self.browser
                                  webState:_webState
                             driveFolderID:driveFolderID
                                  identity:_identity];
  [_childBrowseCoordinator start];
}

- (void)searchDriveFolderWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                        driveFolderID:(DriveItemIdentifier*)driveFolderID {
  // TODO(crbug.com/344812548): Start the `SearchDriveFilePickerCoordinator` and
  // add it as child coordinator.
}

@end
