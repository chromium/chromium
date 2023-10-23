// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_coordinator.h"

#import <StoreKit/StoreKit.h>

#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/photos/photos_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/store_kit/store_kit_coordinator.h"
#import "ios/chrome/browser/store_kit/store_kit_coordinator_delegate.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_coordinator.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_mediator.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_mediator_delegate.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

@interface SaveToPhotosCoordinator () <AccountPickerCoordinatorDelegate,
                                       SaveToPhotosMediatorDelegate,
                                       StoreKitCoordinatorDelegate>

@end

@implementation SaveToPhotosCoordinator {
  GURL _imageURL;
  web::Referrer _referrer;
  base::WeakPtr<web::WebState> _webState;
  SaveToPhotosMediator* _mediator;
  AlertCoordinator* _alertCoordinator;
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
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  PhotosService* photosService =
      PhotosServiceFactory::GetForBrowserState(browserState);
  PrefService* prefService = browserState->GetPrefs();
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  _mediator =
      [[SaveToPhotosMediator alloc] initWithPhotosService:photosService
                                              prefService:prefService
                                    accountManagerService:accountManagerService
                                          identityManager:identityManager];
  _mediator.delegate = self;
  [_mediator startWithImageURL:_imageURL
                      referrer:_referrer
                      webState:_webState.get()];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  [_alertCoordinator stop];
  _alertCoordinator = nil;
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

- (void)showTryAgainOrCancelAlertWithTitle:(NSString*)title
                                   message:(NSString*)message
                             tryAgainTitle:(NSString*)tryAgainTitle
                            tryAgainAction:(ProceduralBlock)tryAgainAction
                               cancelTitle:(NSString*)cancelTitle
                              cancelAction:(ProceduralBlock)cancelAction {
  if (_alertCoordinator) {
    [_alertCoordinator stop];
    _alertCoordinator = nil;
  }

  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:title
                         message:message];
  [_alertCoordinator addItemWithTitle:tryAgainTitle
                               action:tryAgainAction
                                style:UIAlertActionStyleDefault
                            preferred:YES
                              enabled:YES];
  [_alertCoordinator addItemWithTitle:cancelTitle
                               action:cancelAction
                                style:UIAlertActionStyleCancel
                            preferred:NO
                              enabled:YES];
  [_alertCoordinator start];
}

- (void)hideTryAgainOrCancelAlert {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

- (void)showStoreKitWithProductIdentifier:(NSString*)productIdentifer
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

@end
