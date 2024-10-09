// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/root_drive_file_picker_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/browse_drive_file_picker_coordinator.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/browse_drive_file_picker_coordinator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

@interface RootDriveFilePickerCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    DriveFilePickerMediatorDelegate,
    BrowseDriveFilePickerCoordinatorDelegate>

@end

@implementation RootDriveFilePickerCoordinator {
  DriveFilePickerNavigationController* _navigationController;
  DriveFilePickerMediator* _mediator;
  DriveFilePickerTableViewController* _viewController;
  // WebState for which the Drive file picker is presented.
  base::WeakPtr<web::WebState> _webState;
  raw_ptr<AuthenticationService> _authenticationService;
  id<SystemIdentity> _currentIdentity;
  // A child `BrowseDriveFilePickerCoordinator` created and started to browse an
  // drive folder.
  BrowseDriveFilePickerCoordinator* _childBrowseCoordinator;
  // The set of images being fetched, soon to be added to `_imageCache`.
  NSMutableSet<NSString*>* _imagesPending;
  // Cache of fetched images for the Drive file picker.
  NSCache<NSString*, UIImage*>* _imageCache;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK(webState);
    _webState = webState->GetWeakPtr();
    _imagesPending = [NSMutableSet set];
    _imageCache = [[NSCache alloc] init];
  }
  return self;
}

- (void)start {
  ProfileIOS* profile = self.browser->GetProfile()->GetOriginalProfile();
  _authenticationService = AuthenticationServiceFactory::GetForProfile(profile);
  _currentIdentity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  drive::DriveService* driveService =
      drive::DriveServiceFactory::GetForProfile(profile);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  std::unique_ptr<image_fetcher::ImageDataFetcher> imageFetcher =
      std::make_unique<image_fetcher::ImageDataFetcher>(
          profile->GetSharedURLLoaderFactory());
  _viewController = [[DriveFilePickerTableViewController alloc] init];
  _navigationController = [[DriveFilePickerNavigationController alloc]
      initWithRootViewController:_viewController];
  _mediator = [[DriveFilePickerMediator alloc]
           initWithWebState:_webState.get()
                   identity:_currentIdentity
                      title:nil
              imagesPending:_imagesPending
                 imageCache:_imageCache
             collectionType:DriveFilePickerCollectionType::kRoot
           folderIdentifier:nil
                     filter:DriveFilePickerFilter::kShowAllFiles
        ignoreAcceptedTypes:NO
            sortingCriteria:DriveItemsSortingType::kName
           sortingDirection:DriveItemsSortingOrder::kAscending
               driveService:driveService
      accountManagerService:accountManagerService
               imageFetcher:std::move(imageFetcher)];

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
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  _mediator.driveFilePickerHandler = driveFilePickerHandler;

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
  [_childBrowseCoordinator stop];
  _childBrowseCoordinator = nil;
  _navigationController = nil;
  _viewController = nil;
  _authenticationService = nil;
}

- (void)setSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  CHECK(_mediator);
  _currentIdentity = selectedIdentity;
  [_navigationController popToRootViewControllerAnimated:YES];
  [_childBrowseCoordinator stop];
  _childBrowseCoordinator = nil;
  [_mediator setSelectedIdentity:selectedIdentity];
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

- (void)
    browseDriveCollectionWithMediator:
        (DriveFilePickerMediator*)driveFilePickerMediator
                                title:(NSString*)title
                        imagesPending:(NSMutableSet<NSString*>*)imagesPending
                           imageCache:(NSCache<NSString*, UIImage*>*)imageCache
                       collectionType:
                           (DriveFilePickerCollectionType)collectionType
                     folderIdentifier:(NSString*)folderIdentifier
                               filter:(DriveFilePickerFilter)filter
                  ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes
                      sortingCriteria:(DriveItemsSortingType)sortingCriteria
                     sortingDirection:(DriveItemsSortingOrder)sortingDirection {
  _childBrowseCoordinator = [[BrowseDriveFilePickerCoordinator alloc]
      initWithBaseNavigationViewController:_navigationController
                                   browser:self.browser
                                  webState:_webState
                                     title:title
                             imagesPending:imagesPending
                                imageCache:imageCache
                            collectionType:collectionType
                          folderIdentifier:folderIdentifier
                                    filter:filter
                       ignoreAcceptedTypes:ignoreAcceptedTypes
                           sortingCriteria:sortingCriteria
                          sortingDirection:sortingDirection
                                  identity:_currentIdentity];
  _childBrowseCoordinator.delegate = self;
  [_childBrowseCoordinator start];
}

- (void)mediatorDidStopFileSelection:(DriveFilePickerMediator*)mediator {
  __weak id<DriveFilePickerCommands> driveFilePickerHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         DriveFilePickerCommands);
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [driveFilePickerHandler hideDriveFilePicker];
                         }];
}

- (void)browseToParentWithMediator:(DriveFilePickerMediator*)mediator {
}

#pragma mark - BrowseDriveFilePickerCoordinatorDelegate

- (void)coordinatorShouldStop:(ChromeCoordinator*)coordinator {
  CHECK(coordinator == _childBrowseCoordinator);
  [_childBrowseCoordinator stop];
  _childBrowseCoordinator = nil;
}

@end
