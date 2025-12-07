// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/root_drive_file_picker_coordinator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/browse_drive_file_picker_coordinator.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/browse_drive_file_picker_coordinator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_collection.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_image_fetcher.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_metrics_helper.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_alert_utils.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
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
  SigninCoordinator* _signinCoordinator;
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
  // The image fetcher.
  std::unique_ptr<DriveFilePickerImageFetcher> _imageFetcher;
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
    _presentationControllerShouldDismiss = YES;
  }
  return self;
}

- (void)start {
  ProfileIOS* profile = self.profile->GetOriginalProfile();
  _authenticationService = AuthenticationServiceFactory::GetForProfile(profile);
  _currentIdentity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  _imageFetcher = std::make_unique<DriveFilePickerImageFetcher>(
      profile->GetSharedURLLoaderFactory());
  _metricsHelper = [[DriveFilePickerMetricsHelper alloc] init];
  _viewController = [[DriveFilePickerTableViewController alloc] init];
  _viewController.driveFilePickerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DriveFilePickerCommands);
  _navigationController = [[DriveFilePickerNavigationController alloc]
      initWithRootViewController:_viewController];

  _mediator = [[DriveFilePickerMediator alloc]
      initWithWebState:_webState.get()
            collection:DriveFilePickerCollection::GetRoot(_currentIdentity)
               options:DriveFilePickerOptions::Default()];
  _mediator.delegate = self;
  _mediator.driveFilePickerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DriveFilePickerCommands);
  _mediator.driveService = drive::DriveServiceFactory::GetForProfile(profile);
  _mediator.identityManager = IdentityManagerFactory::GetForProfile(profile);
  _mediator.accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  _mediator.imageFetcher = _imageFetcher.get();
  _mediator.metricsHelper = _metricsHelper;

  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.presentationController.delegate = self;
  if (ui::GetDeviceFormFactor() ==
      ui::DeviceFormFactor::DEVICE_FORM_FACTOR_PHONE) {
    _navigationController.sheetPresentationController.prefersGrabberVisible =
        YES;
    // TODO(crbug.com/441764702): Add `mediumDetent` back.
    _navigationController.sheetPresentationController.detents = @[
      [UISheetPresentationControllerDetent largeDetent],
    ];
    _navigationController.sheetPresentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierLarge;
  } else {
    _navigationController.sheetPresentationController.prefersGrabberVisible =
        NO;
    _navigationController.sheetPresentationController.detents =
        @[ [UISheetPresentationControllerDetent largeDetent] ];
    _navigationController.sheetPresentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierLarge;
  }

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];

  _viewController.mutator = _mediator;
  _mediator.consumer = _viewController;

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
  [self stopSigninCoordinator];
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

- (void)browseDriveCollectionWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                               collection:
                                   (std::unique_ptr<DriveFilePickerCollection>)
                                       collection
                                  options:(DriveFilePickerOptions)options {
  [_mediator setActive:NO];
  _childBrowseCoordinator = [[BrowseDriveFilePickerCoordinator alloc]
      initWithBaseNavigationViewController:_navigationController
                                   browser:self.browser
                                  webState:_webState
                                collection:std::move(collection)
                              imageFetcher:_imageFetcher.get()
                                   options:options
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
                         didUpdateOptions:(DriveFilePickerOptions)options {
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
                        didUpdateOptions:(DriveFilePickerOptions)options {
  [_mediator setPendingOptions:options];
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

- (void)stopSigninCoordinator {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
}

- (void)addAccountCompletionWithCoordinator:(SigninCoordinator*)coordinator
                                     result:(SigninCoordinatorResult)result
                         completionIdentity:
                             (id<SystemIdentity>)completionIdentity {
  CHECK_EQ(_signinCoordinator, coordinator, base::NotFatalUntil::M151);
  if (result == SigninCoordinatorResultSuccess) {
    [self addAndSelectNewIdentity:completionIdentity];
  } else {
    [self reportAddingIdentityFailure];
  }
  [self stopSigninCoordinator];
}

// Initiate the add account flow.
- (void)showAddAccount {
  if (_signinCoordinator.viewWillPersist) {
    return;
  }
  [_signinCoordinator stop];
  __weak __typeof(self) weakSelf = self;
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::kDriveFilePickerIos;
  SigninContextStyle contextStyle = SigninContextStyle::kDefault;
  _signinCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:_navigationController
                                          browser:signin::GetRegularBrowser(
                                                      self.browser)
                                     contextStyle:contextStyle
                                      accessPoint:accessPoint
                                   prefilledEmail:nil
                             continuationProvider:
                                 DoNothingContinuationProvider()];
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> completionIdentity) {
        [weakSelf addAccountCompletionWithCoordinator:coordinator
                                               result:result
                                   completionIdentity:completionIdentity];
      };
  [_signinCoordinator start];
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
  [_mediator setCollection:DriveFilePickerCollection::GetRoot(identity)];
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
