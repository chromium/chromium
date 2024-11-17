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
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_metrics_helper.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_alert_utils.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/device_form_factor.h"

@interface RootDriveFilePickerCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    DriveFilePickerMediatorDelegate,
    BrowseDriveFilePickerCoordinatorDelegate,
    UIGestureRecognizerDelegate>

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
  // Whether the file picker should dismiss when swiping down.
  BOOL _presentationControllerShouldDismiss;
  // A helper class to report metrics.
  DriveFilePickerMetricsHelper* _metricsHelper;
  // Gesture recognizer to properly handle tap-to-dismiss.
  UITapGestureRecognizer* _tapToDismissGestureRecognizer;
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
    _presentationControllerShouldDismiss = YES;
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
  _metricsHelper = [[DriveFilePickerMetricsHelper alloc] init];
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
               imageFetcher:std::move(imageFetcher)
              metricsHelper:_metricsHelper];

  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.presentationController.delegate = self;
  if (ui::GetDeviceFormFactor() ==
      ui::DeviceFormFactor::DEVICE_FORM_FACTOR_PHONE) {
    _navigationController.sheetPresentationController.prefersGrabberVisible =
        YES;
    _navigationController.sheetPresentationController.detents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent],
    ];
    _navigationController.sheetPresentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierMedium;
  } else {
    _navigationController.sheetPresentationController.prefersGrabberVisible =
        NO;
    _navigationController.sheetPresentationController.detents =
        @[ [UISheetPresentationControllerDetent largeDetent] ];
    _navigationController.sheetPresentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierLarge;
  }

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

  // Add tap gesture recognizer to window, to handle tap-to-dismiss.
  _tapToDismissGestureRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(didTapToDismiss:)];
  _tapToDismissGestureRecognizer.numberOfTapsRequired = 1;
  _tapToDismissGestureRecognizer.cancelsTouchesInView = NO;
  _tapToDismissGestureRecognizer.delegate = self;
  [self.baseViewController.view.window
      addGestureRecognizer:_tapToDismissGestureRecognizer];
}

- (void)stop {
  [_metricsHelper reportOutcomeMetrics];
  [self.baseViewController.view.window
      removeGestureRecognizer:_tapToDismissGestureRecognizer];
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
  if (selectedIdentity == _currentIdentity) {
    return;
  }
  CHECK(_mediator);
  [_metricsHelper reportAccountChangeWithSuccess:YES isAccountNew:NO];
  [self updateCurrentIdentityWithIdentity:selectedIdentity];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return _presentationControllerShouldDismiss;
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  _metricsHelper.userDismissed = YES;
  // If the navigation controller is not dismissed programmatically i.e. not
  // dismissed using `dismissViewControllerAnimated:completion:`, then call
  // `-hideDriveFilePicker`.
  id<DriveFilePickerCommands> driveFilePickerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DriveFilePickerCommands);
  [driveFilePickerHandler hideDriveFilePicker];
}

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  // If this is called then it means the user attempted to dismiss the Drive
  // file picker while `_presentationControllerShouldDismiss` was NO. This means
  // the 'Discard selection' alert should be presented.
  [self showDiscardSelectionAlert];
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
  [_mediator setActive:NO];
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
                                  identity:_currentIdentity
                             metricsHelper:_metricsHelper];
  _childBrowseCoordinator.delegate = self;
  [_childBrowseCoordinator start];
}

- (void)mediatorDidStopFileSelection:(DriveFilePickerMediator*)mediator {
  [self stopAnimated];
}

- (void)browseToParentWithMediator:(DriveFilePickerMediator*)mediator {
}

- (void)browseDriveCollectionWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                          didUpdateFilter:(DriveFilePickerFilter)filter
                          sortingCriteria:(DriveItemsSortingType)sortingCriteria
                         sortingDirection:
                             (DriveItemsSortingOrder)sortingDirection
                      ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes {
}

- (void)mediatorDidTapAddAccount:(DriveFilePickerMediator*)mediator {
  [self showAddAccount];
}

- (void)mediator:(DriveFilePickerMediator*)mediator
    didAllowDismiss:(BOOL)allowDismiss {
  _presentationControllerShouldDismiss = allowDismiss;
}

- (void)mediator:(DriveFilePickerMediator*)mediator
    didActivateSearch:(BOOL)searchActivated {
  _navigationController.sheetPresentationController.prefersGrabberVisible =
      !searchActivated &&
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE);
}

#pragma mark - BrowseDriveFilePickerCoordinatorDelegate

