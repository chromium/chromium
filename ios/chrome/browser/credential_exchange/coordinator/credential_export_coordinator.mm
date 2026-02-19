// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/coordinator/credential_export_coordinator.h"

#import <string>
#import <vector>

#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/browser/credential_exchange/coordinator/credential_export_mediator.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_view_controller.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/passwords/coordinator/password_export_handler.h"
#import "ios/chrome/browser/passwords/coordinator/password_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webauthn/coordinator/passkey_welcome_screen_coordinator.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface CredentialExportCoordinator () <
    CredentialExportMediatorDelegate,
    PasskeyKeychainProviderBridgeDelegate,
    PasskeyWelcomeScreenCoordinatorDelegate,
    PasswordExportHandler>
@end

@implementation CredentialExportCoordinator {
  // Displays a view allowing the user to select credentials to export.
  CredentialExportViewController* _viewController;

  // Handles interaction with the credential export OS libraries.
  CredentialExportMediator* _mediator;

  // Bridge to the PasskeyKeychainProvider that manages passkey vault keys.
  PasskeyKeychainProviderBridge* _passkeyKeychainProviderBridge;

  // All credential groups that can be exported. Only valid until `start`, at
  // which point it is moved from and should not be accessed.
  std::vector<password_manager::AffiliatedGroup> _affiliatedGroups;

  // Module handling reauthentication before accessing sensitive data.
  ReauthenticationModule* _reauthModule;

  // Alert for "Preparing Passwords" state of CSV export.
  UIAlertController* _preparingPasswordsAlert;

  // Displays passkey GPM PIN flows, if needed.
  PasskeyWelcomeScreenCoordinator* _passkeyWelcomeScreenCoordinator;

  // Coordinator for displaying alerts in the export flow.
  AlertCoordinator* _alertCoordinator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                    affiliatedGroups:
                        (std::vector<password_manager::AffiliatedGroup>)
                            affiliatedGroups {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _affiliatedGroups = std::move(affiliatedGroups);
  }
  return self;
}

- (void)start {
  _viewController = [[CredentialExportViewController alloc] init];
  ProfileIOS* profile = self.profile;
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(profile);
  _reauthModule = password_manager::BuildReauthenticationModule();
  _mediator = [[CredentialExportMediator alloc]
              initWithWindow:_baseNavigationController.view.window
            affiliatedGroups:std::move(_affiliatedGroups)
                passkeyModel:IOSPasskeyModelFactory::GetForProfile(profile)
               faviconLoader:faviconLoader
      reauthenticationModule:_reauthModule
               exportHandler:self
                 syncService:SyncServiceFactory::GetForProfile(profile)
             identityManager:IdentityManagerFactory::GetForProfile(profile)];
  _affiliatedGroups = {};
  _viewController.delegate = _mediator;
  _mediator.delegate = self;
  _mediator.consumer = _viewController;
  _viewController.faviconProvider = _mediator;

  [_baseNavigationController pushViewController:_viewController animated:YES];
}

#pragma mark - PasskeyKeychainProviderBridgeDelegate

- (void)performUserVerificationIfNeeded:(ProceduralBlock)completion {
  // TODO(crbug.com/449701042): Perform user verification.
  completion();
}

