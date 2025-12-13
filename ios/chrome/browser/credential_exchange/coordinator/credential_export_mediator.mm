// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/coordinator/credential_export_mediator.h"

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "components/favicon_base/favicon_types.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exporter.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_group_identifier.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_view_controller_items.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

namespace {
// Favicon constants.
const CGFloat kFaviconSize = 24.0;
const CGFloat kMinFaviconSize = 16.0;

}  // namespace

@implementation CredentialExportMediator {
  // Used as a presentation anchor for OS views. Must not be nil.
  UIWindow* _window;

  // Responsible for interaction with the credential export OS libraries.
  CredentialExporter* _credentialExporter;

  // All credential groups that can be exported. Only valid until `setConsumer`,
  // at which point it is moved from and should not be accessed.
  std::vector<password_manager::AffiliatedGroup> _affiliatedGroups;

  // Provides access to stored WebAuthn credentials.
  raw_ptr<webauthn::PasskeyModel> _passkeyModel;

  // Service used to retrieve favicons.
  raw_ptr<FaviconLoader> _faviconLoader;
}

- (instancetype)initWithWindow:(UIWindow*)window
              affiliatedGroups:(std::vector<password_manager::AffiliatedGroup>)
                                   affiliatedGroups
                  passkeyModel:(webauthn::PasskeyModel*)passkeyModel
                 faviconLoader:(FaviconLoader*)faviconLoader {
  self = [super init];
  if (self) {
    _window = window;
    _affiliatedGroups = std::move(affiliatedGroups);
    _passkeyModel = passkeyModel;
    _faviconLoader = faviconLoader;
  }
  return self;
}

- (void)setConsumer:(id<CredentialExportConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer setAffiliatedGroups:std::move(_affiliatedGroups)];
  _affiliatedGroups = {};
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
            _passkeyModel->GetPasskey(
                rpId, credentialId,
                webauthn::PasskeyModel::ShadowedCredentials::kExclude);

        if (passkey.has_value() && !passkey->hidden()) {
          passkeysToExport.push_back(*std::move(passkey));
        }
      }
    }
  }

  if (passkeysToExport.empty()) {
    [self startExportWithTrustedVaultKeys:nil
                                passwords:std::move(passwordsToExport)
                                 passkeys:std::move(passkeysToExport)];
  } else {
    __weak __typeof(self) weakSelf = self;

    base::OnceCallback<void(NSArray<NSData*>*)> fetchSecretsCallback =
        base::BindOnce(
            [](__weak CredentialExportMediator* mediator,
               std::vector<password_manager::CredentialUIEntry> passwords,
               std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
               NSArray<NSData*>* trustedVaultKeys) {
              [mediator startExportWithTrustedVaultKeys:trustedVaultKeys
                                              passwords:std::move(passwords)
                                               passkeys:std::move(passkeys)];
            },
            weakSelf, std::move(passwordsToExport),
            std::move(passkeysToExport));

    void (^completionBlock)(NSArray<NSData*>*) =
        base::CallbackToBlock(std::move(fetchSecretsCallback));

    [self.delegate fetchTrustedVaultKeysWithCompletion:completionBlock];
  }
}

#pragma mark - Private

- (void)
    startExportWithTrustedVaultKeys:(NSArray<NSData*>*)trustedVaultKeys
                          passwords:
                              (std::vector<password_manager::CredentialUIEntry>)
                                  passwords
                           passkeys:(std::vector<
                                        sync_pb::WebauthnCredentialSpecifics>)
                                        passkeys {
  if (@available(iOS 26, *)) {
    _credentialExporter = [[CredentialExporter alloc] initWithWindow:_window];

    [_credentialExporter startExportWithPasswords:std::move(passwords)
                                         passkeys:std::move(passkeys)
                                 trustedVaultKeys:trustedVaultKeys];
  }
}

#pragma mark - CredentialExportFaviconProvider

- (void)fetchFaviconForURL:(const GURL&)URL
                completion:(void (^)(FaviconAttributes*, BOOL))completion {
  if (!_faviconLoader) {
    completion(nil, YES);
    return;
  }

  _faviconLoader->FaviconForPageUrl(
      URL, kFaviconSize, kMinFaviconSize,
      /*fallback_to_google_server=*/false,
      ^(FaviconAttributes* attributes, bool cached) {
        completion(attributes, cached);
      });
}

@end
