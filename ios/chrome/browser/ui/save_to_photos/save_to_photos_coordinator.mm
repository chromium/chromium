// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_coordinator.h"

#import <StoreKit/StoreKit.h>

#import "base/metrics/user_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_coordinator.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_coordinator_delegate.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_logger.h"
#import "ios/chrome/browser/photos/model/photos_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/manage_storage_alert_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_mediator.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_mediator_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

@interface SaveToPhotosCoordinator () <AccountPickerCoordinatorDelegate,
                                       AccountPickerLogger,
                                       ManageStorageAlertCommands,
                                       SaveToPhotosMediatorDelegate,
                                       StoreKitCoordinatorDelegate>

@end

@implementation SaveToPhotosCoordinator {
  GURL _imageURL;
  web::Referrer _referrer;
  base::WeakPtr<web::WebState> _webState;
  SaveToPhotosMediator* _mediator;
  UIAlertController* _alertController;
  StoreKitCoordinator* _storeKitCoordinator;
  AccountPickerCoordinator* _accountPickerCoordinator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  imageURL:(const GURL&)imageURL
                                  referrer:(const web::Referrer&)referrer
                                  webState:(web::WebState*)webState {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _imageURL = imageURL;
    _referrer = referrer;
    CHECK(webState);
    _webState = webState->GetWeakPtr();
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:self
                           forProtocol:@protocol(ManageStorageAlertCommands)];
  id<ManageStorageAlertCommands> manageStorageAlertHandler =
      HandlerForProtocol(dispatcher, ManageStorageAlertCommands);
  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  ProfileIOS* profile = self.browser->GetProfile();
  PhotosService* photosService = PhotosServiceFactory::GetForProfile(profile);
  PrefService* prefService = profile->GetPrefs();
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  _mediator = [[SaveToPhotosMediator alloc]
          initWithPhotosService:photosService
                    prefService:prefService
          accountManagerService:accountManagerService
                identityManager:identityManager
      manageStorageAlertHandler:manageStorageAlertHandler
             applicationHandler:applicationHandler];
  _mediator.delegate = self;
  [_mediator startWithImageURL:_imageURL
                      referrer:_referrer
                      webState:_webState.get()];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [_mediator disconnect];
  _mediator = nil;
  [_alertController.presentingViewController dismissViewControllerAnimated:NO
                                                                completion:nil];
  _alertController = nil;
  [_storeKitCoordinator stop];
  _storeKitCoordinator = nil;
  [_accountPickerCoordinator stopAnimated:NO];
  _accountPickerCoordinator = nil;
}

#pragma mark - SaveToPhotosMediatorDelegate

- (void)showAccountPickerWithConfiguration:
            (AccountPickerConfiguration*)configuration
                          selectedIdentity:
                              (id<SystemIdentity>)selectedIdentity {
  _accountPickerCoordinator = [[AccountPickerCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                   configuration:configuration];
  _accountPickerCoordinator.delegate = self;
  _accountPickerCoordinator.logger = self;
  [_accountPickerCoordinator start];
  if (selectedIdentity) {
    // If the mediator does not want to override the selected identity, leave
    // the one presented by default by the account picker.
    _accountPickerCoordinator.selectedIdentity = selectedIdentity;
  }
}

- (void)hideAccountPicker {
  [_accountPickerCoordinator stopAnimated:YES];
}

- (void)startValidationSpinnerForAccountPicker {
  [_accountPickerCoordinator startValidationSpinner];
}

- (void)stopValidationSpinnerForAccountPicker {
  [_accountPickerCoordinator stopValidationSpinner];
}

- (void)showTryAgainOrCancelAlertWithTitle:(NSString*)title
                                   message:(NSString*)message
                             tryAgainTitle:(NSString*)tryAgainTitle
                            tryAgainAction:(ProceduralBlock)tryAgainBlock
                               cancelTitle:(NSString*)cancelTitle
                              cancelAction:(ProceduralBlock)cancelBlock {
  if (_alertController) {
    [_alertController.presentingViewController
        dismissViewControllerAnimated:NO
                           completion:nil];
  }
  _alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* tryAgainAction =
      [UIAlertAction actionWithTitle:tryAgainTitle
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action) {
                               if (tryAgainBlock) {
                                 tryAgainBlock();
                               }
                             }];
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:cancelTitle
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action) {
                               if (cancelBlock) {
                                 cancelBlock();
                               }
                             }];
  [_alertController addAction:tryAgainAction];
  [_alertController addAction:cancelAction];
  [_alertController setPreferredAction:tryAgainAction];
  UIViewController* alertBaseViewController =
      _accountPickerCoordinator.viewController;
  if (!alertBaseViewController) {
    alertBaseViewController = self.baseViewController;
  }
  CHECK(alertBaseViewController);
  [alertBaseViewController presentViewController:_alertController
                                        animated:YES
                                      completion:nil];
}

