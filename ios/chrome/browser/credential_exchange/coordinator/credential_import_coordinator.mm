// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/coordinator/credential_import_coordinator.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/not_fatal_until.h"
#import "base/notreached.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/credential_exchange/coordinator/credential_import_mediator.h"
#import "ios/chrome/browser/credential_exchange/public/credential_import_stage.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_import_view_controller.h"
#import "ios/chrome/browser/data_import/public/credential_item_identifier.h"
#import "ios/chrome/browser/data_import/public/import_data_item.h"
#import "ios/chrome/browser/data_import/public/passkey_import_item.h"
#import "ios/chrome/browser/data_import/public/password_import_item.h"
#import "ios/chrome/browser/data_import/ui/data_import_credential_conflict_resolution_view_controller.h"
#import "ios/chrome/browser/data_import/ui/data_import_credential_conflict_resolution_view_controller_delegate.h"
#import "ios/chrome/browser/data_import/ui/data_import_invalid_credentials_view_controller.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/settings/ui_bundled/password/reauthentication/local_reauthentication_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/utils/password_auto_fill_status_manager.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/utils/credential_provider_settings_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webauthn/coordinator/passkey_welcome_screen_coordinator.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface CredentialImportCoordinator () <
    CredentialImportMediatorDelegate,
    CredentialImportViewControllerDelegate,
    DataImportCredentialConflictResolutionViewControllerDelegate,
    LocalReauthenticationCoordinatorDelegate,
    PasskeyKeychainProviderBridgeDelegate,
    PasskeyWelcomeScreenCoordinatorDelegate>
@end

@implementation CredentialImportCoordinator {
  // Handles interaction with the model.
  CredentialImportMediator* _mediator;

  // Token received from the OS during app launch needed to receive credentials.
  NSUUID* _UUID;

  // Presents the `_viewController` controlled by this coordinator.
  UINavigationController* _navigationController;

  // The view controller for the import flow.
  CredentialImportViewController* _viewController;

  // Bridge to the PasskeyKeychainProvider that manages passkey vault keys.
  PasskeyKeychainProviderBridge* _passkeyKeychainProviderBridge;

  // Reauthentication module used in credential import flow.
  id<ReauthenticationProtocol> _reauthModule;

  // Coordinator for blocking credential import until Local Authentication is
  // passed. Used for requiring authentication when the app is
  // backgrounded/foregrounded with credential import opened.
  LocalReauthenticationCoordinator* _reauthCoordinator;

  // Coordinator for displaying alerts in the import flow.
  AlertCoordinator* _alertCoordinator;

  // Coordinator for displaying welcome screen for fetching trusted vault keys.
  PasskeyWelcomeScreenCoordinator* _passkeyWelcomeScreenCoordinator;

  // Provides status of password manager as iOS AutoFill credential provider.
  PasswordAutoFillStatusManager* _passwordAutoFillStatusManager;

  // Whether there is currently an ongoing action triggered by the primary
  // button tap, that should not be handled twice.
  BOOL _primaryActionInProgress;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      UUID:(NSUUID*)UUID
                              reauthModule:
                                  (id<ReauthenticationProtocol>)reauthModule {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _UUID = UUID;
    _reauthModule = reauthModule;
  }
  return self;
}

