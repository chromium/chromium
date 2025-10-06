// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/credential_export_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/settings/ui_bundled/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webauthn/coordinator/credential_export_mediator.h"
#import "ios/chrome/browser/webauthn/ui/credential_export_view_controller.h"
#import "ios/chrome/browser/webauthn/ui/credential_export_view_controller_presentation_delegate.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface CredentialExportCoordinator () <
    CredentialExportViewControllerPresentationDelegate,
    PasskeyKeychainProviderBridgeDelegate>
@end

@implementation CredentialExportCoordinator {
  // Displays a view allowing the user to select credentials to export.
  CredentialExportViewController* _viewController;

  // Handles interaction with the credential export OS libraries.
  CredentialExportMediator* _mediator;

  // Used to fetch the user's saved passwords for export.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;

  // Bridge to the PasskeyKeychainProvider that manages passkey vault keys.
  PasskeyKeychainProviderBridge* _passkeyKeychainProviderBridge;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                         savedPasswordsPresenter:
                             (password_manager::SavedPasswordsPresenter*)
                                 savedPasswordsPresenter {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _savedPasswordsPresenter = savedPasswordsPresenter;
  }
  return self;
}

- (void)start {
  _viewController = [[CredentialExportViewController alloc] init];
  _viewController.delegate = self;

  _mediator = [[CredentialExportMediator alloc]
               initWithWindow:_baseNavigationController.view.window
      savedPasswordsPresenter:_savedPasswordsPresenter];

  [_baseNavigationController pushViewController:_viewController animated:YES];
}

#pragma mark - CredentialExportViewControllerPresentationDelegate

- (void)userDidStartExport {
  // TODO(crbug.com/449701042): Only fetch keys if there are selected passkeys.
  _passkeyKeychainProviderBridge = [[PasskeyKeychainProviderBridge alloc]
        initWithEnableLogging:NO
         navigationController:_baseNavigationController
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
                               // TODO(crbug.com/449701042): Consider adding new
                               // reauth purpose.
                               purpose:PasskeyKeychainProvider::
                                           ReauthenticatePurpose::kEncrypt
                            completion:^(
                                NSArray<NSData*>* securityDomainSecrets) {
                              [weakSelf startExportWithSecurityDomainSecrets:
                                            securityDomainSecrets];
                            }];
}

#pragma mark - PasskeyKeychainProviderBridgeDelegate

- (void)performUserVerificationIfNeeded:(ProceduralBlock)completion {
  // TODO(crbug.com/449701042): Perform user verification.
  completion();
}

// TODO(crbug.com/449701042): Display all types of welcome views.
- (void)showEnrollmentWelcomeScreen:(ProceduralBlock)enrollBlock {
  enrollBlock();
}

- (void)showFixDegradedRecoverabilityWelcomeScreen:
    (ProceduralBlock)fixDegradedRecoverabilityBlock {
  fixDegradedRecoverabilityBlock();
}

- (void)showReauthenticationWelcomeScreen:(ProceduralBlock)reauthenticateBlock {
  reauthenticateBlock();
}

- (void)providerDidCompleteReauthentication {
}

#pragma mark - Private

- (void)startExportWithSecurityDomainSecrets:
    (NSArray<NSData*>*)securityDomainSecrets {
  if (@available(iOS 26, *)) {
    [_mediator startExportWithSecurityDomainSecrets:securityDomainSecrets];
  }
}

@end