- (void)showStoreKitWithProductIdentifier:(NSString*)productIdentifer
                            providerToken:(NSString*)providerToken
                            campaignToken:(NSString*)campaignToken {
  if (_storeKitCoordinator) {
    [_storeKitCoordinator stop];
    _storeKitCoordinator = nil;
  }

  _storeKitCoordinator = [[StoreKitCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];
  _storeKitCoordinator.delegate = self;
  _storeKitCoordinator.iTunesProductParameters = @{
    SKStoreProductParameterITunesItemIdentifier : productIdentifer,
    SKStoreProductParameterProviderToken : providerToken,
    SKStoreProductParameterCampaignToken : campaignToken
  };
  [_storeKitCoordinator start];
}

- (void)hideStoreKit {
  [_storeKitCoordinator stop];
  _storeKitCoordinator = nil;
}

- (void)showSnackbarWithMessage:(NSString*)message
                     buttonText:(NSString*)buttonText
                  messageAction:(ProceduralBlock)messageAction
               completionAction:(void (^)(BOOL))completionAction {
  id<SnackbarCommands> snackbarHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  [snackbarHandler showSnackbarWithMessage:message
                                buttonText:buttonText
                             messageAction:messageAction
                          completionAction:completionAction];
}

// Hide Save to Photos UI synchronously.
- (void)hideSaveToPhotos {
  id<SaveToPhotosCommands> saveToPhotosHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SaveToPhotosCommands);
  [saveToPhotosHandler stopSaveToPhotos];
}

#pragma mark - StoreKitCoordinatorDelegate

- (void)storeKitCoordinatorWantsToStop:(StoreKitCoordinator*)coordinator {
  [_mediator storeKitWantsToHide];
}

#pragma mark - AccountPickerCoordinatorDelegate

- (void)accountPickerCoordinator:
            (AccountPickerCoordinator*)accountPickerCoordinator
    openAddAccountWithCompletion:(void (^)(id<SystemIdentity>))completion {
  id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  ShowSigninCommand* addAccountCommand = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kAddAccount
               identity:nil
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_SAVE_TO_PHOTOS_IOS
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:^(SigninCoordinatorResult result,
                          SigninCompletionInfo* completionInfo) {
                 if (completion) {
                   completion(completionInfo.identity);
                 }
               }];
  [applicationCommandsHandler
              showSignin:addAccountCommand
      baseViewController:accountPickerCoordinator.viewController];
}

- (void)accountPickerCoordinator:
            (AccountPickerCoordinator*)accountPickerCoordinator
               didSelectIdentity:(id<SystemIdentity>)identity
                    askEveryTime:(BOOL)askEveryTime {
  [_mediator accountPickerDidSelectIdentity:identity askEveryTime:askEveryTime];
}

- (void)accountPickerCoordinatorCancel:
    (AccountPickerCoordinator*)accountPickerCoordinator {
  [_mediator accountPickerDidCancel];
}

- (void)accountPickerCoordinatorAllIdentityRemoved:
    (AccountPickerCoordinator*)accountPickerCoordinator {
  [self hideSaveToPhotos];
}

- (void)accountPickerCoordinatorDidStop:
    (AccountPickerCoordinator*)accountPickerCoordinator {
  [_mediator accountPickerWasHidden];
  _accountPickerCoordinator = nil;
}

#pragma mark - AccountPickerLogger

- (void)logAccountPickerSelectionScreenOpened {
  base::RecordAction(base::UserMetricsAction(
      "MobileSaveToPhotosAccountPickerSelectionScreenOpened"));
}

- (void)logAccountPickerNewIdentitySelected {
  base::RecordAction(base::UserMetricsAction(
      "MobileSaveToPhotosAccountPickerNewIdentitySelected"));
}

- (void)logAccountPickerSelectionScreenClosed {
  base::RecordAction(base::UserMetricsAction(
      "MobileSaveToPhotosAccountPickerSelectionScreenClosed"));
}

- (void)logAccountPickerAddAccountScreenOpened {
  base::RecordAction(base::UserMetricsAction(
      "MobileSaveToPhotosAccountPickerAddAccountScreenOpened"));
}

- (void)logAccountPickerAddAccountCompleted {
  base::RecordAction(base::UserMetricsAction(
      "MobileSaveToPhotosAccountPickerAddAccountCompleted"));
}

#pragma mark - ManageStorageAlertCommands

- (void)showManageStorageAlertForIdentity:(id<SystemIdentity>)identity {
  if (_alertController) {
    [_alertController.presentingViewController
        dismissViewControllerAnimated:NO
                           completion:nil];
  }
  _alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_MANAGE_STORAGE_ALERT_TITLE)
                       message:l10n_util::GetNSString(
                                   IDS_IOS_MANAGE_STORAGE_ALERT_MESSAGE)
                preferredStyle:UIAlertControllerStyleAlert];
  __weak __typeof(_mediator) weakMediator = _mediator;
  UIAlertAction* manageStorageAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_MANAGE_STORAGE_ALERT_MANAGE_STORAGE_BUTTON)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                [weakMediator showManageStorageForIdentity:identity];
              }];
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action) {
                               [weakMediator manageStorageAlertDidCancel];
                             }];
  [_alertController addAction:manageStorageAction];
  [_alertController addAction:cancelAction];
  [_alertController setPreferredAction:manageStorageAction];
  UIViewController* alertBaseViewController =
      _accountPickerCoordinator.viewController;
  if (!alertBaseViewController) {
    alertBaseViewController = self.baseViewController;
  }
  CHECK(alertBaseViewController);
  [alertBaseViewController presentViewController:_alertController
                                        animated:YES
                                      completion:nil];
}

@end