- (void)start {
  // Ensure that the status manager is initialized and has checked the status by
  // the time the import flow finishes.
  _passwordAutoFillStatusManager =
      [PasswordAutoFillStatusManager sharedManager];
  [_passwordAutoFillStatusManager checkAndUpdatePasswordAutoFillStatus];

  _viewController = [[CredentialImportViewController alloc] init];
  _viewController.delegate = self;
  ProfileIOS* profile = self.profile;
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      savedPasswordsPresenter =
          std::make_unique<password_manager::SavedPasswordsPresenter>(
              IOSChromeAffiliationServiceFactory::GetForProfile(profile),
              IOSChromeProfilePasswordStoreFactory::GetForProfile(
                  profile, ServiceAccessType::EXPLICIT_ACCESS),
              IOSChromeAccountPasswordStoreFactory::GetForProfile(
                  profile, ServiceAccessType::EXPLICIT_ACCESS),
              /*passkey_model=*/nullptr);
  _mediator = [[CredentialImportMediator alloc]
                 initWithUUID:_UUID
                     delegate:self
              identityManager:IdentityManagerFactory::GetForProfile(profile)
      savedPasswordsPresenter:std::move(savedPasswordsPresenter)
                 passkeyModel:IOSPasskeyModelFactory::GetForProfile(
                                  self.profile)
                faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                  profile)
                  syncService:SyncServiceFactory::GetForProfile(profile)
                  prefService:profile->GetPrefs()];
  _mediator.consumer = _viewController;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.navigationBarHidden = NO;
  _navigationController.toolbarHidden = NO;
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _mediator = nil;
  _navigationController = nil;
  _viewController = nil;
}

#pragma mark - CredentialImportMediatorDelegate

- (void)showImportScreen {
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
  [self startReauthCoordinator];
}

- (void)showNothingImportedScreen {
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_CREDENTIAL_EXCHANGE_NOTHING_IMPORTED_TITLE);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_CREDENTIAL_EXCHANGE_NOTHING_IMPORTED_MESSAGE);
  [self showAlertWithTitle:title
                   message:message
        baseViewController:self.baseViewController];
}

- (void)showNothingImportedEnterpriseScreen {
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_CREDENTIAL_EXCHANGE_NOTHING_IMPORTED_TITLE);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_CREDENTIAL_EXCHANGE_NOTHING_IMPORTED_ENTERPRISE_MESSAGE);
  [self showAlertWithTitle:title
                   message:message
        baseViewController:self.baseViewController];
}

- (void)showGenericError {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_EXCHANGE_GENERIC_ERROR_TITLE);
  [self showAlertWithTitle:title
                   message:nil
        baseViewController:self.baseViewController];
}

- (void)showConflictResolutionScreenWithPasswords:
            (NSArray<PasswordImportItem*>*)passwords
                                         passkeys:(NSArray<PasskeyImportItem*>*)
                                                      passkeys {
  // Wraps the conflict resolution view in a navigation controller to display
  // navigation bar and toolbar.
  DataImportCredentialConflictResolutionViewController*
      conflictResolutionViewController =
          [[DataImportCredentialConflictResolutionViewController alloc]
              initWithPasswordConflicts:passwords
                       passkeyConflicts:passkeys];
  conflictResolutionViewController.mutator = _mediator;
  conflictResolutionViewController.reauthModule = _reauthModule;
  conflictResolutionViewController.delegate = self;

  [_navigationController pushViewController:conflictResolutionViewController
                                   animated:YES];
}

#pragma mark - CredentialImportViewControllerDelegate