- (void)showWelcomeScreenWithPurpose:
            (webauthn::PasskeyWelcomeScreenPurpose)purpose
                          completion:
                              (webauthn::PasskeyWelcomeScreenAction)completion {
  _passkeyWelcomeScreenCoordinator = [[PasskeyWelcomeScreenCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                         purpose:purpose
                      completion:completion];
  _passkeyWelcomeScreenCoordinator.delegate = self;
  [_passkeyWelcomeScreenCoordinator start];
}

- (void)providerDidCompleteReauthentication {
}

#pragma mark - PasskeyWelcomeScreenCoordinatorDelegate

- (void)passkeyWelcomeScreenCoordinatorWantsToBeDismissed:
    (PasskeyWelcomeScreenCoordinator*)coordinator {
  CHECK_EQ(_passkeyWelcomeScreenCoordinator, coordinator);
  [self dismissPasskeyWelcomeScreenWithCompletion:nil];
}

#pragma mark - CredentialExportMediatorDelegate

- (void)fetchTrustedVaultKeysWithCompletion:
    (void (^)(webauthn::SharedKeyList))completion {
  CHECK(completion);
  bool metricsReportingEnabled =
      GetApplicationContext()->GetLocalState()->GetBoolean(
          metrics::prefs::kMetricsReportingEnabled);
  _passkeyKeychainProviderBridge = [[PasskeyKeychainProviderBridge alloc]
        initWithEnableLogging:metricsReportingEnabled
      navigationItemTitleView:password_manager::CreatePasswordManagerTitleView(
                                  l10n_util::GetNSString(
                                      IDS_IOS_PASSWORD_MANAGER))];
  _passkeyKeychainProviderBridge.delegate = self;

  CoreAccountInfo account =
      IdentityManagerFactory::GetForProfile(self.profile)
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  __weak __typeof(self) weakSelf = self;
  auto completion_block = base::CallbackToBlock(base::BindOnce(
      [](__weak __typeof(self) weakSelf,
         void (^completion)(webauthn::SharedKeyList),
         webauthn::SharedKeyList trustedVaultKeys, NSError* error) {
        [weakSelf onTrustedVaultKeysFetched:std::move(trustedVaultKeys)
                                      error:error
                                 completion:completion];
      },
      weakSelf, completion));
  [_passkeyKeychainProviderBridge
      fetchTrustedVaultKeysForGaia:account.gaia.ToNSString()
                        credential:nil
                           purpose:webauthn::ReauthenticatePurpose::kDecrypt
                        completion:completion_block];
}

- (void)showGenericError {
  [self showErrorAlert];
}

#pragma mark - PasswordExportHandler

- (void)showActivityViewWithActivityItems:(NSArray*)activityItems
                        completionHandler:(void (^)(NSString* activityType,
                                                    BOOL completed,
                                                    NSArray* returnedItems,
                                                    NSError* activityError))
                                              completionHandler {
  UIActivityViewController* activityViewController =
      [[UIActivityViewController alloc] initWithActivityItems:activityItems
                                        applicationActivities:nil];

  NSArray* excludedActivityTypes = @[
    UIActivityTypeAddToReadingList, UIActivityTypeAirDrop,
    UIActivityTypeCopyToPasteboard, UIActivityTypeOpenInIBooks,
    UIActivityTypePostToFacebook, UIActivityTypePostToFlickr,
    UIActivityTypePostToTencentWeibo, UIActivityTypePostToTwitter,
    UIActivityTypePostToVimeo, UIActivityTypePostToWeibo, UIActivityTypePrint
  ];
  activityViewController.excludedActivityTypes = excludedActivityTypes;

  activityViewController.completionWithItemsHandler = completionHandler;

  activityViewController.popoverPresentationController.sourceView =
      _viewController.view;
  activityViewController.popoverPresentationController.sourceRect =
      _viewController.view.bounds;

  [self presentViewControllerForExportFlow:activityViewController];
}

- (void)showExportErrorAlertWithLocalizedReason:(NSString*)localizedReason {
  // TODO(crbug.com/470440092): Implement alerts to be displayed when exporting
  // selected passwords to csv.
}

- (void)showPreparingPasswordsAlert {
  // TODO(crbug.com/470440092): Implement alerts to be displayed when exporting
  // selected passwords to csv.
}

- (void)showSetPasscodeForPasswordExportDialog {
  // TODO(crbug.com/470440092): Implement alerts to be displayed when exporting
  // selected passwords to csv.
}

#pragma mark - Private

- (void)presentViewControllerForExportFlow:(UIViewController*)viewController {
  // TODO(crbug.com/470440092): Once showPreparingPasswordsAlert is implemented,
  // check here if _preparingPasswordsAlert needs dismissal.
  [_viewController presentViewController:viewController
                                animated:YES
                              completion:nil];
}

// Called when fetching trusted vault keys for passkeys finishes. If there are
// no unexpected errors and the keys are present, calls `completion`
- (void)onTrustedVaultKeysFetched:(webauthn::SharedKeyList)trustedVaultKeys
                            error:(NSError*)error
                       completion:
                           (void (^)(webauthn::SharedKeyList))completion {
  // First, dismiss welcome screens if there are any presented.
  if (_viewController.presentedViewController) {
    __weak __typeof(self) weakSelf = self;
    [self dismissPasskeyWelcomeScreenWithCompletion:^{
      [weakSelf onTrustedVaultKeysFetched:std::move(trustedVaultKeys)
                                    error:error
                               completion:completion];
    }];
    return;
  }

  // Display an alert if there is a real error (not just user cancellation).
  if (trustedVaultKeys.empty() && error &&
      error.code != webauthn::kErrorUserDismissedGPMPinFlow) {
    [self showErrorAlert];
    return;
  }

  if (!trustedVaultKeys.empty()) {
    completion(std::move(trustedVaultKeys));
  }
}

// Starts the alert coordinator displaying a generic error.
- (void)showErrorAlert {
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_CREDENTIAL_EXCHANGE_EXPORT_GENERIC_ERROR_TITLE);
  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:_viewController.presentedViewController
                         browser:self.browser
                           title:title
                         message:nil];
  __weak id<CredentialExportCoordinatorDelegate> weakDelegate = _delegate;
  __weak __typeof(self) weakSelf = self;
  [_alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CLOSE)
                action:^{
                  [weakDelegate credentialExportCoordinatorDidFinish:weakSelf];
                }
                 style:UIAlertActionStyleCancel];
  [_alertCoordinator start];
}

// Dismisses the passkey welcome screen with `completion`.
- (void)dismissPasskeyWelcomeScreenWithCompletion:(ProceduralBlock)completion {
  [_passkeyWelcomeScreenCoordinator stopWithCompletion:completion];
  _passkeyWelcomeScreenCoordinator.delegate = nil;
  _passkeyWelcomeScreenCoordinator = nil;
}

@end
