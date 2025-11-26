// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/coordinator/credential_export_mediator.h"

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exporter.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_group_identifier.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_view_controller_items.h"

@implementation CredentialExportMediator {
  // Used as a presentation anchor for OS views. Must not be nil.
  UIWindow* _window;

  // Used to fetch the user's saved passwords for export.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;

  // Responsible for interaction with the credential export OS libraries.
  CredentialExporter* _credentialExporter;

  // Provides access to stored WebAuthn credentials.
  raw_ptr<webauthn::PasskeyModel> _passkeyModel;
}

- (instancetype)initWithWindow:(UIWindow*)window
       savedPasswordsPresenter:
           (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter
                  passkeyModel:(webauthn::PasskeyModel*)passkeyModel {
  self = [super init];
  if (self) {
    _window = window;
    _savedPasswordsPresenter = savedPasswordsPresenter;
    _passkeyModel = passkeyModel;
  }
  return self;
}

- (void)setConsumer:(id<CredentialExportConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer
      setAffiliatedGroups:_savedPasswordsPresenter->GetAffiliatedGroups()];
}

#pragma mark - CredentialExportViewControllerPresentationDelegate

- (void)userDidStartExport:(NSArray<CredentialGroupIdentifier*>*)selectedItems {
  std::vector<password_manager::CredentialUIEntry> passwordsToExport;
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeysToExport;

  for (CredentialGroupIdentifier* item in selectedItems) {
    password_manager::AffiliatedGroup group = item.affiliatedGroup;
    for (const password_manager::CredentialUIEntry& credential :
         group.GetCredentials()) {
      if (credential.passkey_credential_id.empty()) {
        passwordsToExport.push_back(credential);
      } else {
        std::string credentialId(credential.passkey_credential_id.begin(),
                                 credential.passkey_credential_id.end());
        std::string rpId = credential.rp_id;

        std::optional<sync_pb::WebauthnCredentialSpecifics> passkey =
            _passkeyModel->GetPasskeyByCredentialId(rpId, credentialId);

        if (passkey.has_value() && !passkey->hidden()) {
          passkeysToExport.push_back(*std::move(passkey));
        }
      }
    }
  }

  if (passkeysToExport.empty()) {
    [self startExportWithSecurityDomainSecrets:nil
                                     passwords:std::move(passwordsToExport)
                                      passkeys:std::move(passkeysToExport)];
  } else {
    __weak __typeof(self) weakSelf = self;

    base::OnceCallback<void(NSArray<NSData*>*)> fetchSecretsCallback =
        base::BindOnce(
            [](__weak CredentialExportMediator* mediator,
               std::vector<password_manager::CredentialUIEntry> passwords,
               std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
               NSArray<NSData*>* secrets) {
              [mediator
                  startExportWithSecurityDomainSecrets:secrets
                                             passwords:std::move(passwords)
                                              passkeys:std::move(passkeys)];
            },
            weakSelf, std::move(passwordsToExport),
            std::move(passkeysToExport));

    void (^completionBlock)(NSArray<NSData*>*) =
        base::CallbackToBlock(std::move(fetchSecretsCallback));

    [self.delegate fetchSecurityDomainSecretsWithCompletion:completionBlock];
  }
}

#pragma mark - Private

- (void)
    startExportWithSecurityDomainSecrets:
        (NSArray<NSData*>*)securityDomainSecrets
                               passwords:
                                   (std::vector<
                                       password_manager::CredentialUIEntry>)
                                       passwords
                                passkeys:
                                    (std::vector<
                                        sync_pb::WebauthnCredentialSpecifics>)
                                        passkeys {
  if (@available(iOS 26, *)) {
    _credentialExporter = [[CredentialExporter alloc] initWithWindow:_window];

    [_credentialExporter startExportWithPasswords:std::move(passwords)
                                         passkeys:std::move(passkeys)
                            securityDomainSecrets:securityDomainSecrets];
  }
}

@end