- (void)didTapPrimaryActionButton {
  if (_primaryActionInProgress) {
    return;
  }

  _primaryActionInProgress = YES;
  switch (_mediator.importStage) {
    case CredentialImportStage::kNotStarted: {
      // If no passkeys are being imported, there is no point in fetching the
      // trusted vault keys, proceed to start the importing process.
      if (!_mediator.importingPasskeys) {
        [_mediator startImportingCredentialsWithTrustedVaultKeys:{}];
        _primaryActionInProgress = NO;
        break;
      }

      bool metricsReportingEnabled =
          GetApplicationContext()->GetLocalState()->GetBoolean(
              metrics::prefs::kMetricsReportingEnabled);
      _passkeyKeychainProviderBridge = [[PasskeyKeychainProviderBridge alloc]
            initWithEnableLogging:metricsReportingEnabled
          navigationItemTitleView:
              password_manager::CreatePasswordManagerTitleView(
                  l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER))];
      _passkeyKeychainProviderBridge.delegate = self;

      CoreAccountInfo account =
          IdentityManagerFactory::GetForProfile(self.profile)
              ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
      __weak __typeof(self) weakSelf = self;
      auto completion_block = base::CallbackToBlock(base::BindOnce(
          [](__weak __typeof(self) weakSelf,
             webauthn::SharedKeyList trustedVaultKeys, NSError* error) {
            [weakSelf onTrustedVaultKeysFetched:std::move(trustedVaultKeys)
                                          error:error];
          },
          weakSelf));
      [_passkeyKeychainProviderBridge
          fetchTrustedVaultKeysForGaia:account.gaia.ToNSString()
                            credential:nil
                               purpose:webauthn::ReauthenticatePurpose::kEncrypt
                            completion:completion_block];
      break;
    }
    case CredentialImportStage::kImporting:
      NOTREACHED(base::NotFatalUntil::M153)
          << "Primary action button should be disabled";
      // This code should not be reached, but in case it is, ensure that the
      // further stages can proceed. Clean up when cleaning up not fatal until.
      _primaryActionInProgress = NO;
      break;
    case CredentialImportStage::kImported: {
      // On successful import, display the credential provider prompt, if the
      // AutoFill is not already enabled.
      if (!_passwordAutoFillStatusManager.ready ||
          _passwordAutoFillStatusManager.autoFillEnabled) {
        [_delegate credentialImportCoordinatorDidFinish:self];
        break;
      }

      // The completion handler of the OS library function might not run on
      // the main thread, ensure that the UI dismissal does.
      __weak __typeof(self) weakSelf = self;
      auto callback = base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(^(BOOL appWasEnabledForAutoFill) {
            [weakSelf
                handleTurnOnAutoFillPromptOutcome:appWasEnabledForAutoFill];
          }));
      [ASSettingsHelper
          requestToTurnOnCredentialProviderExtensionWithCompletionHandler:
              base::CallbackToBlock(std::move(callback))];
      break;
    }
  }
}

- (void)didTapDismissButton {
  [self.delegate credentialImportCoordinatorDidFinish:self];
}

- (void)didTapInfoButtonForType:(ImportDataItemType)type {
  switch (type) {
    case ImportDataItemType::kPasswords: {
      CHECK_GT(_mediator.invalidPasswords.count, 0u);
      [self presentInvalidCredentials:_mediator.invalidPasswords
                                 type:CredentialType::kPassword];
      break;
    }
    case ImportDataItemType::kPasskeys: {
      CHECK_GT(_mediator.invalidPasskeys.count, 0u);
      [self presentInvalidCredentials:_mediator.invalidPasskeys
                                 type:CredentialType::kPasskey];
      break;
    }
    case ImportDataItemType::kBookmarks:
    case ImportDataItemType::kHistory:
    case ImportDataItemType::kPayment:
      NOTREACHED() << "Those types are not supported in credential exchange";
  }
}

#pragma mark - DataImportCredentialConflictResolutionViewControllerDelegate

- (void)cancelledConflictResolution {
  [self.delegate credentialImportCoordinatorDidFinish:self];
}

- (void)resolvedCredentialConflicts {
  [_navigationController popToViewController:_viewController animated:YES];
}

#pragma mark - LocalReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (LocalReauthenticationCoordinator*)coordinator {
  // No-op.
}

- (void)dismissUIAfterFailedReauthenticationWithCoordinator:
    (LocalReauthenticationCoordinator*)coordinator {
  CHECK_EQ(_reauthCoordinator, coordinator);
  [self.delegate credentialImportCoordinatorDidFinish:self];
}

- (void)willPushReauthenticationViewController {
  // No-op.
}

#pragma mark - PasskeyKeychainProviderBridgeDelegate

- (void)performUserVerificationIfNeeded:(ProceduralBlock)completion {
  // TODO(crbug.com/450982128): Perform user verification.
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
  // Not actionable for credential import.
}

