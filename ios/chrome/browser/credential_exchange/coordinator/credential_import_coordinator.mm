// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/coordinator/credential_import_coordinator.h"

#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/credential_exchange/coordinator/credential_import_mediator.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_import_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webauthn/public/passkey_welcome_screen_util.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_strings.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_view_controller.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface CredentialImportCoordinator () <
    CredentialImportMediatorDelegate,
    PasskeyKeychainProviderBridgeDelegate,
    PasskeyWelcomeScreenViewControllerDelegate,
    PromoStyleViewControllerDelegate>
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

  // Email of the signed in user account.
  std::string _userEmail;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      UUID:(NSUUID*)UUID {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _UUID = UUID;
  }
  return self;
}

- (void)start {
  _viewController = [[CredentialImportViewController alloc] init];
  _viewController.delegate = self;
  _userEmail = IdentityManagerFactory::GetForProfile(self.profile)
                   ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                   .email;
  _mediator = [[CredentialImportMediator alloc] initWithUUID:_UUID
                                                    delegate:self
                                                   userEmail:_userEmail];
  _mediator.consumer = _viewController;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.navigationBarHidden = NO;
}

#pragma mark - CredentialImportMediatorDelegate

- (void)showImportScreen {
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  // TODO(crbug.com/450982128): Skip fetching keys with no passkeys present.
  bool metricsReportingEnabled =
      GetApplicationContext()->GetLocalState()->GetBoolean(
          metrics::prefs::kMetricsReportingEnabled);
  _passkeyKeychainProviderBridge = [[PasskeyKeychainProviderBridge alloc]
        initWithEnableLogging:metricsReportingEnabled
         navigationController:_navigationController
      navigationItemTitleView:password_manager::CreatePasswordManagerTitleView(
                                  l10n_util::GetNSString(
                                      IDS_IOS_PASSWORD_MANAGER))];
  _passkeyKeychainProviderBridge.delegate = self;

  CoreAccountInfo account =
      IdentityManagerFactory::GetForProfile(self.profile)
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  __weak __typeof(self) weakSelf = self;
  [_passkeyKeychainProviderBridge
      fetchSecurityDomainSecretForGaia:account.gaia.ToNSString()
                            credential:nil
                               purpose:PasskeyKeychainProvider::
                                           ReauthenticatePurpose::kEncrypt
                            completion:^(
                                NSArray<NSData*>* securityDomainSecrets) {
                              [weakSelf onSecurityDomainSecretsFetched:
                                            securityDomainSecrets];
                            }];
}

#pragma mark - PasskeyKeychainProviderBridgeDelegate

- (void)performUserVerificationIfNeeded:(ProceduralBlock)completion {
  // TODO(crbug.com/450982128): Perform user verification.
  completion();
}

- (void)showEnrollmentWelcomeScreen:(ProceduralBlock)enrollBlock {
  CreateAndPresentPasskeyWelcomeScreen(PasskeyWelcomeScreenPurpose::kEnroll,
                                       _navigationController, /*delegate=*/self,
                                       enrollBlock, _userEmail);
}

- (void)showFixDegradedRecoverabilityWelcomeScreen:
    (ProceduralBlock)fixDegradedRecoverabilityBlock {
  CreateAndPresentPasskeyWelcomeScreen(
      PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability,
      _navigationController, /*delegate=*/self, fixDegradedRecoverabilityBlock,
      _userEmail);
}

- (void)showReauthenticationWelcomeScreen:(ProceduralBlock)reauthenticateBlock {
  CreateAndPresentPasskeyWelcomeScreen(
      PasskeyWelcomeScreenPurpose::kReauthenticate, _navigationController,
      /*delegate=*/self, reauthenticateBlock, _userEmail);
}

- (void)providerDidCompleteReauthentication {
  // TODO(crbug.com/450982128): Implement if needed.
}

#pragma mark - PasskeyWelcomeScreenViewControllerDelegate

- (void)passkeyWelcomeScreenViewControllerShouldBeDismissed:
    (PasskeyWelcomeScreenViewController*)passkeyWelcomeScreenViewController {
  [_navigationController popToViewController:_viewController animated:YES];
}

#pragma mark - Private

// Called when fetching security domain secrets for passkeys finishes. Dismisses
// screens that were presented for the fetching (if any). Informs mediator to
// start importing credentials.
- (void)onSecurityDomainSecretsFetched:
    (NSArray<NSData*>*)securityDomainSecrets {
  [_navigationController popToViewController:_viewController animated:YES];
  if (securityDomainSecrets.count == 0) {
    // TODO(crbug.com/450982128): Handle error.
    return;
  }

  [_mediator
      startImportingCredentialsWithSecurityDomainSecrets:securityDomainSecrets];
}

@end
