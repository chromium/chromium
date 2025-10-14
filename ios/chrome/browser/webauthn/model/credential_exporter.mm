// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/credential_exporter.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "ios/chrome/browser/webauthn/model/credential_exchange_passkey.h"
#import "ios/chrome/browser/webauthn/model/credential_exchange_password.h"
#import "ios/chrome/browser/webauthn/model/credential_export_manager_swift.h"
#import "net/base/apple/url_conversions.h"

@implementation CredentialExporter {
  // Used as a presentation anchor for OS views. Must not be nil.
  UIWindow* _window;

  // Exports credentials through the OS ASCredentialExportManager API.
  CredentialExportManager* _credentialExportManager;

  // Used to fetch the user's saved passwords for export.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;

  // Secrets for passkey security domain needed for decryption.
  // TODO(crbug.com/444112223): Ensure this is in memory only for decryption.
  NSArray<NSData*>* _securityDomainSecrets;

  // Provides access to stored WebAuthn credentials.
  raw_ptr<webauthn::PasskeyModel> _passkeyModel;
}

- (instancetype)initWithWindow:(UIWindow*)window
       savedPasswordsPresenter:
           (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter
         securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets
                  passkeyModel:(webauthn::PasskeyModel*)passkeyModel {
  CHECK(window);
  CHECK(savedPasswordsPresenter);
  CHECK(passkeyModel);

  self = [super init];
  if (self) {
    _window = window;
    _credentialExportManager = [[CredentialExportManager alloc] init];
    _savedPasswordsPresenter = savedPasswordsPresenter;
    _securityDomainSecrets = securityDomainSecrets;
    _passkeyModel = passkeyModel;
  }
  return self;
}

// TODO(crbug.com/449859205): Add a unit test for this method.
- (void)startExport API_AVAILABLE(ios(26.0)) {
  std::vector<password_manager::CredentialUIEntry> credentials =
      _savedPasswordsPresenter->GetSavedPasswords();

  NSMutableArray<CredentialExchangePassword*>* exportedPasswords =
      [NSMutableArray arrayWithCapacity:credentials.size()];

  for (const password_manager::CredentialUIEntry& credential : credentials) {
    NSString* username = base::SysUTF16ToNSString(credential.username) ?: @"";
    NSString* password = base::SysUTF16ToNSString(credential.password) ?: @"";
    NSString* note = base::SysUTF16ToNSString(credential.note) ?: @"";
    NSURL* URL =
        net::NSURLWithGURL(credential.GetURL()) ?: [NSURL URLWithString:@""];

    CredentialExchangePassword* exportedPassword =
        [[CredentialExchangePassword alloc] initWithURL:URL
                                               username:username
                                               password:password
                                                   note:note];
    [exportedPasswords addObject:exportedPassword];
  }

  NSMutableArray<CredentialExchangePasskey*>* exportedPasskeys =
      [NSMutableArray array];

  for (const sync_pb::WebauthnCredentialSpecifics& passkey :
       _passkeyModel->GetUnShadowedPasskeys()) {
    if (passkey.hidden()) {
      continue;
    }

    // TODO(crbug.com/449150840): Get the decrypted private key.
    NSData* encryptedBlob = nil;
    if (!passkey.private_key().empty()) {
      encryptedBlob = [NSData dataWithBytes:passkey.private_key().data()
                                     length:passkey.private_key().size()];
    } else if (!passkey.encrypted().empty()) {
      encryptedBlob = [NSData dataWithBytes:passkey.encrypted().data()
                                     length:passkey.encrypted().size()];
    } else {
      continue;
    }

    NSData* credentialId =
        [NSData dataWithBytes:passkey.credential_id().data()
                       length:passkey.credential_id().size()];
    NSString* rpId = base::SysUTF8ToNSString(passkey.rp_id());
    NSData* userId = [NSData dataWithBytes:passkey.user_id().data()
                                    length:passkey.user_id().size()];
    NSString* userName = base::SysUTF8ToNSString(passkey.user_name());
    NSString* userDisplayName =
        base::SysUTF8ToNSString(passkey.user_display_name());

    CredentialExchangePasskey* exportedPasskey =
        [[CredentialExchangePasskey alloc] initWithCredentialId:credentialId
                                                           rpId:rpId
                                                       userName:userName
                                                userDisplayName:userDisplayName
                                                         userId:userId
                                                     privateKey:encryptedBlob];
    [exportedPasskeys addObject:exportedPasskey];
  }

  [_credentialExportManager startExportWithPasswords:exportedPasswords
                                            passkeys:exportedPasskeys
                                              window:_window];
}

@end
