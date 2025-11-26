// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_exporter.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_passkey.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_password.h"
#import "ios/chrome/browser/credential_exchange/model/credential_export_manager_swift.h"
#import "net/base/apple/url_conversions.h"

@implementation CredentialExporter {
  // Used as a presentation anchor for OS views. Must not be nil.
  UIWindow* _window;

  // Exports credentials through the OS ASCredentialExportManager API.
  CredentialExportManager* _credentialExportManager;
}

- (instancetype)initWithWindow:(UIWindow*)window {
  CHECK(window);

  self = [super init];
  if (self) {
    _window = window;
    _credentialExportManager = [[CredentialExportManager alloc] init];
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
           securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets
    API_AVAILABLE(ios(26.0)) {
  NSArray<CredentialExchangePassword*>* exportedPasswords =
      [self transformPasswords:std::move(passwords)];
  NSArray<CredentialExchangePasskey*>* exportedPasskeys =
      [self transformPasskeys:std::move(passkeys)
                 usingSecrets:securityDomainSecrets];

  [_credentialExportManager startExportWithPasswords:exportedPasswords
                                            passkeys:exportedPasskeys
                                              window:_window];
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
         usingSecrets:(NSArray<NSData*>*)secrets {
  NSMutableArray<CredentialExchangePasskey*>* exportedPasskeys =
      [NSMutableArray arrayWithCapacity:passkeys.size()];

  for (const sync_pb::WebauthnCredentialSpecifics& passkey : passkeys) {
    NSData* privateKey = [self decryptPrivateKeyForPasskey:passkey
                                              usingSecrets:secrets];

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

// Attempts to decrypt the private key for a given passkey using the available
// security domain secrets.
- (NSData*)decryptPrivateKeyForPasskey:
               (const sync_pb::WebauthnCredentialSpecifics&)passkey
                          usingSecrets:(NSArray<NSData*>*)secrets {
  sync_pb::WebauthnCredentialSpecifics_Encrypted decrypted_data;
  for (NSData* securityDomainSecret in secrets) {
    if (webauthn::passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
            base::apple::NSDataToSpan(securityDomainSecret), passkey,
            &decrypted_data)) {
      return [NSData dataWithBytes:decrypted_data.private_key().data()
                            length:decrypted_data.private_key().size()];
    }
  }
  return nil;
}

@end
