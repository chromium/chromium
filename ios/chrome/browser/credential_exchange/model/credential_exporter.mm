// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_exporter.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_passkey.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_password.h"
#import "ios/chrome/browser/credential_exchange/model/credential_export_manager_swift.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

@interface CredentialExporter () <CredentialExportManagerDelegate>
@end

@implementation CredentialExporter {
  // Used as a presentation anchor for OS views. Must not be nil.
  UIWindow* _window;

  // Exports credentials through the OS ASCredentialExportManager API.
  CredentialExportManager* _credentialExportManager;

  // Delegate for CredentialImporter.
  id<CredentialExporterDelegate> _delegate;
}

- (instancetype)initWithWindow:(UIWindow*)window
                      delegate:(id<CredentialExporterDelegate>)delegate {
  CHECK(window);

  self = [super init];
  if (self) {
    _window = window;
    _delegate = delegate;
    _credentialExportManager = [[CredentialExportManager alloc] init];
    _credentialExportManager.delegate = self;
  }
  return self;
}

#pragma mark - Public

// TODO(crbug.com/449859205): Add a unit test for this method.
- (void)startExportWithPasswords:
            (std::vector<password_manager::CredentialUIEntry>)passwords
                        passkeys:
                            (std::vector<sync_pb::WebauthnCredentialSpecifics>)
                                passkeys
                trustedVaultKeys:(webauthn::SharedKeyList)trustedVaultKeys
                       userEmail:(NSString*)userEmail API_AVAILABLE(ios(26.0)) {
  NSArray<CredentialExchangePassword*>* exportedPasswords =
      [self transformPasswords:std::move(passwords)];
  NSArray<CredentialExchangePasskey*>* exportedPasskeys =
      [self transformPasskeys:std::move(passkeys)
          usingTrustedVaultKeys:std::move(trustedVaultKeys)];

  [_credentialExportManager
      startExportWithPasswords:exportedPasswords
                      passkeys:exportedPasskeys
                        window:_window
                     userEmail:userEmail
                  exporterName:
                      l10n_util::GetNSString(
                          IDS_IOS_CREDENTIAL_EXCHANGE_EXPORTER_DISPLAY_NAME)];
}

#pragma mark - Private

// Returns an array of passwords formatted for the credential export API.
- (NSArray<CredentialExchangePassword*>*)transformPasswords:
    (std::vector<password_manager::CredentialUIEntry>)passwords {
  NSMutableArray<CredentialExchangePassword*>* exportedPasswords =
      [NSMutableArray arrayWithCapacity:passwords.size()];

  for (const password_manager::CredentialUIEntry& credential : passwords) {
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
  return exportedPasswords;
}

// Returns an array of passkeys formatted for the credential export API.
- (NSArray<CredentialExchangePasskey*>*)
        transformPasskeys:
            (std::vector<sync_pb::WebauthnCredentialSpecifics>)passkeys
    usingTrustedVaultKeys:(webauthn::SharedKeyList)trustedVaultKeys {
  NSMutableArray<CredentialExchangePasskey*>* exportedPasskeys =
      [NSMutableArray arrayWithCapacity:passkeys.size()];

  for (const sync_pb::WebauthnCredentialSpecifics& passkey : passkeys) {
    NSData* privateKey = [self decryptPrivateKeyForPasskey:passkey
                                     usingTrustedVaultKeys:trustedVaultKeys];

    if (!privateKey) {
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
                                                     privateKey:privateKey];
    [exportedPasskeys addObject:exportedPasskey];
  }
  return exportedPasskeys;
}

// Attempts to decrypt the private key for a given `passkey` with
// `trustedVaultKeys`.
- (NSData*)decryptPrivateKeyForPasskey:
               (const sync_pb::WebauthnCredentialSpecifics&)passkey
                 usingTrustedVaultKeys:
                     (const webauthn::SharedKeyList&)trustedVaultKeys {
  sync_pb::WebauthnCredentialSpecifics_Encrypted decrypted;
  for (const webauthn::SharedKey& trustedVaultKey : trustedVaultKeys) {
    if (webauthn::passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
            trustedVaultKey, passkey, &decrypted)) {
      return [NSData dataWithBytes:decrypted.private_key().data()
                            length:decrypted.private_key().size()];
    }
  }
  return nil;
}

#pragma mark - CredentialExportManagerDelegate

- (void)onExportError {
  [_delegate onExportError];
}

@end