#pragma mark - PasskeyWelcomeScreenCoordinatorDelegate

- (void)passkeyWelcomeScreenCoordinatorWantsToBeDismissed:
    (PasskeyWelcomeScreenCoordinator*)coordinator {
  CHECK_EQ(_passkeyWelcomeScreenCoordinator, coordinator);
  [self dismissPasskeyWelcomeScreenWithCompletion:nil];
}

#pragma mark - Private

// Called when fetching trusted vault keys for passkeys finishes. If there are
// no unexpected errors and the keys are present, informs the mediator to start
// importing credentials.
- (void)onTrustedVaultKeysFetched:(webauthn::SharedKeyList)trustedVaultKeys
                            error:(NSError*)error {
  // First, dismiss welcome screens if there are any presented.
  if (_viewController.presentedViewController) {
    __weak __typeof(self) weakSelf = self;
    [self dismissPasskeyWelcomeScreenWithCompletion:^{
      [weakSelf onTrustedVaultKeysFetched:std::move(trustedVaultKeys)
                                    error:error];
    }];
    return;
  }

  // Display an alert if there is a real error (not just user cancellation).
  if (trustedVaultKeys.empty() && error &&
      error.code != webauthn::kErrorUserDismissedGPMPinFlow) {
    NSString* title =
        l10n_util::GetNSString(IDS_IOS_CREDENTIAL_EXCHANGE_GENERIC_ERROR_TITLE);
    [self showAlertWithTitle:title
                     message:nil
          baseViewController:_viewController];
    return;
  }

  if (!trustedVaultKeys.empty()) {
    [_mediator
        startImportingCredentialsWithTrustedVaultKeys:std::move(
                                                          trustedVaultKeys)];
  }

  _primaryActionInProgress = NO;
}

// Presents the invalid credentials view for `credentials` with `type`.
- (void)presentInvalidCredentials:(NSArray<CredentialImportItem*>*)credentials
                             type:(CredentialType)type {
  if (_viewController.presentedViewController) {
    return;
  }
  DataImportInvalidCredentialsViewController* invalidCredentialsViewController =
      [[DataImportInvalidCredentialsViewController alloc]
          initWithInvalidCredentials:credentials
                                type:type];
  [_viewController
      presentViewController:
          [[UINavigationController alloc]
              initWithRootViewController:invalidCredentialsViewController]
                   animated:YES
                 completion:nil];
}

// Starts reauthCoordinator. Once started, it observes scene state changes and
// requires authentication when the scene is backgrounded and then foregrounded
// while credential import is opened.
// TODO(crbug.com/458733320): Explore EG test feasibility.
- (void)startReauthCoordinator {
  _reauthCoordinator = [[LocalReauthenticationCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser
                reauthenticationModule:_reauthModule
                           authOnStart:NO];
  _reauthCoordinator.delegate = self;
  [_reauthCoordinator start];
}

// Starts the alert coordinator with `title` and `message` and initializes it
// with `baseViewController`.
- (void)showAlertWithTitle:(NSString*)title
                   message:(NSString*)message
        baseViewController:(UIViewController*)baseViewController {
  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:baseViewController
                                                   browser:self.browser
                                                     title:title
                                                   message:message];
  __weak id<CredentialImportCoordinatorDelegate> weakDelegate = _delegate;
  __weak __typeof(self) weakSelf = self;
  [_alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CLOSE)
                action:^{
                  [weakDelegate credentialImportCoordinatorDidFinish:weakSelf];
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

// Handles outcome of user's choice in the credential provider promo prompt.
- (void)handleTurnOnAutoFillPromptOutcome:(BOOL)appWasEnabledForAutoFill {
  RecordTurnOnCredentialProviderExtensionPromptOutcome(
      TurnOnCredentialProviderExtensionPromptSource::kCredentialImport,
      appWasEnabledForAutoFill);

  [_delegate credentialImportCoordinatorDidFinish:self];
}

@end