- (void)coordinatorShouldStop:(ChromeCoordinator*)coordinator {
  CHECK(coordinator == _childBrowseCoordinator);
  [_childBrowseCoordinator stop];
  _childBrowseCoordinator = nil;
  [_mediator setActive:YES];
}

- (void)browseDriveFilePickerCoordinator:
            (BrowseDriveFilePickerCoordinator*)coordinator
                         didUpdateFilter:(DriveFilePickerFilter)filter
                         sortingCriteria:(DriveItemsSortingType)sortingCriteria
                        sortingDirection:
                            (DriveItemsSortingOrder)sortingDirection
                     ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes {
  [_mediator setPendingFilter:filter
              sortingCriteria:sortingCriteria
             sortingDirection:sortingDirection
          ignoreAcceptedTypes:ignoreAcceptedTypes];
}

- (void)coordinatorDidTapAddAccount:(ChromeCoordinator*)coordinator {
  [self showAddAccount];
}

- (void)coordinator:(ChromeCoordinator*)coordinator
    didAllowDismiss:(BOOL)allowDismiss {
  _presentationControllerShouldDismiss = allowDismiss;
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

#pragma mark - Private

// Initiate the add account flow.
- (void)showAddAccount {
  __weak __typeof(self) weakSelf = self;
  id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  ShowSigninCommand* addAccountCommand = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kAddAccount
               identity:nil
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_DRIVE_FILE_PICKER_IOS
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
             completion:^(SigninCoordinatorResult result,
                          SigninCompletionInfo* completionInfo) {
               if (result == SigninCoordinatorResultSuccess) {
                 [weakSelf addAndSelectNewIdentity:completionInfo.identity];
               } else {
                 [weakSelf reportAddingIdentityFailure];
               }
             }];
  [applicationCommandsHandler showSignin:addAccountCommand
                      baseViewController:_navigationController];
}

// Called when user interrupted a download/upload.
- (void)userInterrupted {
  _metricsHelper.userInterrupted = YES;
  [self stopAnimated];
}

// Stops the Drive file picker after animating its dismissal.
- (void)stopAnimated {
  __weak id<DriveFilePickerCommands> driveFilePickerHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         DriveFilePickerCommands);
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [driveFilePickerHandler hideDriveFilePicker];
                         }];
}

// Called when a tap is detected in the window. This should present the 'Discard
// selection' alert if the tap is outside the Drive file picker and if the
// presentation controller is not allowed to dismiss the Drive file picker by
// itself (`_presentationControllerShouldDismiss` is NO).
- (void)didTapToDismiss:(UITapGestureRecognizer*)sender {
  if (sender.state != UIGestureRecognizerStateEnded) {
    return;
  }
  CGPoint tapLocation = [sender locationInView:_navigationController.view];
  if ([_navigationController.view pointInside:tapLocation withEvent:nil]) {
    // If the tap occurred within the Drive file picker, ignore it.
    return;
  }
  if (_presentationControllerShouldDismiss) {
    // If the presentation controller should already dismiss when the user taps
    // outside the presented view controller, do nothing here as it will dismiss
    // itself.
    return;
  }
  // Otherwise present 'Discard selection' alert.
  [self showDiscardSelectionAlert];
}

// Shows the 'Discard selection' alert on top of `_navigationController`.
- (void)showDiscardSelectionAlert {
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock discardSelectionBlock = ^{
    [weakSelf userInterrupted];
  };
  UIAlertController* discardSelectionAlertController =
      DiscardSelectionAlertController(discardSelectionBlock, nil);
  [_navigationController presentViewController:discardSelectionAlertController
                                      animated:YES
                                    completion:nil];
}

// Updates the current identity with a new identity. The new identity can be an
// already registered or a newly added identity.
- (void)updateCurrentIdentityWithIdentity:(id<SystemIdentity>)identity {
  _currentIdentity = identity;
  [_navigationController popToRootViewControllerAnimated:YES];
  [_childBrowseCoordinator stop];
  _childBrowseCoordinator = nil;
  [_mediator setSelectedIdentity:identity];
}

// Adds a new identity to be the current identity.
- (void)addAndSelectNewIdentity:(id<SystemIdentity>)identity {
  CHECK(_mediator);
  [_metricsHelper reportAccountChangeWithSuccess:YES isAccountNew:YES];
  [self updateCurrentIdentityWithIdentity:identity];
}

// Reports adding a new identity failure.
- (void)reportAddingIdentityFailure {
  [_metricsHelper reportAccountChangeWithSuccess:NO isAccountNew:YES];
}

@end
