// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/browse_drive_file_picker_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/browse_drive_file_picker_coordinator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_collection.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_metrics_helper.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

@interface BrowseDriveFilePickerCoordinator () <
    DriveFilePickerMediatorDelegate,
    DriveFilePickerTableViewControllerDelegate,
    BrowseDriveFilePickerCoordinatorDelegate>

@end

@implementation BrowseDriveFilePickerCoordinator {
  DriveFilePickerMediator* _mediator;

  DriveFilePickerTableViewController* _viewController;

  // Parameters to initialize the mediator.
  base::WeakPtr<web::WebState> _webState;
  BrowseDriveFilePickerCoordinator* _childBrowseCoordinator;
  std::unique_ptr<DriveFilePickerCollection> _collection;
  DriveFilePickerOptions _options;
  raw_ptr<DriveFilePickerImageFetcher> _imageFetcher;
  __weak DriveFilePickerMetricsHelper* _metricsHelper;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationViewController:
        (UINavigationController*)baseNavigationController
                                 browser:(Browser*)browser
                                webState:(base::WeakPtr<web::WebState>)webState
                              collection:
                                  (std::unique_ptr<DriveFilePickerCollection>)
                                      collection
                            imageFetcher:
                                (DriveFilePickerImageFetcher*)imageFetcher
                                 options:(DriveFilePickerOptions)options
                           metricsHelper:
                               (DriveFilePickerMetricsHelper*)metricsHelper {
  self = [super initWithBaseViewController:baseNavigationController
                                   browser:browser];
  if (self) {
    CHECK(webState);
    CHECK(collection);
    _baseNavigationController = baseNavigationController;
    _webState = webState;
    _collection = std::move(collection);
    _options = options;
    _imageFetcher = imageFetcher;
    _metricsHelper = metricsHelper;
  }
  return self;
}

- (void)start {
  _viewController = [[DriveFilePickerTableViewController alloc] init];
  _mediator =
      [[DriveFilePickerMediator alloc] initWithWebState:_webState.get()
                                             collection:std::move(_collection)
                                                options:_options];

  ProfileIOS* profile = self.profile->GetOriginalProfile();
  _mediator.delegate = self;
  _mediator.driveFilePickerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DriveFilePickerCommands);
  _mediator.driveService = drive::DriveServiceFactory::GetForProfile(profile);
  _mediator.identityManager = IdentityManagerFactory::GetForProfile(profile);
  _mediator.accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  _mediator.imageFetcher = _imageFetcher;
  _mediator.metricsHelper = _metricsHelper;

  _viewController.delegate = self;
  _viewController.driveFilePickerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DriveFilePickerCommands);
  [_baseNavigationController pushViewController:_viewController animated:YES];
  _baseNavigationController.sheetPresentationController.prefersGrabberVisible =
      YES;

  _viewController.mutator = _mediator;
  _mediator.consumer = _viewController;
}

- (void)stop {
  [_mediator disconnect];
  if (![_viewController isMovingFromParentViewController]) {
    [_viewController.navigationController popViewControllerAnimated:YES];
  }
  [_childBrowseCoordinator stop];
  _childBrowseCoordinator = nil;
  _mediator = nil;
  _viewController = nil;
}

#pragma mark - DriveFilePickerMediatorDelegate

- (void)browseDriveCollectionWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                               collection:
                                   (std::unique_ptr<DriveFilePickerCollection>)
                                       collection
                                  options:(DriveFilePickerOptions)options {
  [_mediator setActive:NO];
  _childBrowseCoordinator = [[BrowseDriveFilePickerCoordinator alloc]
      initWithBaseNavigationViewController:_baseNavigationController
                                   browser:self.browser
                                  webState:_webState
                                collection:std::move(collection)
                              imageFetcher:_imageFetcher
                                   options:options
                             metricsHelper:_metricsHelper];
  _childBrowseCoordinator.delegate = self;
  [_childBrowseCoordinator start];
}

- (void)mediatorDidStopFileSelection:(DriveFilePickerMediator*)mediator {
  __weak id<DriveFilePickerCommands> driveFilePickerHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         DriveFilePickerCommands);
  [self.baseNavigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [driveFilePickerHandler hideDriveFilePicker];
                         }];
}

- (void)browseToParentWithMediator:(DriveFilePickerMediator*)mediator {
  [self.delegate coordinatorShouldStop:self];
}

- (void)browseDriveCollectionWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                         didUpdateOptions:(DriveFilePickerOptions)options {
  [self.delegate browseDriveFilePickerCoordinator:self
                                 didUpdateOptions:options];
}

- (void)mediatorDidTapAddAccount:(DriveFilePickerMediator*)mediator {
  [self.delegate coordinatorDidTapAddAccount:self];
}

- (void)mediator:(DriveFilePickerMediator*)mediator
    didAllowDismiss:(BOOL)allowDismiss {
  [self.delegate coordinator:self didAllowDismiss:allowDismiss];
}

- (void)mediator:(DriveFilePickerMediator*)mediator
    didActivateSearch:(BOOL)searchActivated {
  _baseNavigationController.sheetPresentationController.prefersGrabberVisible =
      !searchActivated;
}

#pragma mark - DriveFilePickerTableViewControllerDelegate

- (void)viewControllerDidDisappear:(UIViewController*)viewController {
  [self.delegate coordinatorShouldStop:self];
}

#pragma mark - BrowseDriveFilePickerCoordinatorDelegate

- (void)coordinatorShouldStop:(ChromeCoordinator*)coordinator {
  CHECK(coordinator == _childBrowseCoordinator);
  [_childBrowseCoordinator stop];
  _childBrowseCoordinator = nil;
  // Inform the mediator that it is back on the top.
  [_mediator setActive:YES];
}

- (void)browseDriveFilePickerCoordinator:
            (BrowseDriveFilePickerCoordinator*)coordinator
                        didUpdateOptions:(DriveFilePickerOptions)options {
  [_mediator setPendingOptions:options];
  [self.delegate browseDriveFilePickerCoordinator:self
                                 didUpdateOptions:options];
}

- (void)coordinatorDidTapAddAccount:(ChromeCoordinator*)coordinator {
  [self.delegate coordinatorDidTapAddAccount:self];
}

- (void)coordinator:(ChromeCoordinator*)coordinator
    didAllowDismiss:(BOOL)allowDismiss {
  [self.delegate coordinator:self didAllowDismiss:allowDismiss];
}

@end
